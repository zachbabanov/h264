/*
* @license
* (C) zachbabanov
*
*/

#pragma once

#include <socket/Packet.h>
#include <log/Logger.h>

#include <arpa/inet.h>

class SocketInterface {
public :
    SocketInterface(std::string sendAddress, uint16_t sendPort, uint16_t ownPort = 8000) {
        _udpSocket = socket(AF_INET, SOCK_DGRAM, 0);

        if (_udpSocket < 0) {
            Logger::Instance().Error("Failed to create a udp socket");
        }

        memset(&_sendAddress, 0, sizeof(_sendAddress));
        _sendAddress.sin_family = AF_INET;
        _sendAddress.sin_port = htons(sendPort);

        if (inet_pton(AF_INET, sendAddress.c_str(), &_sendAddress.sin_addr) <= 0) {
            Logger::Instance().Error("Mandatory configuration parameter <ip> in section <general> is a non valid IP address");
            close(_udpSocket);
            exit(1);
        }

        memset(&_ownAddress, 0, sizeof(_ownAddress));
        _ownAddress.sin_family = AF_INET;
        _ownAddress.sin_port = htons(ownPort);
        _ownAddress.sin_addr.s_addr = INADDR_ANY;

        if (bind(_udpSocket, reinterpret_cast<struct sockaddr*>(&_ownAddress), sizeof(_ownAddress)) < 0) {
            Logger::Instance().Error("Failed to bind incoming connection to specified port");
            close(_udpSocket);
            exit(1);
        }

        if (connect(_udpSocket, reinterpret_cast<struct sockaddr*>(&_sendAddress), sizeof(_sendAddress)) < 0) {
            Logger::Instance().Error("Failed to bind outgoing connection to specified address/port");
            close(_udpSocket);
            exit(1);
        }
    }

    explicit SocketInterface(uint16_t ownPort = 8000) {
        _udpSocket = socket(AF_INET, SOCK_DGRAM, 0);

        if (_udpSocket < 0) {
            Logger::Instance().Error("Failed to create a udp socket");
        }

        memset(&_ownAddress, 0, sizeof(_ownAddress));
        _ownAddress.sin_family = AF_INET;
        _ownAddress.sin_port = htons(ownPort);
        _ownAddress.sin_addr.s_addr = INADDR_ANY;

        if (bind(_udpSocket, reinterpret_cast<struct sockaddr*>(&_ownAddress), sizeof(_ownAddress)) < 0) {
            Logger::Instance().Error("Failed to bind incoming connection to specified port");
            close(_udpSocket);
            exit(1);
        }
    }

    bool Send(const char *payloadBuffer, size_t payloadLength) const {
        if (!_sendAddress.sin_port) {
            Logger::Instance().Error("Trying to send message from <client> socket (this type of socket is listen only)");
            return false;
        }

        return send(_udpSocket, payloadBuffer, payloadLength, 0);
    }

    bool Send(packet_t &&packet) const {
        if (!_sendAddress.sin_port) {
            Logger::Instance().Error("Trying to send message from <client> socket (this type of socket is listen only)");
            return false;
        }

        return send(_udpSocket, &packet, sizeof(packet), 0);
    }

    bool Send(rs_packet_t &&packet) const {
        if (!_sendAddress.sin_port) {
            Logger::Instance().Error("Trying to send message from <client> socket (this type of socket is listen only)");
            return false;
        }

        return send(_udpSocket, &packet, sizeof(packet), 0);
    }

    bool Receive(char* receivedMessage, size_t receivedSize) const {
        return recv(_udpSocket, receivedMessage, receivedSize, 0);
    }

    [[nodiscard]] std::optional<std::variant<rs_packet_t, packet_t>> Receive() const {
        uint8_t receivedMessage[blockSize + headerSize];

        auto receivedSize = recv(_udpSocket, receivedMessage, sizeof(receivedMessage), 0);

        if (receivedSize < 0) {
            Logger::Instance().Error("Error while receiving message on <server> socket");
            return std::nullopt;
        }

        return decomposePacket(receivedMessage, receivedSize);
    }

private:
    int _udpSocket{-1};
    struct sockaddr_in _ownAddress{};
    struct sockaddr_in _sendAddress{};
};
