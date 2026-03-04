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

static constexpr size_t blockSize = 1024;
static constexpr size_t fieldSize = blockSize >> 3;

#pragma pack(push, 1)
/*   The structure of the header is assumed to be as follows:
 *    0                    1                   2                   3                   4
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |     ENCODE MODE     |   PACKET INDEX    |              PAYLOAD SIZE             |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                                   BLOCK INDEX                                   |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                                NALU BLOCK INDEX                                 |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                                 NALU BLOCK SIZE                                 |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                                     PAYLOAD                                     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *    ENCODE MODE is boolean and in case its value is false, all other fields consider absent,
 *    so header size is only 1 byte, instead of 6
 */

typedef struct {
    uint8_t encodeMode;
    uint8_t packetIndex;
    uint16_t payloadSize;
    uint32_t blockIndex;
    uint32_t naluIndex;
    uint32_t naluSize;
} header_t;

typedef struct {
    header_t header;
    uint8_t payload[fieldSize];
} rs_packet_t;

typedef struct {
    header_t header;
    uint8_t payload[blockSize];
} packet_t;
#pragma pack(pop)

static constexpr size_t headerSize = sizeof(header_t);
static constexpr size_t packetIndexOffset = offsetof(header_t, packetIndex);
static constexpr size_t payloadSizeOffset = offsetof(header_t, payloadSize);
static constexpr size_t blockIndexOffset = offsetof(header_t, blockIndex);
static constexpr size_t naluIndexOffset = offsetof(header_t, naluIndex);
static constexpr size_t naluSizeOffset = offsetof(header_t, naluSize);
static constexpr size_t payloadOffset = offsetof(packet_t, payload);

inline std::optional<packet_t> composePacket(uint32_t blockIndex, uint32_t naluIndex, uint32_t naluSize,
                                             const uint8_t *payloadBuffer, uint16_t payloadLength) noexcept {
    if (payloadLength > blockSize || !payloadBuffer) {
        return std::nullopt;
    }

    packet_t packet = {};

    packet.header.encodeMode = 0;
    packet.header.packetIndex = 0;
    packet.header.payloadSize = htons(payloadLength);
    packet.header.blockIndex = htonl(blockIndex);
    packet.header.naluIndex = htonl(naluIndex);
    packet.header.naluSize = htonl(naluSize);
    memcpy(packet.payload, payloadBuffer, payloadLength);

    return packet;
}

inline std::optional<rs_packet_t> composePacket(uint32_t blockIndex, uint8_t packetIndex, uint16_t blockLength,
                                                uint32_t naluIndex, uint32_t naluSize,
                                                const uint8_t *payloadBuffer, uint16_t payloadLength) noexcept {
    if (payloadLength > fieldSize || !payloadBuffer) {
        return std::nullopt;
    }

    rs_packet_t packet = {};

    packet.header.encodeMode = 1;
    packet.header.packetIndex = packetIndex;
    packet.header.payloadSize = htons(blockLength);
    packet.header.blockIndex = htonl(blockIndex);
    packet.header.naluIndex = htonl(naluIndex);
    packet.header.naluSize = htonl(naluSize);
    memcpy(packet.payload, payloadBuffer, fieldSize);

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
            packet.header.encodeMode = encodeMode;
            packet.header.packetIndex = static_cast<uint8_t>(*(payloadBuffer + packetIndexOffset));

            uint16_t payloadSize;
            memcpy(&payloadSize, payloadBuffer + payloadSizeOffset, sizeof(payloadSize));
            packet.header.payloadSize = ntohs(payloadSize);

            if (packet.header.payloadSize > blockSize) {
                return std::nullopt;
            }

            uint32_t blockIndex;
            memcpy(&blockIndex, payloadBuffer + blockIndexOffset, sizeof(blockIndex));
            packet.header.blockIndex = ntohl(blockIndex);

            uint32_t naluIndex;
            memcpy(&naluIndex, payloadBuffer + naluIndexOffset, sizeof(naluIndex));
            packet.header.naluIndex = ntohl(naluIndex);

            uint32_t naluSize;
            memcpy(&naluSize, payloadBuffer + naluSizeOffset, sizeof(naluSize));
            packet.header.naluSize = ntohl(naluSize);

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
            packet.header.packetIndex = static_cast<uint8_t>(*(payloadBuffer + packetIndexOffset));

            uint16_t payloadSize;
            memcpy(&payloadSize, payloadBuffer + payloadSizeOffset, sizeof(payloadSize));
            packet.header.payloadSize = ntohs(payloadSize);

            uint32_t blockIndex;
            memcpy(&blockIndex, payloadBuffer + blockIndexOffset, sizeof(blockIndex));
            packet.header.blockIndex = ntohl(blockIndex);

            uint32_t naluIndex;
            memcpy(&naluIndex, payloadBuffer + naluIndexOffset, sizeof(naluIndex));
            packet.header.naluIndex = ntohl(naluIndex);

            uint32_t naluSize;
            memcpy(&naluSize, payloadBuffer + naluSizeOffset, sizeof(naluSize));
            packet.header.naluSize = ntohl(naluSize);

            memcpy(packet.payload, payloadBuffer + headerSize, payloadLength - headerSize);

            return packet;
        }
        default: {
            return std::nullopt;
        }
    }
}
