project(Assimp LANGUAGES CXX)

set(ASSIMP_RUNTIME_LIBRARY_NAME "")

if (WIN32)
    set(ASSIMP_RUNTIME_LIBRARY_NAME "assimp-vc143-mt.dll")
    set(ASSIMP_RUNTIME_LIBRARY_DIR "windows-x64")
elseif (APPLE)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        set(ASSIMP_RUNTIME_LIBRARY_NAME "libassimp.6.dylib")
        set(ASSIMP_RUNTIME_LIBRARY_DIR "macos-arm64")
    else ()
        message(FATAL_ERROR "Assimp prebuilt runtime is not available for macOS architecture '${CMAKE_SYSTEM_PROCESSOR}'.")
    endif ()
elseif (UNIX)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        set(ASSIMP_RUNTIME_LIBRARY_NAME "libassimp.so.6")
        set(ASSIMP_RUNTIME_LIBRARY_DIR "linux-x64")
    else ()
        message(FATAL_ERROR "Assimp prebuilt runtime is not available for Linux architecture '${CMAKE_SYSTEM_PROCESSOR}'.")
    endif ()
else ()
    message(FATAL_ERROR "Assimp prebuilt runtime is not available for this platform.")
endif ()

set(ASSIMP_RUNTIME_LIBRARY
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/${ASSIMP_RUNTIME_LIBRARY_DIR}/${ASSIMP_RUNTIME_LIBRARY_NAME}")

if (NOT EXISTS "${ASSIMP_RUNTIME_LIBRARY}")
    message(FATAL_ERROR "Assimp runtime was not found: ${ASSIMP_RUNTIME_LIBRARY}")
endif ()

set(ASSIMP_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/include/assimp/config.h.in"
        "${ASSIMP_GENERATED_INCLUDE_DIR}/assimp/config.h"
)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE
        "${ASSIMP_GENERATED_INCLUDE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(${PROJECT_NAME} INTERFACE
        "CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_NAME=\"${ASSIMP_RUNTIME_LIBRARY_NAME}\""
        "CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_PATH=\"${ASSIMP_RUNTIME_LIBRARY}\""
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        CORE_ENGINE_RUNTIME_LIBRARY "${ASSIMP_RUNTIME_LIBRARY}"
        CORE_ENGINE_RUNTIME_LIBRARY_NAME "${ASSIMP_RUNTIME_LIBRARY_NAME}"
)
