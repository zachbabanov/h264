/*
* @license
* (C) zachbabanov
*
*/

#pragma once

#include <log/Logger.h>

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/bsf.h>
}

namespace h264 {
    class StreamReader {
    public:
        StreamReader(const StreamReader&) = delete;
        StreamReader& operator=(const StreamReader&) = delete;
        StreamReader(StreamReader&&) = delete;
        StreamReader& operator=(StreamReader&&) = delete;

        static StreamReader& Instance() {
            static StreamReader streamReaderInstance;
            return streamReaderInstance;
        }

        int ReadTo(std::vector<uint8_t>& buffer, uint32_t& outBlockIndex, uint32_t& outNaluIndex, uint32_t& outNaluBlockSize) {
            std::lock_guard<std::recursive_mutex> lock(_mutex);

            if (_currentNALU.empty() || _currentNaluOffset >= _currentNALU.size()) {
                if (!ExtractNextNalu()) {
                    return 0;
                }
            }

            size_t naluSize = _currentNALU.size();
            auto naluBlockSize = static_cast<uint32_t>((naluSize + blockSize - 1) / blockSize);
            auto blockIndex = static_cast<uint32_t>(_currentNaluOffset / blockSize);
            size_t bytesToCopy = std::min(blockSize, naluSize - _currentNaluOffset);

            buffer.resize(bytesToCopy);
            std::memcpy(buffer.data(), _currentNALU.data() + _currentNaluOffset, bytesToCopy);

            outBlockIndex = blockIndex;
            outNaluIndex = _currentNaluIndex;
            outNaluBlockSize = naluBlockSize;
            _currentNaluOffset += bytesToCopy;

            if (_currentNaluOffset >= _currentNALU.size()) {
                _currentNALU.clear();
                _currentNaluOffset = 0;
                _currentNaluIndex++;
            }

            return static_cast<int>(bytesToCopy);
        }

        void Open(const std::string& url) {
            std::lock_guard<std::recursive_mutex> lock(_mutex);

            if (_formatContext) {
                Close();
            }

            if (avformat_open_input(&_formatContext, url.c_str(), nullptr, nullptr) < 0) {
                Logger::Instance().Error(fmt::format("Failed to open FFMpeg avformat stream by provided url: {}", url));
                Close();
                exit(1);
            }

            if (avformat_find_stream_info(_formatContext, nullptr) < 0) {
                Logger::Instance().Error(fmt::format("Failed to find FFMpeg avformat stream info by provided url: {}", url));
                Close();
                exit(1);
            }

            bool isRawH264 = (strcmp(_formatContext->iformat->name, "h264") == 0);

            for (unsigned int streamId = 0; streamId < _formatContext->nb_streams; ++streamId) {
                if (_formatContext->streams[streamId]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
                    _formatContext->streams[streamId]->codecpar->codec_id == AV_CODEC_ID_H264) {
                    _videoStreamId = static_cast<int>(streamId);
                    break;
                }
            }

            if (_videoStreamId == -1) {
                Logger::Instance().Error(fmt::format("No h264 video stream found by provided url: {}", url));
                Close();
                exit(1);
            }

            if (isRawH264) {
                Logger::Instance().Info("Raw H.264 input detected, skipping bitstream filter");
                _bitStreamFilterContext = nullptr;
            } else {
                AVCodecParameters* avCodecParameters = _formatContext->streams[_videoStreamId]->codecpar;
                const AVBitStreamFilter* bitStreamFilter = av_bsf_get_by_name("h264_mp4toannexb");

                if (!bitStreamFilter) {
                    Logger::Instance().Error(fmt::format("Bitstream filter 'h264_mp4toannexb' not found with provided url: {}", url));
                    Close();
                    exit(1);
                }

                if (av_bsf_alloc(bitStreamFilter, &_bitStreamFilterContext) < 0) {
                    Logger::Instance().Error(fmt::format("Failed to allocate FFMpeg bit stream filter context with provided url: {}", url));
                    Close();
                    exit(1);
                }

                if (avcodec_parameters_copy(_bitStreamFilterContext->par_in, avCodecParameters) < 0) {
                    Logger::Instance().Error(fmt::format("Failed to copy FFMpeg codec parameters into incoming FFMpeg bit stream filter context with provided url: {}", url));
                    Close();
                    exit(1);
                }

                if (av_bsf_init(_bitStreamFilterContext) < 0) {
                    Logger::Instance().Error(fmt::format("Failed to initialize FFMpeg bit stream filter context with provided url: {}", url));
                    Close();
                    exit(1);
                }
            }

            _streamBuffer.clear();
            _streamBufferOffset = 0;
            _currentNALU.clear();
            _currentNaluOffset = 0;
            _currentNaluIndex = 0;
        }

        void Close() {
            std::lock_guard<std::recursive_mutex> lock(_mutex);

            if (_bitStreamFilterContext) {
                av_bsf_free(&_bitStreamFilterContext);
                _bitStreamFilterContext = nullptr;
            }

            if (_formatContext) {
                avformat_close_input(&_formatContext);
                _formatContext = nullptr;
            }

            if (_avPacket) {
                av_packet_unref(_avPacket);
            }

            _videoStreamId = -1;
            _videoStreamPosition = 0;
            _streamBuffer.clear();
            _streamBufferOffset = 0;
            _currentNALU.clear();
            _currentNaluOffset = 0;
            _currentNaluIndex = 0;
        }

    private:
        StreamReader() {
            if (avformat_network_init() < 0) {
                Logger::Instance().Error("Failed to initialize AVFormat network");
                exit(1);
            }

            _avPacket = av_packet_alloc();

            if (!_avPacket) {
                Logger::Instance().Error("Failed to allocate AVPacket");
                exit(1);
            }
        }

        ~StreamReader() {
            Close();
            av_packet_free(&_avPacket);
            avformat_network_deinit();
        }

        bool ExtractNextNalu() {
            while (true) {
                size_t startPos = FindStartCode(_streamBuffer, _streamBufferOffset);
                if (startPos != std::string::npos) {
                    size_t endPos = FindStartCode(_streamBuffer, startPos + 3);
                    if (endPos == std::string::npos) {
                        endPos = _streamBuffer.size();
                    }

                    _currentNALU.assign(_streamBuffer.begin() + static_cast<long>(startPos),
                                        _streamBuffer.begin() + static_cast<long>(endPos));

                    _streamBufferOffset = endPos;
                    _currentNaluOffset = 0;

                    return true;
                } else {
                    if (!FillStreamBuffer()) {
                        return false;
                    }
                }
            }
        }

        bool FillStreamBuffer() {
            int ret = ReadPacketFromStream();
            if (ret <= 0) {
                return false;
            }

            if (_avPacket->size > 0 && _avPacket->data) {
                _streamBuffer.insert(_streamBuffer.end(), _avPacket->data, _avPacket->data + _avPacket->size);
            }

            av_packet_unref(_avPacket);
            return true;
        }

        static size_t FindStartCode(const std::vector<uint8_t>& buf, size_t offset) {
            for (size_t i = offset; i + 2 < buf.size(); ++i) {
                if (i + 3 < buf.size() && buf[i] == 0x00 && buf[i+1] == 0x00 && buf[i+2] == 0x00 && buf[i+3] == 0x01) {
                    return i;
                }

                if (buf[i] == 0x00 && buf[i+1] == 0x00 && buf[i+2] == 0x01) {
                    return i;
                }
            }

            return std::string::npos;
        }

        int ReadPacketFromStream() {
            if (_bitStreamFilterContext) {
                bool sentNullTerminator = false;

                while (true) {
                    auto receivedPacket = av_bsf_receive_packet(_bitStreamFilterContext, _avPacket);

                    switch (receivedPacket) {
                        case 0: {
                            if (_avPacket->size <= 0 || !_avPacket->data) {
                                av_packet_unref(_avPacket);
                                continue;
                            }

                            _videoStreamPosition = 0;
                            return 1;
                        }
                        case AVERROR(EAGAIN): {
                            if (sentNullTerminator) {
                                Logger::Instance().Error("Bitstream filter returned EAGAIN after null packet sent");
                                return -1;
                            }

                            AVPacket* tempAvPacket = av_packet_alloc();

                            if (!tempAvPacket) {
                                Logger::Instance().Error("Failed to allocate temporary AVPacket for reading frames");
                                return -1;
                            }

                            int ret = av_read_frame(_formatContext, tempAvPacket);

                            if (ret < 0) {
                                av_packet_free(&tempAvPacket);

                                if (ret == AVERROR_EOF) {
                                    ret = av_bsf_send_packet(_bitStreamFilterContext, nullptr);

                                    if (ret < 0) {
                                        Logger::Instance().Error("Failed to send null packet to bitstream filter during flush");
                                        return -1;
                                    }

                                    sentNullTerminator = true;
                                    continue;
                                } else {
                                    Logger::Instance().Error("FFMpeg error while reading frame from demuxer");
                                    return -1;
                                }
                            }

                            if (tempAvPacket->stream_index == _videoStreamId) {
                                ret = av_bsf_send_packet(_bitStreamFilterContext, tempAvPacket);
                                av_packet_free(&tempAvPacket);

                                if (ret < 0) {
                                    Logger::Instance().Error("FFMpeg bit stream filter failed to accept packet");
                                    return -1;
                                }

                                continue;
                            } else {
                                av_packet_free(&tempAvPacket);
                                continue;
                            }
                        }
                        case AVERROR_EOF: {
                            av_packet_unref(_avPacket);
                            return 0;
                        }
                        default: {
                            Logger::Instance().Error(fmt::format("Unknown FFMpeg error during bit stream filter packet receive: {}", receivedPacket));
                            av_packet_unref(_avPacket);
                            return -1;
                        }
                    }
                }
            } else {
                AVPacket* tempAvPacket = av_packet_alloc();

                if (!tempAvPacket) {
                    Logger::Instance().Error("Failed to allocate temporary AVPacket");
                    return -1;
                }

                while (true) {
                    int readResult = av_read_frame(_formatContext, tempAvPacket);

                    if (readResult < 0) {
                        av_packet_free(&tempAvPacket);

                        if (readResult == AVERROR_EOF) {
                            return 0;
                        } else {
                            Logger::Instance().Error("Error reading frame from demuxer");
                            return -1;
                        }
                    }

                    if (tempAvPacket->stream_index == _videoStreamId) {
                        av_packet_unref(_avPacket);
                        av_packet_move_ref(_avPacket, tempAvPacket);
                        av_packet_free(&tempAvPacket);
                        _videoStreamPosition = 0;
                        return 1;
                    } else {
                        av_packet_unref(tempAvPacket);
                    }
                }
            }
        }

        AVPacket* _avPacket{nullptr};
        AVFormatContext* _formatContext{nullptr};
        AVBSFContext* _bitStreamFilterContext{nullptr};

        int _videoStreamPosition{0};
        int _videoStreamId{-1};

        std::recursive_mutex _mutex;

        std::vector<uint8_t> _streamBuffer;
        size_t _streamBufferOffset{0};

        std::vector<uint8_t> _currentNALU;
        size_t _currentNaluOffset{0};
        uint32_t _currentNaluIndex{0};
    };
}
