/*
 * @license
 * (C) zachbabanov
 *
 */

#pragma once

#include <log/Logger.h>

#include <condition_variable>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <deque>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
}

namespace h264 {
    class Player {
    public:
        Player() {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
                Logger::Instance().Error(fmt::format("SDL initialization failed: {}", SDL_GetError()));
                exit(1);
            }

            if (!InitDecoder()) {
                Logger::Instance().Error("Failed to initialize FFmpeg decoder");
                SDL_Quit();
                exit(1);
            }
        }

        ~Player() {
            Stop();
            Cleanup();
            SDL_Quit();
        }

        Player(const Player&) = delete;
        Player& operator=(const Player&) = delete;
        Player(Player&&) = delete;
        Player& operator=(Player&&) = delete;

        void AddData(std::vector<uint8_t> data) {
            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                _queue.insert(_queue.end(), data.begin(), data.end());
            }

            _queueCondVar.notify_one();
        }

        void Start() {
            if (!_playerThread.joinable()) {
                _stop = false;
                _playerThread = std::jthread(&Player::Run, this);
            }
        }

        void Stop() {
            if (_playerThread.joinable()) {
                _stop = true;
                _queueCondVar.notify_all();
                _playerThread.request_stop();
                _playerThread.join();
            }
        }

        bool IsFinished() const {
            return _stop.load();
        }

    private:
        void Run() {
            if (!CreateWindowAndRenderer()) {
                Logger::Instance().Error("Failed to create SDL window and renderer");
                return;
            }

            using namespace std::chrono_literals;
            constexpr auto tickInterval = 33ms;
            auto nextTick = std::chrono::steady_clock::now();

            while (!_stop && !_playerThread.get_stop_token().stop_requested()) {
                SDL_Event event;

                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT) {
                        _stop = true;
                        break;
                    }
                }

                {
                    std::unique_lock<std::mutex> lock(_queueMutex);
                    _queueCondVar.wait_until(lock, nextTick, [this] { return !_queue.empty() || _stop; });
                }

                if (_stop) {
                    break;
                }

                {
                    std::lock_guard<std::mutex> lock(_queueMutex);

                    if (!_queue.empty()) {
                        _parseBuffer.insert(_parseBuffer.end(), _queue.begin(), _queue.end());
                        _queue.clear();
                    }
                }

                bool needMoreData = false;

                while (!needMoreData && _parseOffset < _parseBuffer.size() && !_stop) {
                    const uint8_t* data = _parseBuffer.data() + _parseOffset;
                    int dataSize = static_cast<int>(_parseBuffer.size() - _parseOffset);

                    int used = av_parser_parse2(_parser, _codecContext,
                                                &_packet->data, &_packet->size,
                                                data, dataSize,
                                                AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

                    if (used < 0) {
                        Logger::Instance().Error("Error parsing H.264 data");
                        break;
                    }

                    _parseOffset += used;

                    if (_packet->size > 0) {
                        DecodeAndDisplay(_packet);
                        av_packet_unref(_packet);
                    } else {
                        needMoreData = true;
                    }
                }

                if (_parseOffset >= _parseBuffer.size()) {
                    _parseBuffer.clear();
                    _parseOffset = 0;
                } else if (_parseOffset > 0) {
                    size_t remaining = _parseBuffer.size() - _parseOffset;
                    std::memmove(_parseBuffer.data(), _parseBuffer.data() + _parseOffset, remaining);
                    _parseBuffer.resize(remaining);
                    _parseOffset = 0;
                }

                nextTick += tickInterval;
                std::this_thread::sleep_until(nextTick);
            }

            DestroyWindowAndRenderer();
        }

        bool InitDecoder() {
            const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);

            if (!codec) {
                Logger::Instance().Error("H.264 decoder not found");
                return false;
            }

            _codecContext = avcodec_alloc_context3(codec);

            if (!_codecContext) {
                Logger::Instance().Error("Failed to allocate codec context");
                return false;
            }

            _parser = av_parser_init(AV_CODEC_ID_H264);

            if (!_parser) {
                Logger::Instance().Error("Failed to create H.264 parser");
                return false;
            }

            _frame = av_frame_alloc();
            _packet = av_packet_alloc();

            if (!_frame || !_packet) {
                Logger::Instance().Error("Failed to allocate frame or packet");
                return false;
            }

            if (avcodec_open2(_codecContext, codec, nullptr) < 0) {
                Logger::Instance().Error("Failed to open codec");
                return false;
            }

            return true;
        }

        bool CreateWindowAndRenderer() {
            _window = SDL_CreateWindow("H.264 Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                       1280, 720, SDL_WINDOW_RESIZABLE);

            if (!_window) {
                Logger::Instance().Error(fmt::format("SDL window creation failed: {}", SDL_GetError()));
                return false;
            }

            _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

            if (!_renderer) {
                Logger::Instance().Error(fmt::format("SDL renderer creation failed: {}", SDL_GetError()));
                SDL_DestroyWindow(_window);
                _window = nullptr;
                return false;
            }

            return true;
        }

        void DestroyWindowAndRenderer() {
            if (_texture) {
                SDL_DestroyTexture(_texture);
                _texture = nullptr;
            }

            if (_renderer) {
                SDL_DestroyRenderer(_renderer);
                _renderer = nullptr;
            }

            if (_window) {
                SDL_DestroyWindow(_window);
                _window = nullptr;
            }
        }

        void Cleanup() {
            av_parser_close(_parser);
            avcodec_free_context(&_codecContext);
            av_frame_free(&_frame);
            av_packet_free(&_packet);
            sws_freeContext(_swsContext);
        }

        void DecodeAndDisplay(AVPacket* pkt) {
            int ret = avcodec_send_packet(_codecContext, pkt);

            if (ret < 0) {
                Logger::Instance().Error("Error sending packet to decoder");
                return;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(_codecContext, _frame);

                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    return;
                } else if (ret < 0) {
                    Logger::Instance().Error("Error during decoding");
                    return;
                }

                if (_width != _frame->width || _height != _frame->height || _pixFmt != _frame->format) {
                    _width = _frame->width;
                    _height = _frame->height;
                    _pixFmt = static_cast<AVPixelFormat>(_frame->format);

                    if (_texture) {
                        SDL_DestroyTexture(_texture);
                    }

                    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                                 _width, _height);

                    if (!_texture) {
                        Logger::Instance().Error(fmt::format("Failed to create SDL texture: {}", SDL_GetError()));
                        return;
                    }

                    _swsContext = sws_getCachedContext(_swsContext,
                                                       _width, _height, _pixFmt,
                                                       _width, _height, AV_PIX_FMT_YUV420P,
                                                       SWS_BILINEAR, nullptr, nullptr, nullptr);

                    SDL_SetWindowSize(_window, _width, _height);
                }

                RenderFrame(_frame);
            }
        }

        void RenderFrame(AVFrame* frame) {
            SDL_UpdateYUVTexture(_texture, nullptr,
                                 frame->data[0], frame->linesize[0],
                                 frame->data[1], frame->linesize[1],
                                 frame->data[2], frame->linesize[2]);

            SDL_RenderClear(_renderer);
            SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
            SDL_RenderPresent(_renderer);
        }

        std::deque<uint8_t> _queue;
        std::mutex _queueMutex;
        std::condition_variable _queueCondVar;

        std::jthread _playerThread;
        std::atomic<bool> _stop{false};

        AVCodecContext* _codecContext{nullptr};
        AVCodecParserContext* _parser{nullptr};
        AVFrame* _frame{nullptr};
        AVPacket* _packet{nullptr};
        SwsContext* _swsContext{nullptr};

        SDL_Window* _window{nullptr};
        SDL_Renderer* _renderer{nullptr};
        SDL_Texture* _texture{nullptr};

        int _width{0};
        int _height{0};
        AVPixelFormat _pixFmt{AV_PIX_FMT_NONE};

        std::vector<uint8_t> _parseBuffer;
        size_t _parseOffset{0};
    };
}
