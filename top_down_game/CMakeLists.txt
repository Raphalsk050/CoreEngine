project(CoreEngineSandbox LANGUAGES CXX)

file(GLOB_RECURSE GAME_SRC_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")


file(GLOB_RECURSE GAME_HDR_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")


add_executable(TopDownGame
        ${GAME_SRC_FILES}
)

target_include_directories(TopDownGame PRIVATE ${GAME_HDR_FILES})
target_link_libraries(TopDownGame PRIVATE CoreEngine)
target_compile_features(TopDownGame PRIVATE cxx_std_23)

if (COMMAND copy_core_engine_runtime_dlls)
    copy_core_engine_runtime_dlls(TopDownGame)
endif ()
