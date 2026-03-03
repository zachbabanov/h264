/*
* @license
* (C) zachbabanov
*
*/

#pragma once

#include <config/Parser.h>
#include <config/Fields.h>
#include <log/Logger.h>

#include <shared_mutex>

namespace client {
    class Config {
    public:
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;
        Config(Config&&) = delete;
        Config& operator=(Config&&) = delete;

        static Config& Instance() {
            static Config confidDiInstance;
            return confidDiInstance;
        }

        [[nodiscard]] std::string ServerIp() const {
            std::shared_lock lock(_mutex);
            return _serverIp;
        }

        [[nodiscard]] uint16_t ServerPort() const {
            std::shared_lock lock(_mutex);
            return _serverPort;
        }

        [[nodiscard]] std::string LogPath() const {
            std::shared_lock lock(_mutex);
            return _logPath;
        }

        [[nodiscard]] uint8_t LogLevel() const {
            std::shared_lock lock(_mutex);
            return _logLevel;
        }

        [[nodiscard]] uint16_t OwnPort() const {
            std::shared_lock lock(_mutex);
            return _ownPort;
        }

        [[nodiscard]] std::string StreamSource() const {
            std::shared_lock lock(_mutex);
            return _streamSource;
        }

        [[nodiscard]] bool EncodingMode() const {
            std::shared_lock lock(_mutex);
            return _encodingMode;
        }

    private:
        Config() {
            auto jsonConfig = config::parser::Read(config::fields::CLIENT_CONFIG_PATH.data());

            if (!jsonConfig) {
                Logger::Instance().Error("Empty or missing config file client.json");
                exit(1);
            }

            if (!jsonConfig->IsObject() || jsonConfig->ObjectEmpty()) {
                Logger::Instance().Error("Config file client.json is not an object");
                exit(1);
            }

            if (!jsonConfig->HasMember(config::fields::GENERAL_SECTION.data())) {
                Logger::Instance().Error("Config file client.json missing mandatory section <general>");
                exit(1);
            }

            if (!jsonConfig->HasMember(config::fields::SERVER_SECTION.data())) {
                Logger::Instance().Error("Config file client.json missing mandatory section <server>");
                exit(1);
            }

            auto logSectionIt = jsonConfig->FindMember(config::fields::LOG_SECTION.data());

            if (logSectionIt != jsonConfig->MemberEnd()) {
                auto &logSectionVal = logSectionIt->value;

                if (auto path = logSectionVal.FindMember(config::fields::PATH.data());
                        path != logSectionVal.MemberEnd()) {
                    if (!path->value.IsString()) {
                        Logger::Instance().Error("Config file client.json optional section <log> optional field <path> has wrong type");
                        exit(1);
                    }

                    _logPath = path->value.GetString();
                    Logger::Instance().SetFilePath(_logPath);
                }

                if (auto level = logSectionVal.FindMember(config::fields::LEVEL.data());
                        level != logSectionVal.MemberEnd()) {
                    if (!level->value.IsUint()) {
                        Logger::Instance().Error("Config file client.json optional section <log> optional field <level> has wrong type");
                        exit(1);
                    }

                    _logLevel = level->value.GetUint();
                    Logger::Instance().SetLevel(static_cast<enum LogLevel>(_logLevel));
                }
            }

            Logger::Instance().Info("Finished reading config file client.json optional section <log>");
            Logger::Instance().Info(fmt::format("\t_logPath: {}", _logPath));
            Logger::Instance().Info(fmt::format("\t_logLevel: {}", _logLevel));

            auto serverSectionIt = jsonConfig->FindMember(config::fields::SERVER_SECTION.data());

            if (serverSectionIt != jsonConfig->MemberEnd()) {
                auto &serverSectionVal = serverSectionIt->value;

                if (!serverSectionVal.IsObject() || serverSectionVal.ObjectEmpty()) {
                    Logger::Instance().Error("Config file client.json mandatory section <server> is empty");
                    exit(1);
                }

                if (!serverSectionVal.HasMember(config::fields::IP.data())) {
                    Logger::Instance().Error("Config file client.json mandatory section <server> missing mandatory field <ip>");
                    exit(1);
                }

                if (!serverSectionVal.HasMember(config::fields::PORT.data())) {
                    Logger::Instance().Error("Config file client.json mandatory section <server> missing mandatory field <port>");
                    exit(1);
                }

                if (auto serverIp = serverSectionVal.FindMember(config::fields::IP.data());
                        serverIp != serverSectionVal.MemberEnd()) {
                    if (!serverIp->value.IsString()) {
                        Logger::Instance().Error("Config file client.json mandatory section <server> mandatory field <ip> has wrong type");
                        exit(1);
                    }

                    _serverIp = serverIp->value.GetString();
                }

                if (auto serverPort = serverSectionVal.FindMember(config::fields::PORT.data());
                        serverPort != serverSectionVal.MemberEnd()) {
                    if (!serverPort->value.IsUint()) {
                        Logger::Instance().Error("Config file client.json mandatory section <server> mandatory field <port> has wrong type");
                        exit(1);
                    }

                    _serverPort = serverPort->value.GetUint();
                }
            }

            Logger::Instance().Info("Finished reading config file client.json mandatory section <server>");
            Logger::Instance().Info(fmt::format("\t_serverIp: {}", _serverIp));
            Logger::Instance().Info(fmt::format("\t_serverPort: {}", _serverPort));

            auto generalSectionIt = jsonConfig->FindMember(config::fields::GENERAL_SECTION.data());

            if (generalSectionIt != jsonConfig->MemberEnd()) {
                auto &generalSectionVal = generalSectionIt->value;

                if (!generalSectionVal.IsObject() || generalSectionVal.ObjectEmpty()) {
                    Logger::Instance().Error("Config file client.json mandatory section <general> is empty");
                    exit(1);
                }

                if (!generalSectionVal.HasMember(config::fields::STREAM_SOURCE.data())) {
                    Logger::Instance().Error("Config file client.json mandatory section <general> missing mandatory field <source>");
                    exit(1);
                }

                if (auto ownPort = generalSectionVal.FindMember(config::fields::PORT.data());
                        ownPort != generalSectionVal.MemberEnd()) {
                    if (!ownPort->value.IsUint()) {
                        Logger::Instance().Error("Config file client.json mandatory section <general> optional field <port> has wrong type");
                        exit(1);
                    }

                    _ownPort = ownPort->value.GetUint();
                }

                if (auto source = generalSectionVal.FindMember(config::fields::STREAM_SOURCE.data());
                        source != generalSectionVal.MemberEnd()) {
                    if (!source->value.IsString()) {
                        Logger::Instance().Error("Config file client.json mandatory section <general> mandatory field <source> has wrong type");
                        exit(1);
                    }

                    _streamSource = source->value.GetString();
                }

                if (auto encodingMode = generalSectionVal.FindMember(config::fields::ENCODING_MODE.data());
                        encodingMode != generalSectionVal.MemberEnd()) {
                    if (!encodingMode->value.IsBool()) {
                        Logger::Instance().Error("Config file client.json mandatory section <general> optional field <encoding> has wrong type");
                        exit(1);
                    }

                    _encodingMode = encodingMode->value.GetBool();
                }
            }

            Logger::Instance().Info("Finished reading config file client.json mandatory section <general>");
            Logger::Instance().Info(fmt::format("\t_ownPort: {}", _ownPort));
            Logger::Instance().Info(fmt::format("\t_streamSource: {}", _streamSource));
            Logger::Instance().Info(fmt::format("\t_encodingMode: {}", _encodingMode));
        }

        ~Config() = default;

        std::string _serverIp;
        uint16_t _serverPort;
        uint16_t _ownPort{8000};

        std::string _streamSource;
        std::string _logPath{config::fields::DEFAULT_LOG_PATH};
        uint8_t _logLevel{0};
        bool _encodingMode{false};

        mutable std::shared_mutex _mutex;
    };
}

namespace server {
    class Config {
    public:
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;
        Config(Config&&) = delete;
        Config& operator=(Config&&) = delete;

        static Config& Instance() {
            static Config instance;
            return instance;
        }

        [[nodiscard]] std::string LogPath() const {
            std::shared_lock lock(_mutex);
            return _logPath;
        }

        [[nodiscard]] uint8_t LogLevel() const {
            std::shared_lock lock(_mutex);
            return _logLevel;
        }

        [[nodiscard]] uint16_t OwnPort() const {
            std::shared_lock lock(_mutex);
            return _ownPort;
        }

    private:
        Config() {
            auto jsonConfig = config::parser::Read(config::fields::SERVER_CONFIG_PATH.data());

            if (!jsonConfig) {
                Logger::Instance().Error("Empty or missing config file server.json");
                exit(1);
            }

            if (!jsonConfig->IsObject() || jsonConfig->ObjectEmpty()) {
                Logger::Instance().Error("Config file server.json is not an object");
                exit(1);
            }

            if (!jsonConfig->HasMember(config::fields::GENERAL_SECTION.data())) {
                Logger::Instance().Error("Config file server.json missing mandatory section <general>");
                exit(1);
            }

            auto logSectionIt = jsonConfig->FindMember(config::fields::LOG_SECTION.data());

            if (logSectionIt != jsonConfig->MemberEnd()) {
                auto &logSectionVal = logSectionIt->value;

                if (auto path = logSectionVal.FindMember(config::fields::PATH.data());
                        path != logSectionVal.MemberEnd()) {
                    if (!path->value.IsString()) {
                        Logger::Instance().Error("Config file server.json optional section <log> optional field <path> has wrong type");
                        exit(1);
                    }

                    _logPath = path->value.GetString();
                    Logger::Instance().SetFilePath(_logPath);
                }

                if (auto level = logSectionVal.FindMember(config::fields::LEVEL.data());
                        level != logSectionVal.MemberEnd()) {
                    if (!level->value.IsUint()) {
                        Logger::Instance().Error("Config file server.json optional section <log> optional field <level> has wrong type");
                        exit(1);
                    }

                    _logLevel = level->value.GetUint();
                    Logger::Instance().SetLevel(static_cast<enum LogLevel>(_logLevel));
                }
            }

            Logger::Instance().Info("Finished reading config file server.json optional section <log>");
            Logger::Instance().Info(fmt::format("\t_logPath: {}", _logPath));
            Logger::Instance().Info(fmt::format("\t_logLevel: {}", _logLevel));

            auto generalSectionIt = jsonConfig->FindMember(config::fields::GENERAL_SECTION.data());

            if (generalSectionIt != jsonConfig->MemberEnd()) {
                auto &generalSectionVal = generalSectionIt->value;

                if (!generalSectionVal.IsObject() || generalSectionVal.ObjectEmpty()) {
                    Logger::Instance().Error("Config file server.json mandatory section <general> is empty");
                    exit(1);
                }

                if (!generalSectionVal.HasMember(config::fields::PORT.data())) {
                    Logger::Instance().Error("Config file server.json mandatory section <general> missing mandatory field <port>");
                    exit(1);
                }

                if (auto ownPort = generalSectionVal.FindMember(config::fields::PORT.data());
                        ownPort != generalSectionVal.MemberEnd()) {
                    if (!ownPort->value.IsUint()) {
                        Logger::Instance().Error("Config file server.json mandatory section <general> mandatory field <port> has wrong type");
                        exit(1);
                    }

                    _ownPort = ownPort->value.GetUint();
                }
            }

            Logger::Instance().Info("Finished reading config file server.json mandatory section <general>");
            Logger::Instance().Info(fmt::format("\t_ownPort: {}", _ownPort));
        }

        ~Config() = default;

        uint16_t _ownPort{8000};

        std::string _logPath{config::fields::DEFAULT_LOG_PATH};
        uint8_t _logLevel{0};

        mutable std::shared_mutex _mutex;
    };
}
