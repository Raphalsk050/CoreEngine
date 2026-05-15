if(NOT DEFINED CORE_ENGINE_WINDOWS_UCRT_TARGET_DIR)
    message(FATAL_ERROR "CORE_ENGINE_WINDOWS_UCRT_TARGET_DIR was not provided")
endif()

if(NOT DEFINED CORE_ENGINE_WINDOWS_UCRT_CONFIG)
    set(CORE_ENGINE_WINDOWS_UCRT_CONFIG "")
endif()

set(CORE_ENGINE_WINDOWS_UCRT_CLEAN_PATTERNS
        "api-ms-win-core-*.dll"
        "api-ms-win-crt-*.dll"
        "ucrtbase.dll"
        "ucrtbased.dll"
)

foreach(clean_pattern IN LISTS CORE_ENGINE_WINDOWS_UCRT_CLEAN_PATTERNS)
    file(GLOB stale_ucrt_dlls "${CORE_ENGINE_WINDOWS_UCRT_TARGET_DIR}/${clean_pattern}")
    if(stale_ucrt_dlls)
        file(REMOVE ${stale_ucrt_dlls})
    endif()
endforeach()

if(CORE_ENGINE_WINDOWS_UCRT_CONFIG STREQUAL "Debug")
    set(CORE_ENGINE_WINDOWS_UCRT_DIR "${CORE_ENGINE_WINDOWS_UCRT_DEBUG_DIR}")
    set(CORE_ENGINE_WINDOWS_UCRT_KIND "Debug UCRT")
else()
    set(CORE_ENGINE_WINDOWS_UCRT_DIR "${CORE_ENGINE_WINDOWS_UCRT_RELEASE_DIR}")
    set(CORE_ENGINE_WINDOWS_UCRT_KIND "UCRT")
endif()

if(NOT EXISTS "${CORE_ENGINE_WINDOWS_UCRT_DIR}")
    message(WARNING "Windows ${CORE_ENGINE_WINDOWS_UCRT_KIND} directory was not found: ${CORE_ENGINE_WINDOWS_UCRT_DIR}")
    return()
endif()

file(GLOB CORE_ENGINE_WINDOWS_UCRT_DLLS "${CORE_ENGINE_WINDOWS_UCRT_DIR}/*.dll")
if(NOT CORE_ENGINE_WINDOWS_UCRT_DLLS)
    message(WARNING "No Windows ${CORE_ENGINE_WINDOWS_UCRT_KIND} DLLs were found in ${CORE_ENGINE_WINDOWS_UCRT_DIR}")
    return()
endif()

foreach(runtime_dll IN LISTS CORE_ENGINE_WINDOWS_UCRT_DLLS)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtime_dll}"
                "${CORE_ENGINE_WINDOWS_UCRT_TARGET_DIR}"
        RESULT_VARIABLE COPY_RESULT
    )
    if(NOT COPY_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to copy Windows UCRT DLL: ${runtime_dll}")
    endif()
endforeach()

message(STATUS "Copied Windows ${CORE_ENGINE_WINDOWS_UCRT_KIND} DLLs from ${CORE_ENGINE_WINDOWS_UCRT_DIR}")
