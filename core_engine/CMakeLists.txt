cmake_minimum_required(VERSION 3.14...4.6)
project(CoreEngine LANGUAGES CXX)

option(CORE_ENGINE_ENABLE_DILIGENT "Build the experimental Diligent render backend" OFF)
set(CORE_ENGINE_DILIGENT_CORE_DIR "" CACHE PATH "Path to a DiligentCore checkout")
option(CORE_ENGINE_ENABLE_STEAM "Enable Steamworks integration" ON)
set(CORE_ENGINE_STEAMWORKS_SDK_DIR "" CACHE PATH "Path to the Steamworks SDK root")
set(CORE_ENGINE_STEAM_APP_ID "480" CACHE STRING "Steam AppID used by development builds")

if(NOT CORE_ENGINE_STEAMWORKS_SDK_DIR)
    foreach(STEAMWORKS_SDK_CANDIDATE
            "$ENV{CORE_ENGINE_STEAMWORKS_SDK_DIR}"
            "$ENV{STEAMWORKS_SDK_DIR}"
            "$ENV{STEAM_SDK_DIR}"
            "${CMAKE_SOURCE_DIR}/steam_sdk"
            "${CMAKE_SOURCE_DIR}/steamworks_sdk"
            "${CMAKE_SOURCE_DIR}/third_party/steam_sdk"
            "${CMAKE_SOURCE_DIR}/third_party/steamworks_sdk"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/steam_sdk"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/steamworks_sdk")
        if(STEAMWORKS_SDK_CANDIDATE AND EXISTS "${STEAMWORKS_SDK_CANDIDATE}/public/steam/steam_api.h")
            set(CORE_ENGINE_STEAMWORKS_SDK_DIR "${STEAMWORKS_SDK_CANDIDATE}" CACHE PATH "Path to the Steamworks SDK root" FORCE)
            break()
        endif()
    endforeach()
endif()

add_subdirectory(third_party)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/(build|cmake-build|out|third_party)/.*")
list(FILTER ENGINE_HEADERS EXCLUDE REGEX ".*/(build|cmake-build|out|third_party)/.*")

if(NOT CORE_ENGINE_ENABLE_DILIGENT)
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/platform/diligent/.*")
    list(FILTER ENGINE_HEADERS EXCLUDE REGEX ".*/platform/diligent/.*")
endif()

add_library(${PROJECT_NAME} STATIC)
target_sources(${PROJECT_NAME} PRIVATE ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(${PROJECT_NAME} PUBLIC ThirdParty)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

set(CORE_ENGINE_STEAM_RUNTIME_LIBRARY "")
set(CORE_ENGINE_STEAM_RUNTIME_LIBRARY_NAME "")

if(CORE_ENGINE_ENABLE_STEAM)
    if(NOT EXISTS "${CORE_ENGINE_STEAMWORKS_SDK_DIR}/public/steam/steam_api.h")
        message(FATAL_ERROR "CORE_ENGINE_STEAMWORKS_SDK_DIR must point to the Steamworks SDK root. Use -DCORE_ENGINE_ENABLE_STEAM=OFF only for explicit offline builds.")
    endif()

    if(EXISTS "${CORE_ENGINE_STEAMWORKS_SDK_DIR}/CMakeLists.txt" AND NOT TARGET Steamworks::SDK)
        add_subdirectory("${CORE_ENGINE_STEAMWORKS_SDK_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/SteamworksSDK" EXCLUDE_FROM_ALL)
    endif()

    if(NOT TARGET Steamworks::SDK)
        message(FATAL_ERROR "Steamworks::SDK target was not created. Add CMakeLists.txt to CORE_ENGINE_STEAMWORKS_SDK_DIR or use the bundled core_engine/third_party/steamworks_sdk path.")
    endif()

    get_target_property(CORE_ENGINE_STEAM_RUNTIME_LIBRARY Steamworks::SDK CORE_ENGINE_RUNTIME_LIBRARY)
    get_target_property(CORE_ENGINE_STEAM_RUNTIME_LIBRARY_NAME Steamworks::SDK CORE_ENGINE_RUNTIME_LIBRARY_NAME)

    set(CORE_ENGINE_STEAM_RUNTIME_LIBRARY "${CORE_ENGINE_STEAM_RUNTIME_LIBRARY}"
        CACHE INTERNAL "Steamworks runtime library path")
    set(CORE_ENGINE_STEAM_RUNTIME_LIBRARY_NAME "${CORE_ENGINE_STEAM_RUNTIME_LIBRARY_NAME}"
        CACHE INTERNAL "Steamworks runtime library file name")

    target_link_libraries(${PROJECT_NAME} PUBLIC Steamworks::SDK)
endif()

set(CORE_ENGINE_HAS_DILIGENT_D3D11 0)
set(CORE_ENGINE_HAS_DILIGENT_D3D12 0)
set(CORE_ENGINE_HAS_DILIGENT_VULKAN 0)

if(CORE_ENGINE_ENABLE_DILIGENT)
    if(TARGET Diligent-GraphicsEngineD3D11-shared)
        set(CORE_ENGINE_HAS_DILIGENT_D3D11 1)
    endif()

    if(TARGET Diligent-GraphicsEngineD3D12-shared)
        set(CORE_ENGINE_HAS_DILIGENT_D3D12 1)
    endif()

    if(TARGET Diligent-GraphicsEngineVk-shared)
        set(CORE_ENGINE_HAS_DILIGENT_VULKAN 1)
    endif()
endif()

target_compile_definitions(${PROJECT_NAME}
    PUBLIC CENGINE_DEBUG_BUILD=$<IF:$<CONFIG:Debug>,1,0>
    PUBLIC CORE_ENGINE_ENABLE_STEAM=$<BOOL:${CORE_ENGINE_ENABLE_STEAM}>
    PUBLIC CORE_ENGINE_STEAM_APP_ID=${CORE_ENGINE_STEAM_APP_ID}
    PRIVATE CORE_ENGINE_ENABLE_DILIGENT=$<BOOL:${CORE_ENGINE_ENABLE_DILIGENT}>
    PRIVATE CORE_ENGINE_HAS_DILIGENT_D3D11=${CORE_ENGINE_HAS_DILIGENT_D3D11}
    PRIVATE CORE_ENGINE_HAS_DILIGENT_D3D12=${CORE_ENGINE_HAS_DILIGENT_D3D12}
    PRIVATE CORE_ENGINE_HAS_DILIGENT_VULKAN=${CORE_ENGINE_HAS_DILIGENT_VULKAN}
)

target_compile_options(${PROJECT_NAME} PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/wd4324>)

if(CORE_ENGINE_ENABLE_DILIGENT)
    if(WIN32)
        target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_WIN32=1)
    elseif(APPLE)
        target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_MACOS=1 PLATFORM_APPLE=1)
    elseif(UNIX)
        target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_LINUX=1)
    endif()
endif()

function(copy_steam_runtime_dlls target)
    if(NOT CORE_ENGINE_ENABLE_STEAM)
        return()
    endif()

    if(NOT EXISTS "${CORE_ENGINE_STEAM_RUNTIME_LIBRARY}")
        message(FATAL_ERROR "Steamworks runtime library was not found: ${CORE_ENGINE_STEAM_RUNTIME_LIBRARY}")
    endif()

    set(STEAM_APPID_FILE "${CMAKE_CURRENT_BINARY_DIR}/steam_appid.txt")
    file(WRITE "${STEAM_APPID_FILE}" "${CORE_ENGINE_STEAM_APP_ID}\n")

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CORE_ENGINE_STEAM_RUNTIME_LIBRARY}"
            "$<TARGET_FILE_DIR:${target}>/${CORE_ENGINE_STEAM_RUNTIME_LIBRARY_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${STEAM_APPID_FILE}"
            "$<TARGET_FILE_DIR:${target}>/steam_appid.txt"
        COMMENT "Copying Steamworks runtime files"
    )
endfunction()
