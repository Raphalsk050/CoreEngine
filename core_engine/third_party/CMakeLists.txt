project(ThirdParty)

add_subdirectory(entt EXCLUDE_FROM_ALL)
add_subdirectory(glm EXCLUDE_FROM_ALL)

set_property(DIRECTORY entt PROPERTY FOLDER "ThirdParty")
set_property(DIRECTORY glm PROPERTY FOLDER "ThirdParty")

add_library(ThirdParty INTERFACE)

target_include_directories(ThirdParty INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"       # Entt
    "${CMAKE_CURRENT_SOURCE_DIR}/glm"   # GLM with strip prefix of glm/
)

target_link_libraries(ThirdParty INTERFACE glm Entt)
