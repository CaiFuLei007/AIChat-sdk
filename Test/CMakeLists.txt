# =================================================================
#                         基础配置
# =================================================================

cmake_minimum_required(VERSION 3.18)

project(
    AI_sdk
    VERSION 1.0.0
    LANGUAGES CXX
)

# 设置C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

 # 设置输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)

# 设置可执行程序构建类型
set(CMAKE_BUILD_TYPE "Debug")
# set(CMAKE_BUILD_TYPE "Release")

# 设置优化级别
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1")


# =================================================================
#                         生成可执行程序
# =================================================================

file(GLOB_RECURSE SRC_LISTS
    "../sdk/src/*.cc"
)

add_executable(all_test
    all_test.cc
    ${SRC_LISTS}
)


# =================================================================
#                         添加依赖
# =================================================================


include_directories(
    ${CMAKE_SOURCE_DIR}/../sdk/include
    ${CMAKE_SOURCE_DIR}/../aichat_server/src
)

find_package(GTest REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(httplib REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(fmt REQUIRED)

find_library(SQLITE3_LIB 
  NAMES libsqlite3.so
  PATHS /usr/lib/x86_64-linux-gnu/
  REQUIRED
)

find_package(CURL REQUIRED)

target_compile_definitions(all_test PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(
    all_test
    PRIVATE
    spdlog
    fmt::fmt
    GTest::gtest GTest::gtest_main
    jsoncpp
    httplib::httplib
    OpenSSL::SSL
    OpenSSL::Crypto
    ${SQLITE3_LIB}
    CURL::libcurl
)
