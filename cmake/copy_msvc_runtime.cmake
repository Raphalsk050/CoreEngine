if(NOT DEFINED CORE_ENGINE_MSVC_RUNTIME_TARGET_DIR)
    message(FATAL_ERROR "CORE_ENGINE_MSVC_RUNTIME_TARGET_DIR was not provided")
endif()

if(NOT DEFINED CORE_ENGINE_MSVC_RUNTIME_CONFIG)
    set(CORE_ENGINE_MSVC_RUNTIME_CONFIG "")
endif()

if(CORE_ENGINE_MSVC_RUNTIME_CONFIG STREQUAL "Debug")
    if(NOT CORE_ENGINE_COPY_DEBUG_MSVC_RUNTIME_DLLS)
        message(WARNING "MSVC Debug CRT DLLs were not copied. Use RelWithDebInfo/Release for portable packages, or enable CORE_ENGINE_COPY_DEBUG_MSVC_RUNTIME_DLLS for developer-machine handoff builds.")
        return()
    endif()
    set(CORE_ENGINE_MSVC_RUNTIME_DIR "${CORE_ENGINE_MSVC_DEBUG_RUNTIME_DIR}")
    set(CORE_ENGINE_MSVC_RUNTIME_KIND "Debug CRT")
else()
    set(CORE_ENGINE_MSVC_RUNTIME_DIR "${CORE_ENGINE_MSVC_RELEASE_RUNTIME_DIR}")
    set(CORE_ENGINE_MSVC_RUNTIME_KIND "CRT")
endif()

if(NOT EXISTS "${CORE_ENGINE_MSVC_RUNTIME_DIR}")
    message(WARNING "MSVC ${CORE_ENGINE_MSVC_RUNTIME_KIND} directory was not found: ${CORE_ENGINE_MSVC_RUNTIME_DIR}")
    return()
endif()

set(CORE_ENGINE_MSVC_RUNTIME_CLEAN_PATTERNS
        "concrt140*.dll"
        "msvcp140*.dll"
        "vccorlib140*.dll"
        "vcruntime140*.dll"
)

foreach(clean_pattern IN LISTS CORE_ENGINE_MSVC_RUNTIME_CLEAN_PATTERNS)
    file(GLOB stale_runtime_dlls "${CORE_ENGINE_MSVC_RUNTIME_TARGET_DIR}/${clean_pattern}")
    if(stale_runtime_dlls)
        file(REMOVE ${stale_runtime_dlls})
    endif()
endforeach()

file(GLOB CORE_ENGINE_MSVC_RUNTIME_DLLS "${CORE_ENGINE_MSVC_RUNTIME_DIR}/*.dll")
if(NOT CORE_ENGINE_MSVC_RUNTIME_DLLS)
    message(WARNING "No MSVC ${CORE_ENGINE_MSVC_RUNTIME_KIND} DLLs were found in ${CORE_ENGINE_MSVC_RUNTIME_DIR}")
    return()
endif()

foreach(runtime_dll IN LISTS CORE_ENGINE_MSVC_RUNTIME_DLLS)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtime_dll}"
                "${CORE_ENGINE_MSVC_RUNTIME_TARGET_DIR}"
        RESULT_VARIABLE copy_result
    )
    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Failed to copy MSVC runtime DLL: ${runtime_dll}")
    endif()
endforeach()

message(STATUS "Copied MSVC ${CORE_ENGINE_MSVC_RUNTIME_KIND} DLLs from ${CORE_ENGINE_MSVC_RUNTIME_DIR}")
