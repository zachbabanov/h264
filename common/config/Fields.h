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
    static constexpr std::string_view DEFAULT_STAT_PATH{"log/statistics.log"};

    static constexpr std::string_view LOG_SECTION{"log"};
    static constexpr std::string_view SERVER_SECTION{"server"};
    static constexpr std::string_view GENERAL_SECTION{"general"};
    static constexpr std::string_view PACKET_SECTION{"packet"};
    static constexpr std::string_view PLAYER_SECTION{"player"};
    static constexpr std::string_view STAT_SECTION{"statistics"};

    static constexpr std::string_view IP{"ip"};
    static constexpr std::string_view PORT{"port"};

    static constexpr std::string_view ENCODING_MODE{"encoded"};
    static constexpr std::string_view PACKETS_AMOUNT{"amount"};
    static constexpr std::string_view PACKET_GAP{"gap"};

    static constexpr std::string_view STREAM_SOURCE{"source"};

    static constexpr std::string_view SOCKET_SIZE{"rxBufferSize"};
    static constexpr std::string_view BUFFER_INTERVAL{"bufferInterval"};
    static constexpr std::string_view FRAME_ASSEMBLY_TIMEOUT{"frameTimeout"};

    static constexpr std::string_view PATH{"path"};
    static constexpr std::string_view LEVEL{"level"};

    static constexpr std::string_view COUNTERS{"counters"};
    static constexpr std::string_view ACCEPTED_PACKETS{"acceptedPackets"};
    static constexpr std::string_view ACCEPTED_BLOCKS{"acceptedBlocks"};
    static constexpr std::string_view EXPECTED_BLOCKS{"expectedBlocks"};
    static constexpr std::string_view ASSEMBLED_NALU{"assembledNalu"};
    static constexpr std::string_view SKIPPED_NALU{"skippedNalu"};
}
