/*
* @license
* (C) zachbabanov
*
*/

#pragma once

#include <log/Logger.h>

#include <expected>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

namespace config::parser {
    using ParseResult = std::expected<rapidjson::Document, rapidjson::ParseErrorCode>;

    inline ParseResult Read(const std::string &filePath) noexcept {
        int fileDescriptor = open(filePath.c_str(), O_RDONLY);

        if (fileDescriptor < 0) {
            Logger::Instance().Error("Configuration file descriptor cannot be created");
            return std::unexpected(rapidjson::kParseErrorDocumentEmpty);
        }

        struct stat st{};

        if (fstat(fileDescriptor, &st) != 0) {
            Logger::Instance().Error("Error during getting configuration file attributes");
            close(fileDescriptor);
            return std::unexpected(rapidjson::kParseErrorDocumentEmpty);
        }

        if (st.st_size == 0) {
            Logger::Instance().Error("Configuration file is empty file");
            close(fileDescriptor);
            return std::unexpected(rapidjson::kParseErrorDocumentEmpty);
        }

        auto fileData = static_cast<char*>(mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fileDescriptor, 0));
        close(fileDescriptor);

        if (fileData == MAP_FAILED) {
            Logger::Instance().Error("Error during configuration file mapping in virtual memory");
            return std::unexpected(rapidjson::kParseErrorDocumentEmpty);
        }

        rapidjson::Document doc;
        doc.Parse(fileData, st.st_size);
        munmap(fileData, st.st_size);

        if (doc.HasParseError()) {
            Logger::Instance().Error(fmt::format("Configuration file parse from json failed: {}", rapidjson::GetParseError_En(doc.GetParseError())));
            return std::unexpected(doc.GetParseError());
        }

        return doc;
    }
}
