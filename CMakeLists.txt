cmake_minimum_required(VERSION 3.14...4.6)

project(CoreEngineProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(core_engine)
add_subdirectory(app)
