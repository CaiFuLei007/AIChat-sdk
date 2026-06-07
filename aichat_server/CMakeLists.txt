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
    message(STATUS "httplib 已通过 FetchContent 下载，目标: httplib::httplib")
else()
    set(httplib_target httplib::httplib)
    message(STATUS "httplib 已找到，目标: httplib::httplib")
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
    # FetchContent 下载的 jsoncpp 头文件路径是 <json/json.h>
    # 但源码中使用的是 <jsoncpp/json/json.h>，需要添加兼容路径
    set(JSONCPP_INCLUDE_DIRS ${jsoncpp_SOURCE_DIR}/include)
    message(STATUS "jsoncpp 已通过 FetchContent 下载，目标: jsoncpp_static")
else()
    # 系统安装的 jsoncpp 目标名称可能是 jsoncpp_lib 或 jsoncpp_static
    if(TARGET jsoncpp_lib)
        set(jsoncpp_target jsoncpp_lib)
    elseif(TARGET jsoncpp_static)
        set(jsoncpp_target jsoncpp_static)
    else()
        set(jsoncpp_target jsoncpp)
    endif()
    # 系统安装的 jsoncpp 头文件路径是 <jsoncpp/json/json.h>
    # 但源码中使用的是 <json/json.h>，需要添加兼容路径
    get_target_property(JSONCPP_INCLUDE_DIRS ${jsoncpp_target} INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "jsoncpp 已找到，目标: ${jsoncpp_target}")
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
    message(STATUS "spdlog 已通过 FetchContent 下载，目标: spdlog::spdlog")
else()
    set(spdlog_target spdlog::spdlog)
    message(STATUS "spdlog 已找到，目标: spdlog::spdlog")
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
    message(STATUS "fmt 已通过 FetchContent 下载，目标: fmt::fmt")
else()
    set(fmt_target fmt::fmt)
    message(STATUS "fmt 已找到，目标: fmt::fmt")
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
    message(STATUS "gflags 已通过 FetchContent 下载，目标: gflags")
else()
    set(gflags_target gflags)
    message(STATUS "gflags 已找到，目标: gflags")
endif()

# =================================================================
#                    OpenSSL / CURL / SQLite3 (系统库)
# =================================================================

find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL 已找到，版本: ${OPENSSL_VERSION}")

find_package(CURL REQUIRED)
message(STATUS "CURL 已找到，版本: ${CURL_VERSION_STRING}")

find_library(SQLITE3_LIB
    NAMES libsqlite3.so
    PATHS /usr/lib/x86_64-linux-gnu/
    REQUIRED
)
message(STATUS "SQLite3 已找到，库文件: ${SQLITE3_LIB}")

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
    ${JSONCPP_INCLUDE_DIRS}
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
