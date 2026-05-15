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

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${APP_SOURCES} ${APP_HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE CoreEngine)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

set(APP_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
if (EXISTS "${APP_ASSETS_DIR}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/app"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${APP_ASSETS_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/app/assets"
            COMMENT "Copying application assets"
    )
endif ()

if (COMMAND copy_required_dlls)
    copy_required_dlls(${PROJECT_NAME})
endif ()

if (COMMAND copy_steam_runtime_dlls)
    copy_steam_runtime_dlls(${PROJECT_NAME})
endif ()

if (TARGET Assimp)
    get_target_property(ASSIMP_RUNTIME_LIBRARY Assimp CORE_ENGINE_RUNTIME_LIBRARY)
    get_target_property(ASSIMP_RUNTIME_LIBRARY_NAME Assimp CORE_ENGINE_RUNTIME_LIBRARY_NAME)
    if (ASSIMP_RUNTIME_LIBRARY AND ASSIMP_RUNTIME_LIBRARY_NAME)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ASSIMP_RUNTIME_LIBRARY}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${ASSIMP_RUNTIME_LIBRARY_NAME}"
                COMMENT "Copying Assimp runtime"
        )
    endif ()
endif ()
