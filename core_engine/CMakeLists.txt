cmake_minimum_required(VERSION 3.14...4.6)
project(CoreEngine LANGUAGES CXX)

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

add_library(${PROJECT_NAME} STATIC)
target_sources(${PROJECT_NAME} PRIVATE ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(${PROJECT_NAME} PUBLIC ThirdParty)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
