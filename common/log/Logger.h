/*
* @license
* (C) zachbabanov
*
*/

#pragma once

#include <config/Fields.h>

#include <source_location>
#include <iostream>
#include <fstream>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <mutex>

#include <fmt/chrono.h>
#include <fmt/core.h>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    static Logger& Instance() {
        static Logger loggerInstance;
        return loggerInstance;
    }

    void SetLevel(LogLevel level) {
        _level.store(level, std::memory_order_release);
    }

    void SetFilePath(const std::string &path) {
        std::ofstream newFile(path, std::ios::trunc);

        if (!newFile.is_open()) {
            throw std::runtime_error("Log file cannot be opened/created: " + path);
        }

        {
            std::lock_guard<std::mutex> lock(_mutex);
            _file.swap(newFile);
            _filePath = path;
        }
    }

    template<typename... Args>
    void Debug(fmt::format_string<Args...> fmt_str, Args&&... args,
               const std::source_location& loc = std::source_location::current()) {
        log(LogLevel::Debug, fmt::format(fmt_str, std::forward<Args>(args)...), loc);
    }

    template<typename... Args>
    void Info(fmt::format_string<Args...> fmt_str, Args&&... args,
              const std::source_location& loc = std::source_location::current()) {
        log(LogLevel::Info, fmt::format(fmt_str, std::forward<Args>(args)...), loc);
    }

    template<typename... Args>
    void Warn(fmt::format_string<Args...> fmt_str, Args&&... args,
              const std::source_location& loc = std::source_location::current()) {
        log(LogLevel::Warning, fmt::format(fmt_str, std::forward<Args>(args)...), loc);
    }

    template<typename... Args>
    void Error(fmt::format_string<Args...> fmt_str, Args&&... args,
               const std::source_location& loc = std::source_location::current()) {
        log(LogLevel::Error, fmt::format(fmt_str, std::forward<Args>(args)...), loc);
    }

    void Debug(const std::string& message, const std::source_location& loc = std::source_location::current()) {
        log(LogLevel::Debug, message, loc);
    }

    void Info(const std::string& message, const std::source_location& loc = std::source_location::current()) {
        log(LogLevel::Info, message, loc);
    }

    void Warn(const std::string& message, const std::source_location& loc = std::source_location::current()) {
        log(LogLevel::Warning, message, loc);
    }

    void Error(const std::string& message, const std::source_location& loc = std::source_location::current()) {
        log(LogLevel::Error, message, loc);
    }

private:
    Logger() = default;

    ~Logger() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_file.is_open()) {
            _file.close();
        }
    }

    void log(LogLevel level, const std::string& formatted_message, const std::source_location& loc) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (level < _level.load(std::memory_order_acquire)) {
            return;
        }

        std::string log_entry = fmt::format("[{}] [{}] {}:{} – {}",
                                            getCurrentTime(),
                                            getLevel(level),
                                            loc.file_name(),
                                            loc.line(),
                                            formatted_message);

        if (!_file.is_open()) {
            _file.open(_filePath, std::ios::trunc);

            if (!_file.is_open()) {
                std::cerr << "Logger error: cannot open log file: " << _filePath
                          << ", message: " << log_entry << std::endl;
                return;
            }
        }

        try {
            _file << log_entry << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Logger exception: " << e.what() << ", message: " << log_entry << std::endl;
        }
    }

    static std::string_view getLevel(LogLevel level) noexcept {
        using namespace std::string_view_literals;
        switch (level) {
            case LogLevel::Debug:   return "DEBUG";
            case LogLevel::Info:    return "INFO";
            case LogLevel::Warning: return "WARNING";
            case LogLevel::Error:   return "ERROR";
            default:                return "UNKNOWN";
        }
    }

    static std::string getCurrentTime() noexcept {
        auto now = std::chrono::system_clock::now();
        return fmt::format("{:%Y-%m-%d %H:%M:%S}", now);
    }

    std::string _filePath{config::fields::DEFAULT_LOG_PATH};
    std::ofstream _file;
    std::mutex _mutex;
    std::atomic<LogLevel> _level{LogLevel::Debug};
};
