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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1")

# =================================================================
#                    FetchContent 自动下载依赖
# =================================================================

include(FetchContent)

# ---------- httplib (HTTP 库) ----------
find_package(httplib QUIET)
if(NOT httplib_FOUND)
    message(STATUS "httplib 未找到，自动下载...")
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        v0.15.3
    )
    FetchContent_MakeAvailable(httplib)
    set(httplib_target httplib::httplib)
else()
    set(httplib_target httplib::httplib)
endif()

# ---------- jsoncpp (JSON 解析) ----------
find_package(jsoncpp QUIET)
if(NOT jsoncpp_FOUND)
    message(STATUS "jsoncpp 未找到，自动下载...")
    FetchContent_Declare(
        jsoncpp
        GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
        GIT_TAG        1.9.5
    )
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(jsoncpp)
    set(jsoncpp_target jsoncpp_static)
else()
    set(jsoncpp_target jsoncpp)
endif()

# ---------- spdlog (日志库) ----------
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    message(STATUS "spdlog 未找到，自动下载...")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.13.0
    )
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
    set(spdlog_target spdlog::spdlog)
else()
    set(spdlog_target spdlog::spdlog)
endif()

# ---------- fmt (格式化库) ----------
find_package(fmt QUIET)
if(NOT fmt_FOUND)
    message(STATUS "fmt 未找到，自动下载...")
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        10.2.1
    )
    set(FMT_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(fmt)
    set(fmt_target fmt::fmt)
else()
    set(fmt_target fmt::fmt)
endif()

# ---------- gflags (命令行参数解析) ----------
find_package(gflags QUIET)
if(NOT gflags_FOUND)
    message(STATUS "gflags 未找到，自动下载...")
    FetchContent_Declare(
        gflags
        GIT_REPOSITORY https://github.com/gflags/gflags.git
        GIT_TAG        v2.2.2
    )
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(gflags)
    set(gflags_target gflags)
else()
    set(gflags_target gflags)
endif()

# =================================================================
#                    OpenSSL / CURL / SQLite3 (系统库)
# =================================================================

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

find_library(SQLITE3_LIB
    NAMES libsqlite3.so
    PATHS /usr/lib/x86_64-linux-gnu/
    REQUIRED
)

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

target_compile_definitions(aichat_server PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(
    aichat_server
    PRIVATE
    ${httplib_target}
    ${jsoncpp_target}
    ${spdlog_target}
    ${fmt_target}
    ${gflags_target}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${SQLITE3_LIB}
    CURL::libcurl
)
