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

        if (auto bytesSent = send(_udpSocket, payloadBuffer, payloadLength, 0); bytesSent < payloadLength) {
            Logger::Instance().Error(fmt::format("Send failed: sent {} bytes, expected {}", bytesSent, payloadLength));
            return false;
        }

        return true;
    }

    bool Send(packet_t &&packet) const {
        if (!_sendAddress.sin_port) {
            Logger::Instance().Error("Trying to send message from <client> socket (this type of socket is listen only)");
            return false;
        }

        if (auto bytesSent = send(_udpSocket, &packet, sizeof(packet), 0); bytesSent < sizeof(packet)) {
            Logger::Instance().Error(fmt::format("Send failed: sent {} bytes, expected {}", bytesSent, sizeof(packet)));
            return false;
        }

        return true;
    }

    bool Send(rs_packet_t &&packet) const {
        if (!_sendAddress.sin_port) {
            Logger::Instance().Error("Trying to send message from <client> socket (this type of socket is listen only)");
            return false;
        }

        if (auto bytesSent = send(_udpSocket, &packet, sizeof(packet), 0); bytesSent < sizeof(packet)) {
            Logger::Instance().Error(fmt::format("Send failed: sent {} bytes, expected {}", bytesSent, sizeof(packet)));
            return false;
        }

        return true;
    }

    bool SendNonBlocking(const char *payloadBuffer, size_t payloadLength) const {
        if (!_sendAddress.sin_port) {
            Logger::Instance().Error("Trying to send message from <client> socket (this type of socket is listen only)");
            return false;
        }

        if (auto bytesSent = send(_udpSocket, payloadBuffer, payloadLength, MSG_DONTWAIT); bytesSent < payloadLength) {
            if (bytesSent == -1) {
                Logger::Instance().Error(fmt::format("Send failed, error: {}", errno));
                return false;
            }

            Logger::Instance().Error(fmt::format("Send failed: sent {} bytes, expected {}", bytesSent, payloadLength));
            return false;
        }

        return true;
    }

    bool SendNonBlocking(packet_t &&packet) const {
        if (!_sendAddress.sin_port) {
            Logger::Instance().Error("Trying to send message from <client> socket (this type of socket is listen only)");
            return false;
        }

        if (auto bytesSent = send(_udpSocket, &packet, sizeof(packet), MSG_DONTWAIT); bytesSent < sizeof(packet)) {
            if (bytesSent == -1) {
                Logger::Instance().Error(fmt::format("Send failed, error: {}", errno));
                return false;
            }

            Logger::Instance().Error(fmt::format("Send failed: sent {} bytes, expected {}", bytesSent, sizeof(packet)));
            return false;
        }

        return true;
    }

    bool SendNonBlocking(rs_packet_t &&packet) const {
        if (!_sendAddress.sin_port) {
            Logger::Instance().Error("Trying to send message from <client> socket (this type of socket is listen only)");
            return false;
        }

        if (auto bytesSent = send(_udpSocket, &packet, sizeof(packet), MSG_DONTWAIT); bytesSent < sizeof(packet)) {
            if (bytesSent == -1) {
                Logger::Instance().Error(fmt::format("Send failed, error: {}", errno));
                return false;
            }

            Logger::Instance().Error(fmt::format("Send failed: sent {} bytes, expected {}", bytesSent, sizeof(packet)));
            return false;
        }

        return true;
    }

    bool SendToNonBlocking(const char *payloadBuffer, size_t payloadLength, sockaddr_in &address) const {
        socklen_t addressLength = sizeof(address);

        if (auto bytesSent = sendto(_udpSocket, payloadBuffer, payloadLength, MSG_DONTWAIT,
                                    reinterpret_cast<struct sockaddr*>(&address), addressLength); bytesSent < payloadLength) {
            if (bytesSent == -1) {
                Logger::Instance().Error(fmt::format("Send failed, error: {}", errno));
                return false;
            }

            Logger::Instance().Error(fmt::format("Send failed: sent {} bytes, expected {}", bytesSent, payloadLength));
            return false;
        }

        return true;
    }

    bool SendToNonBlocking(packet_t &&packet, sockaddr_in &address) const {
        socklen_t addressLength = sizeof(address);

        if (auto bytesSent = sendto(_udpSocket, &packet, sizeof(packet), MSG_DONTWAIT,
                                    reinterpret_cast<struct sockaddr*>(&address), addressLength); bytesSent < sizeof(packet)) {
            if (bytesSent == -1) {
                Logger::Instance().Error(fmt::format("Send failed, error: {}", errno));
                return false;
            }

            Logger::Instance().Error(fmt::format("Send failed: sent {} bytes, expected {}", bytesSent, sizeof(packet)));
            return false;
        }

        return true;
    }

    bool SendToNonBlocking(rs_packet_t &&packet, sockaddr_in &address) const {
        socklen_t addressLength = sizeof(address);

        if (auto bytesSent = sendto(_udpSocket, &packet, sizeof(packet), MSG_DONTWAIT,
                                  reinterpret_cast<struct sockaddr*>(&address), addressLength); bytesSent < sizeof(packet)) {
            if (bytesSent == -1) {
                Logger::Instance().Error(fmt::format("Send failed, error: {}", errno));
                return false;
            }

            Logger::Instance().Error(fmt::format("Send failed: sent {} bytes, expected {}", bytesSent, sizeof(packet)));
            return false;
        }

        return true;
    }

    bool Receive(char* receivedMessage, size_t receivedSize) const {
        return recv(_udpSocket, receivedMessage, receivedSize, 0);
    }

    [[nodiscard]] std::optional<std::variant<rs_packet_t, packet_t>> Receive() const {
        uint8_t receivedMessage[blockSize + headerSize];

        auto receivedSize = recv(_udpSocket, receivedMessage, sizeof(receivedMessage), 0);

        if (receivedSize < 0) {
            Logger::Instance().Error(fmt::format("Error while receiving message on <server> socket: {}", errno));
            return std::nullopt;
        }

        return decomposePacket(receivedMessage, receivedSize);
    }

    bool ReceiveNonBlocking(char* receivedMessage, size_t receivedSize) const {
        auto receiveResult = recv(_udpSocket, receivedMessage, receivedSize, MSG_DONTWAIT);

        if (receiveResult < 0) {
            if (errno == EAGAIN) {
                Logger::Instance().Debug("Empty receive buffer for non-blocking receive on <server> socket");
                return false;
            }

            Logger::Instance().Error(fmt::format("Error while receiving message on <server> socket: {}", errno));
            return false;
        }

        return true;
    }

    [[nodiscard]] std::optional<std::variant<rs_packet_t, packet_t>> ReceiveNonBlocking() const {
        uint8_t receivedMessage[blockSize + headerSize];

        auto receivedSize = recv(_udpSocket, receivedMessage, sizeof(receivedMessage), MSG_DONTWAIT);

        if (receivedSize < 0) {
            if (errno == EAGAIN) {
                Logger::Instance().Debug("Empty receive buffer for non-blocking receive on <server> socket");
                return std::nullopt;
            }

            Logger::Instance().Error(fmt::format("Error while receiving message on <server> socket: {}", errno));
            return std::nullopt;
        }

        return decomposePacket(receivedMessage, receivedSize);
    }

    bool ReceiveFromNonBlocking(char* receivedMessage, size_t receivedSize, sockaddr_in &address) const {
        socklen_t addressLength = sizeof(address);
        auto receiveResult = recvfrom(_udpSocket, receivedMessage, receivedSize, MSG_DONTWAIT,
                                      reinterpret_cast<struct sockaddr*>(&address), &addressLength);

        if (receiveResult < 0) {
            if (errno == EAGAIN) {
                Logger::Instance().Debug("Empty receive buffer for non-blocking receive on <server> socket");
                return false;
            }

            Logger::Instance().Error(fmt::format("Error while receiving message on <server> socket: {}", errno));
            return false;
        }

        return true;
    }

    [[nodiscard]] std::optional<std::variant<rs_packet_t, packet_t>> ReceiveFromNonBlocking(sockaddr_in &address) const {
        uint8_t receivedMessage[blockSize + headerSize];
        socklen_t addressLength = sizeof(address);

        auto receivedSize = recvfrom(_udpSocket, receivedMessage, sizeof(receivedMessage), MSG_DONTWAIT,
                                 reinterpret_cast<struct sockaddr*>(&address), &addressLength);

        if (receivedSize < 0) {
            if (errno == EAGAIN) {
                Logger::Instance().Debug("Empty receive buffer for non-blocking receive on <server> socket");
                return std::nullopt;
            }

            Logger::Instance().Error(fmt::format("Error while receiving message on <server> socket: {}", errno));
            return std::nullopt;
        }

        return decomposePacket(receivedMessage, receivedSize);
    }

private:
    int _udpSocket{-1};
    struct sockaddr_in _ownAddress{};
    struct sockaddr_in _sendAddress{};
};
