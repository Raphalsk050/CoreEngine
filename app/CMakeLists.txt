cmake_minimum_required(VERSION 3.14...4.6)
project(sandbox LANGUAGES CXX)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

file(GLOB_RECURSE APP_HEADERS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

list(FILTER APP_SOURCES EXCLUDE REGEX ".*/(build|cmake-build|out|third_party)/.*")
list(FILTER APP_HEADERS EXCLUDE REGEX ".*/(build|cmake-build|out|third_party)/.*")

add_executable(${PROJECT_NAME} ${APP_SOURCES})
target_sources(${PROJECT_NAME} PRIVATE ${APP_SOURCES} ${APP_HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE CoreEngine)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")


if (COMMAND copy_required_dlls)
    copy_required_dlls(${PROJECT_NAME})
endif ()
