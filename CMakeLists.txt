cmake_minimum_required(VERSION 3.27)
project(h264)

set(CMAKE_CXX_STANDARD 23)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/third-party/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/common/")

find_package(TBB REQUIRED)
find_package(SDL2 REQUIRED)

find_package(PkgConfig REQUIRED)

pkg_check_modules(AVFORMAT REQUIRED libavformat IMPORTED_TARGET)
pkg_check_modules(AVCODEC  REQUIRED libavcodec  IMPORTED_TARGET)
pkg_check_modules(AVUTIL   REQUIRED libavutil   IMPORTED_TARGET)
pkg_check_modules(SWSCALE  REQUIRED libswscale  IMPORTED_TARGET)

include(FetchContent)
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        11.1.2
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
        rscoder
        GIT_REPOSITORY https://github.com/zachbabanov/rscoder.git
        GIT_TAG master
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/rscoder
)
FetchContent_MakeAvailable(rscoder)

file(GLOB_RECURSE COMMON LIST_DIRECTORIES true "common/*")
file(GLOB_RECURSE CLIENT_SOURCES LIST_DIRECTORIES true "client/*")
file(GLOB_RECURSE SERVER_SOURCES LIST_DIRECTORIES true "server/*")

add_executable(client ${COMMON} ${CLIENT_SOURCES})
add_executable(server ${COMMON} ${SERVER_SOURCES})

target_link_libraries(client
        PkgConfig::AVFORMAT
        PkgConfig::AVCODEC
        PkgConfig::AVUTIL
        PkgConfig::SWSCALE
        SDL2::SDL2
        TBB::tbb
        fmt::fmt
        rscoder
)

target_link_libraries(server
        PkgConfig::AVFORMAT
        PkgConfig::AVCODEC
        PkgConfig::AVUTIL
        PkgConfig::SWSCALE
        SDL2::SDL2
        TBB::tbb
        fmt::fmt
        rscoder
)
