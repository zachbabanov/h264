/*
* @license
* (C) zachbabanov
*
*/

#include <rscoder/Decimation.h>
#include <socket/Interface.h>
#include <config/Config.h>
#include <video/Player.h>
#include <log/Logger.h>

#include <condition_variable>
#include <variant>
#include <atomic>
#include <thread>
#include <deque>
#include <mutex>

using namespace server;

int main() {
    auto& config = Config::Instance();
    const uint16_t ownPort = config.OwnPort();
    const int rxBuferSize = config.ReceiveBufferSize();

    std::atomic<bool> running{true};
    SocketInterface socket(ownPort, rxBuferSize);
    Logger::Instance().Info("SocketInterface created");

    rscoder::Decimation::Decoder decoder;
    h264::Player player;
    player.Start();

    Statistics::Instance().Start();

    std::deque<std::variant<rs_packet_t, packet_t>> packetQueue;
    std::mutex packetQueueMutex;
    std::condition_variable packetQueueCondVar;

    std::thread receiverThread([&] {
        while (running) {
            auto received = socket.ReceiveNonBlocking();

            if (!received) {
                if (errno != EAGAIN) {
                    Logger::Instance().Error("Socket receive failed or timeout");
                }

                continue;
            }

            Statistics::Instance().Increment(config::fields::ACCEPTED_PACKETS.data());

            {
                std::lock_guard<std::mutex> lock(packetQueueMutex);
                packetQueue.push_back(std::move(*received));
            }

            packetQueueCondVar.notify_one();
        }
    });

    std::thread processorThread([&] {
        while (running) {
            std::variant<rs_packet_t, packet_t> packet;

            {
                std::unique_lock<std::mutex> lock(packetQueueMutex);

                packetQueueCondVar.wait(lock, [&] {
                    return !packetQueue.empty() || !running;
                });

                if (!running && packetQueue.empty()) {
                    break;
                }

                packet = std::move(packetQueue.front());
                packetQueue.pop_front();
            }

            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, packet_t>) {
                    uint32_t blockIndex = arg.header.blockIndex;
                    uint16_t payloadLength = arg.header.payloadSize;
                    uint32_t naluIndex = arg.header.naluIndex;
                    uint32_t naluBlockSize = arg.header.naluSize;
                    std::vector<uint8_t> data(arg.payload, arg.payload + payloadLength);

                    Logger::Instance().Debug(fmt::format("Received <raw packet> with blockIndex: {}, naluIndex: {}, "
                                                         "naluBlockSize: {}, payloadLength: {}. Added block to assemble in player",
                                                         blockIndex, naluIndex, naluBlockSize, payloadLength));

                    player.AddBlock(blockIndex, naluIndex, naluBlockSize, std::move(data));
                } else if constexpr (std::is_same_v<T, rs_packet_t>) {
                    uint8_t packetIndex = arg.header.packetIndex;
                    uint16_t payloadLength = arg.header.payloadSize;
                    uint32_t blockIndex = arg.header.blockIndex;
                    uint32_t naluIndex = arg.header.naluIndex;
                    uint32_t naluBlockSize = arg.header.naluSize;
                    std::vector<uint8_t> payload(arg.payload, arg.payload + fieldSize);

                    Logger::Instance().Debug(fmt::format("Received <rs packet> with blockIndex: {}, packetIndex: {}, naluIndex: {}, "
                                                         "naluBlockSize: {}, payloadLength: {}", blockIndex, packetIndex, naluIndex,
                                                         naluBlockSize, payloadLength));

                    uint32_t id = (naluIndex << 16) | blockIndex;
                    auto decodedBlock = decoder.Decode(id, packetIndex, std::move(payload));

                    if (decodedBlock) {
                        Logger::Instance().Info(fmt::format("Recovered block {} out of {} for nalu {}. Added block to assemble in player",
                                                            blockIndex, naluBlockSize - 1, naluIndex));

                        decodedBlock.value().resize(payloadLength);
                        player.AddBlock(blockIndex, naluIndex, naluBlockSize, std::move(*decodedBlock));
                    }
                }
            }, packet);
        }
    });

    receiverThread.join();
    processorThread.join();

    player.Stop();
    Logger::Instance().Info("Server shutting down");
    return 0;
}
