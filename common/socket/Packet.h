/*
* @license
* (C) zachbabanov
*
*/

#pragma once

#include <optional>
#include <variant>
#include <cstdint>
#include <cstring>

#include <netinet/in.h>

/*   The structure of the header is assumed to be as follows:
 *    0                    1                   2                   3                   4
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |     ENCODE MODE     |                        BLOCK INDEX
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |     BLOCK INDEX     |   PACKET INDEX    |                PAYLOAD                |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *    ENCODE MODE is boolean and in case its value is false, all other fields consider absent,
 *    so header size is only 1 byte, instead of 6
 */

static constexpr size_t blockSize = 1024;
static constexpr size_t fieldSize = blockSize >> 3;

#pragma pack(push, 1)
typedef struct {
    uint8_t encodeMode;
    uint32_t blockIndex;
    uint8_t packetIndex;
} header_t;

typedef struct {
    header_t header;
    uint8_t payload[fieldSize];
} rs_packet_t;

typedef struct {
    uint8_t encodeMode;
    uint8_t payload[blockSize];
} packet_t;
#pragma pack(pop)

static constexpr size_t headerSize = sizeof(header_t);
static constexpr size_t payloadOffset = offsetof(packet_t, payload);
static constexpr size_t blockIndexOffset = offsetof(header_t, blockIndex);
static constexpr size_t packetIndexOffset = offsetof(header_t, packetIndex);

inline std::optional<packet_t> composePacket(const uint8_t *payloadBuffer, uint16_t payloadLength) noexcept {
    if (payloadLength > blockSize || !payloadBuffer) {
        return std::nullopt;
    }

    packet_t packet = {};

    packet.encodeMode = 0;
    memcpy(packet.payload, payloadBuffer, payloadLength);

    return packet;
}

inline std::optional<rs_packet_t> composePacket(uint32_t blockIndex, uint8_t packetIndex,
                                         const uint8_t *payloadBuffer, uint16_t payloadLength) noexcept {
    if (payloadLength > fieldSize || !payloadBuffer) {
        return std::nullopt;
    }

    rs_packet_t packet = {};

    packet.header.encodeMode = 1;
    packet.header.blockIndex = htonl(blockIndex);
    packet.header.packetIndex = packetIndex;
    memcpy(packet.payload, payloadBuffer, payloadLength);

    return packet;
}

inline std::optional<std::variant<rs_packet_t, packet_t>> decomposePacket(const uint8_t *payloadBuffer, uint16_t payloadLength) noexcept {
    if (payloadLength < 2 || !payloadBuffer) {
        return std::nullopt;
    }

    auto encodeMode = static_cast<uint8_t>(*payloadBuffer);

    switch (encodeMode) {
        case 0: {
            if (payloadLength - payloadOffset > blockSize) {
                return std::nullopt;
            }

            packet_t packet = {};
            memset(&packet, 0, sizeof(packet));
            packet.encodeMode = encodeMode;
            memcpy(packet.payload, payloadBuffer + payloadOffset, payloadLength - payloadOffset);

            return packet;
        }
        case 1: {
            if (payloadLength - headerSize > fieldSize || payloadLength < headerSize) {
                return std::nullopt;
            }

            rs_packet_t packet = {};
            memset(&packet, 0, sizeof(packet));
            packet.header.encodeMode = encodeMode;

            uint32_t blockIndex;
            memcpy(&blockIndex, payloadBuffer + blockIndexOffset, sizeof(blockIndex));
            packet.header.blockIndex = ntohl(blockIndex);

            packet.header.packetIndex = static_cast<uint8_t>(*(payloadBuffer + packetIndexOffset));
            memcpy(packet.payload, payloadBuffer + headerSize, payloadLength - headerSize);

            return packet;
        }
        default: {
            return std::nullopt;
        }
    }
}
