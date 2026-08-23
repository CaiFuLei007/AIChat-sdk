# =================================================================
#                         卸载脚本
#   作用 : 读取 install_manifest.txt, 删除所有已安装的文件与空目录
#   调用 : cmake --build <build_dir> --target uninstall
# =================================================================

set(MANIFEST_FILE "${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt")

if(NOT EXISTS "${MANIFEST_FILE}")
    message(FATAL_ERROR "找不到 install_manifest.txt: ${MANIFEST_FILE}\n"
                        "请先执行 install 目标后再卸载。")
endif()

message(STATUS "读取安装清单: ${MANIFEST_FILE}")

file(READ "${MANIFEST_FILE}" MANIFEST_CONTENT)
string(REGEX REPLACE "\n" ";" FILES "${MANIFEST_CONTENT}")

set(REMOVED_COUNT 0)
set(MISSING_COUNT 0)

foreach(FILE ${FILES})
    if(EXISTS "${FILE}")
        message(STATUS "删除: ${FILE}")
        file(REMOVE "${FILE}")
        math(EXPR REMOVED_COUNT "${REMOVED_COUNT} + 1")
    else()
        message(STATUS "跳过 (不存在): ${FILE}")
        math(EXPR MISSING_COUNT "${MISSING_COUNT} + 1")
    endif()
endforeach()

# 清理因删除文件而变空的父目录 (仅限 aichat_sdk 子目录, 避免误删系统目录)
set(CANDIDATE_DIRS
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/aichat_sdk"
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/aichat_sdk"
)

foreach(DIR ${CANDIDATE_DIRS})
    if(IS_DIRECTORY "${DIR}")
        file(GLOB ENTRIES "${DIR}/*")
        if(NOT ENTRIES)
            message(STATUS "删除空目录: ${DIR}")
            file(REMOVE_RECURSE "${DIR}")
        endif()
    endif()
endforeach()

message(STATUS "卸载完成: 删除 ${REMOVED_COUNT} 个文件, 跳过 ${MISSING_COUNT} 个不存在的文件")
