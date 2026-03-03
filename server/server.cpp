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
    Logger::Instance().Info("SocketInterface created (server)");

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
                uint32_t naluIndex = arg.header.naluIndex;
                uint32_t naluBlockSize = arg.header.naluSize;
                std::vector<uint8_t> data(arg.payload, arg.payload + blockSize);
                player.AddBlock(blockIndex, naluIndex, naluBlockSize, std::move(data));
                Logger::Instance().Debug("Received raw packet, added to player");
            } else if constexpr (std::is_same_v<T, rs_packet_t>) {
                uint32_t blockIndex = arg.header.blockIndex;
                uint8_t packetIndex = arg.header.packetIndex;
                uint32_t naluIndex = arg.header.naluIndex;
                uint32_t naluBlockSize = arg.header.naluSize;
                std::vector<uint8_t> payload(arg.payload, arg.payload + fieldSize);

                auto decodedBlock = decoder.Decode(blockIndex, packetIndex, std::move(payload));

                if (decodedBlock) {
                    player.AddBlock(blockIndex, naluIndex, naluBlockSize, std::move(*decodedBlock));
                    Logger::Instance().Info(fmt::format("Recovered block {} for nalu {}", blockIndex, naluIndex));
                }
            }
        }, *received);
    }

    player.Stop();
    Logger::Instance().Info("Server shutting down");
    return 0;
}
