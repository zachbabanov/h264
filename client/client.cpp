/*
* @license
* (C) zachbabanov
*
*/

#include <socket/Interface.h>
#include <rscoder/Encoder.h>
#include <config/Config.h>
#include <video/Stream.h>
#include <log/Logger.h>

#include <thread>
#include <atomic>
#include <vector>

using namespace client;

int main() {
    auto& config = Config::Instance();
    const bool encodingMode = config.EncodingMode();
    const std::string serverIp = config.ServerIp();
    const uint16_t serverPort = config.ServerPort();
    const uint16_t ownPort = config.OwnPort();
    const std::string streamSource = config.StreamSource();

    std::atomic<bool> running{true};

    SocketInterface socket(serverIp, serverPort, ownPort);
    Logger::Instance().Info("Socket created");

    rscoder::Encoder::Encoder encoder(8);
    Logger::Instance().Info("Encoder created with 8 cells");

    std::thread readerThread([&] {
        h264::StreamReader& reader = h264::StreamReader::Instance();
        reader.Open(streamSource);
        Logger::Instance().Info(fmt::format("StreamReader opened: {}", streamSource));

        std::vector<uint8_t> buffer(blockSize);
        uint32_t blockIndex = 0;
        uint32_t naluIndex = 0;
        uint32_t naluBlockSize = 0;

        while (running) {
            int bytesRead = reader.ReadTo(buffer, blockIndex, naluIndex, naluBlockSize);

            if (bytesRead < 0) {
                Logger::Instance().Error("StreamReader::ReadTo failed");
                running = false;
                break;
            }

            if (bytesRead == 0) {
                Logger::Instance().Info("End of stream reached");
                break;
            }

            if (!encodingMode) {
                auto packetOpt = composePacket(blockIndex, naluIndex, naluBlockSize,
                                               buffer.data(), static_cast<uint16_t>(bytesRead));

                if (!packetOpt) {
                    Logger::Instance().Error("composePacket failed (non-encoded)");
                    continue;
                }

                if (!socket.Send(std::move(*packetOpt))) {
                    Logger::Instance().Error("Socket send failed (non-encoded)");
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                auto encodedPackets = encoder.Encode(buffer);

                uint8_t packetIndex = 0;
                for (auto& packetPayload : encodedPackets) {
                    auto rsPacketOpt = composePacket(blockIndex, packetIndex, bytesRead, naluIndex, naluBlockSize,
                                                     packetPayload.data(), packetPayload.size());

                    if (!rsPacketOpt) {
                        Logger::Instance().Error(fmt::format("composePacket failed for packet index {}", packetIndex));
                        continue;
                    }

                    if (!socket.Send(std::move(*rsPacketOpt))) {
                        Logger::Instance().Error(fmt::format("Socket send failed for packet index {}", packetIndex));
                    }

                    ++packetIndex;
                    std::this_thread::sleep_for(std::chrono::microseconds(0));
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        reader.Close();
        Logger::Instance().Info("StreamReader closed");
    });

    if (!readerThread.joinable()) {
        Logger::Instance().Error("Error while waiting or reader thread");
        exit(1);
    }

    readerThread.join();

    Logger::Instance().Info("Client shutting down");
    return 0;
}
