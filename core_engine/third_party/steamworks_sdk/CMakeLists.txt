cmake_minimum_required(VERSION 3.14...4.6)

project(SteamworksSDK LANGUAGES CXX)

set(STEAMWORKS_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "Steamworks SDK root")

set(STEAMWORKS_REQUIRED_HEADERS
    "public/steam/steam_api.h"
    "public/steam/steam_api_common.h"
    "public/steam/steamclientpublic.h"
    "public/steam/steamnetworkingtypes.h"
    "public/steam/steamtypes.h"
    "public/steam/isteamnetworkingsockets.h"
    "public/steam/isteamnetworkingutils.h"
)

set(STEAMWORKS_MISSING_HEADERS)
foreach(required_header IN LISTS STEAMWORKS_REQUIRED_HEADERS)
    if(NOT EXISTS "${STEAMWORKS_SDK_ROOT}/${required_header}")
        list(APPEND STEAMWORKS_MISSING_HEADERS "${required_header}")
    endif()
endforeach()

if(STEAMWORKS_MISSING_HEADERS)
    list(JOIN STEAMWORKS_MISSING_HEADERS ", " STEAMWORKS_MISSING_HEADERS_TEXT)
    message(FATAL_ERROR "Steamworks SDK is incomplete at ${STEAMWORKS_SDK_ROOT}. Missing headers: ${STEAMWORKS_MISSING_HEADERS_TEXT}. Copy the full SDK public/steam directory, not only steam_api.h and the import library.")
endif()

if(WIN32)
    set(STEAMWORKS_IMPORT_LIBRARY_CANDIDATES
        "${STEAMWORKS_SDK_ROOT}/public/steam/lib/win64/steam_api64.lib"
        "${STEAMWORKS_SDK_ROOT}/redistributable_bin/win64/steam_api64.lib"
    )
    set(STEAMWORKS_RUNTIME_LIBRARY "${STEAMWORKS_SDK_ROOT}/redistributable_bin/win64/steam_api64.dll")
    set(STEAMWORKS_RUNTIME_LIBRARY_NAME "steam_api64.dll")
elseif(APPLE)
    set(STEAMWORKS_IMPORT_LIBRARY_CANDIDATES
        "${STEAMWORKS_SDK_ROOT}/redistributable_bin/osx/libsteam_api.dylib"
    )
    set(STEAMWORKS_RUNTIME_LIBRARY "${STEAMWORKS_SDK_ROOT}/redistributable_bin/osx/libsteam_api.dylib")
    set(STEAMWORKS_RUNTIME_LIBRARY_NAME "libsteam_api.dylib")
elseif(UNIX)
    set(STEAMWORKS_IMPORT_LIBRARY_CANDIDATES
        "${STEAMWORKS_SDK_ROOT}/redistributable_bin/linux64/libsteam_api.so"
    )
    set(STEAMWORKS_RUNTIME_LIBRARY "${STEAMWORKS_SDK_ROOT}/redistributable_bin/linux64/libsteam_api.so")
    set(STEAMWORKS_RUNTIME_LIBRARY_NAME "libsteam_api.so")
else()
    message(FATAL_ERROR "Steamworks SDK integration is not configured for this platform")
endif()

foreach(candidate IN LISTS STEAMWORKS_IMPORT_LIBRARY_CANDIDATES)
    if(EXISTS "${candidate}")
        set(STEAMWORKS_IMPORT_LIBRARY "${candidate}")
        break()
    endif()
endforeach()

if(NOT STEAMWORKS_IMPORT_LIBRARY)
    message(FATAL_ERROR "Steamworks import library not found under ${STEAMWORKS_SDK_ROOT}")
endif()

if(NOT EXISTS "${STEAMWORKS_RUNTIME_LIBRARY}")
    message(FATAL_ERROR "Steamworks runtime library not found at ${STEAMWORKS_RUNTIME_LIBRARY}")
endif()

add_library(Steamworks::SDK SHARED IMPORTED GLOBAL)

set_target_properties(Steamworks::SDK PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${STEAMWORKS_SDK_ROOT}/public"
    CORE_ENGINE_RUNTIME_LIBRARY "${STEAMWORKS_RUNTIME_LIBRARY}"
    CORE_ENGINE_RUNTIME_LIBRARY_NAME "${STEAMWORKS_RUNTIME_LIBRARY_NAME}"
    CORE_ENGINE_SDK_ROOT "${STEAMWORKS_SDK_ROOT}"
)

if(WIN32)
    set_target_properties(Steamworks::SDK PROPERTIES
        IMPORTED_IMPLIB "${STEAMWORKS_IMPORT_LIBRARY}"
        IMPORTED_LOCATION "${STEAMWORKS_RUNTIME_LIBRARY}"
    )
else()
    set_target_properties(Steamworks::SDK PROPERTIES
        IMPORTED_LOCATION "${STEAMWORKS_IMPORT_LIBRARY}"
    )
endif()
