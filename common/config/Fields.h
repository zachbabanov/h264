/*
* @license
* (C) zachbabanov
*
*/

#pragma once

#include <string_view>

namespace config::fields {
    static constexpr std::string_view CLIENT_CONFIG_PATH{"config/client.json"};
    static constexpr std::string_view SERVER_CONFIG_PATH{"config/server.json"};

    static constexpr std::string_view DEFAULT_LOG_PATH{"log/trace.log"};

    static constexpr std::string_view SERVER_SECTION{"server"};
    static constexpr std::string_view GENERAL_SECTION{"general"};
    static constexpr std::string_view LOG_SECTION{"log"};

    static constexpr std::string_view IP{"ip"};
    static constexpr std::string_view PORT{"port"};

    static constexpr std::string_view ENCODING_MODE{"encoded"};

    static constexpr std::string_view STREAM_SOURCE{"source"};

    static constexpr std::string_view LEVEL{"level"};
    static constexpr std::string_view PATH{"path"};
}
