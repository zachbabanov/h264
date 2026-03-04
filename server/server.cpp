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

#include <variant>
#include <atomic>

using namespace server;

int main() {
    auto& config = Config::Instance();
    const uint16_t ownPort = config.OwnPort();

    std::atomic<bool> running{true};
    SocketInterface socket(ownPort);
    Logger::Instance().Info("SocketInterface created");

    rscoder::Decimation::Decoder decoder;
    h264::Player player;
    player.Start();

    while (running) {
        auto received = socket.Receive();

        if (!received) {
            Logger::Instance().Error("Socket receive failed or timeout");
            continue;
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
        }, *received);
    }

    player.Stop();
    Logger::Instance().Info("Server shutting down");
    return 0;
}
