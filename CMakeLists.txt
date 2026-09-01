# =================================================================
#                         基础配置
# =================================================================

cmake_minimum_required(VERSION 3.18)

project(
    aichat_sdk
    VERSION 1.0.0
    LANGUAGES CXX
)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成动态库需要位置无关代码
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 构建类型默认 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# 设置输出路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 安装路径 (GNUInstallDirs 提供: INCLUDE/ LIB/ BIN/ ...)
include(GNUInstallDirs)
set(CMAKE_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
set(CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})

# =================================================================
#                    FetchContent 自动下载依赖
#   说明 : 仅 SDK 依赖 (httplib/jsoncpp/spdlog/fmt)
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
    # 禁止 httplib 生成 install 目标, 避免第三方头文件污染系统 include
    set(HTTPLIB_INSTALL OFF CACHE BOOL "" FORCE)
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
    # 但源码中使用的是 <json/json.h>，需要添加兼容路径
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

# =================================================================
#                    OpenSSL / CURL / SQLite3 (系统库)
# =================================================================

find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL 已找到，版本: ${OPENSSL_VERSION}")

find_package(CURL REQUIRED)
message(STATUS "CURL 已找到，版本: ${CURL_VERSION_STRING}")

find_package(SQLite3 REQUIRED)
message(STATUS "SQLite3 已找到，版本: ${SQLite3_VERSION}")

# =================================================================
#                         生成动态库
#   说明 : 将 sdk/src 下所有 .cc 编译为 libaichat_sdk.so
# =================================================================

file(GLOB_RECURSE SDK_SRC
    "${CMAKE_SOURCE_DIR}/sdk/src/*.cc"
)

add_library(aichat_sdk SHARED ${SDK_SRC})

# 头文件搜索路径
#   PUBLIC   : 安装后使用者也需要 (sdk/include)
#   PRIVATE  : 仅编译期需要 (jsoncpp 兼容路径)
target_include_directories(aichat_sdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/sdk/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/aichat_sdk>
    PRIVATE
        ${JSONCPP_INCLUDE_DIRS}
)

# 链接依赖 :
#   PUBLIC  : SQLite3 公共头文件 data_manager.h 包含 <sqlite3.h>, 使用方需要传递
#             头文件搜索路径与链接目标, 避免使用方出现 sqlite3 符号未定义
#   PRIVATE : httplib/jsoncpp/spdlog/fmt/OpenSSL/CURL 仅实现内部使用, 公共头文件
#             不暴露其类型, 链接进动态库后无需向使用方传递
target_link_libraries(aichat_sdk
    PUBLIC
        SQLite::SQLite3
    PRIVATE
        ${httplib_target}
        ${jsoncpp_target}
        ${spdlog_target}
        ${fmt_target}
        OpenSSL::SSL
        OpenSSL::Crypto
        CURL::libcurl
)

# httplib 需要 OpenSSL 支持
target_compile_definitions(aichat_sdk PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

# 设置动态库版本号 (libaichat_sdk.so.1.0.0)
set_target_properties(aichat_sdk PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# =================================================================
#                         安装
#   1) 安装头文件 (sdk/include -> ${CMAKE_INSTALL_INCLUDEDIR}/aichat_sdk)
#   2) 安装动态库 (libaichat_sdk.so -> ${CMAKE_INSTALL_LIBDIR}/aichat_sdk)
#   说明: 仅安装 SDK 的头文件和动态库, 不安装可执行程序;
#         aichat_server 与 Test 目录各自独立构建, 不参与安装。
# =================================================================

# 安装动态库到独立的 aichat_sdk 子目录 (默认 /usr/local/lib/aichat_sdk)
# EXPORT : 将 aichat_sdk 目标记入 aichat_sdkTargets 导出集合, 供 find_package 使用
install(TARGETS aichat_sdk
    EXPORT aichat_sdkTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/aichat_sdk
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/aichat_sdk
)

# 安装头文件到独立的 aichat_sdk 子目录 (默认 /usr/local/include/aichat_sdk)
# 保留 sdk/include 下的目录结构 (aichat_sdk.h / base/ / provider/ ...)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/sdk/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aichat_sdk
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
)

# =================================================================
#                    包配置导出 (find_package(aichat_sdk))
#   生成 aichat_sdkConfig.cmake / aichat_sdkConfigVersion.cmake 与
#   aichat_sdkTargets.cmake, 使用方 find_package(aichat_sdk) 后链接
#   aichat_sdk::aichat_sdk 即可自动获得头文件搜索路径与 SQLite3 传递依赖
# =================================================================

include(CMakePackageConfigHelpers)

# 生成包配置文件(填充版本与安装路径占位符)
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/aichat_sdkConfig.cmake.in
    ${CMAKE_BINARY_DIR}/cmake/aichat_sdkConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aichat_sdk
)

# 生成版本兼容文件(主版本号相同即兼容)
write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/cmake/aichat_sdkConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# 导出目标文件(记录动态库的安装位置与 INTERFACE 属性)
install(EXPORT aichat_sdkTargets
    FILE aichat_sdkTargets.cmake
    NAMESPACE aichat_sdk::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aichat_sdk
)

# 安装包配置与版本文件
install(FILES
    ${CMAKE_BINARY_DIR}/cmake/aichat_sdkConfig.cmake
    ${CMAKE_BINARY_DIR}/cmake/aichat_sdkConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aichat_sdk
)

# =================================================================
#                         卸载目标
#   用法 : cmake --build <build_dir> --target uninstall
#   原理 : 读取 install_manifest.txt 中记录的已安装文件并删除,
#          并清理空的 aichat_sdk 子目录。
# =================================================================
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND}
            -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
            -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
            -DCMAKE_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/uninstall.cmake
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "卸载 aichat_sdk (根据 install_manifest.txt 删除已安装文件)"
)

# =================================================================
#                         说明
#   aichat_server 与 Test 目录各自有独立的 CMakeLists.txt,
#   可直接进入对应目录执行 cmake .. 编译 (它们内置 FetchContent 依赖)。
#   本根 CMakeLists 仅负责 SDK 动态库的构建与安装, 避免重复编译源文件。
# =================================================================
