project(ThirdParty)

add_subdirectory(entt EXCLUDE_FROM_ALL)
add_subdirectory(glm EXCLUDE_FROM_ALL)
add_subdirectory(sdl3 EXCLUDE_FROM_ALL)

set_property(DIRECTORY entt PROPERTY FOLDER "ThirdParty")
set_property(DIRECTORY glm PROPERTY FOLDER "ThirdParty")
set_property(DIRECTORY sdl3 PROPERTY FOLDER "ThirdParty")

add_library(ThirdParty INTERFACE)

target_include_directories(ThirdParty INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"                # Entt
        "${CMAKE_CURRENT_SOURCE_DIR}/glm"            # GLM with strip prefix of glm/
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl3/include"   # sdl with strip prefix of sdl3/include/
)

target_link_libraries(ThirdParty INTERFACE glm Entt SDL3::SDL3)
