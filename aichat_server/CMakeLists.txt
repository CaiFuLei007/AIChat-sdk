# =================================================================
#                         基础配置
# =================================================================

cmake_minimum_required(VERSION 3.18)

project(
    aichat_server
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1")

# =================================================================
#                         生成可执行程序
# =================================================================

file(GLOB_RECURSE SDK_SRC
    "../sdk/src/*.cc"
)

add_executable(aichat_server
    src/main.cc
    src/aichat_server.cc
    ${SDK_SRC}
)

# =================================================================
#                         添加依赖
# =================================================================

include_directories(
    ${CMAKE_SOURCE_DIR}/../sdk/include
    ${CMAKE_SOURCE_DIR}/src
)

find_package(jsoncpp REQUIRED)
find_package(httplib REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(fmt REQUIRED)
find_package(CURL REQUIRED)

find_library(SQLITE3_LIB
    NAMES libsqlite3.so
    PATHS /usr/lib/x86_64-linux-gnu/
    REQUIRED
)

target_compile_definitions(aichat_server PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(
    aichat_server
    PRIVATE
    spdlog
    fmt::fmt
    jsoncpp
    httplib::httplib
    OpenSSL::SSL
    OpenSSL::Crypto
    ${SQLITE3_LIB}
    CURL::libcurl
)
