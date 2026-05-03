cmake_minimum_required(VERSION 3.14...4.6)
project(CoreEngine LANGUAGES CXX)

option(CORE_ENGINE_ENABLE_DILIGENT "Build the experimental Diligent render backend" OFF)
set(CORE_ENGINE_DILIGENT_CORE_DIR "" CACHE PATH "Path to a DiligentCore checkout")

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

target_compile_definitions(${PROJECT_NAME}
    PUBLIC CENGINE_DEBUG_BUILD=$<IF:$<CONFIG:Debug>,1,0>
    PRIVATE CORE_ENGINE_ENABLE_DILIGENT=$<BOOL:${CORE_ENGINE_ENABLE_DILIGENT}>
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
