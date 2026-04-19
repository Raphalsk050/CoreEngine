project(ThirdParty)

add_subdirectory(entt EXCLUDE_FROM_ALL)
add_subdirectory(glm EXCLUDE_FROM_ALL)
add_subdirectory(sdl3 EXCLUDE_FROM_ALL)

set_property(DIRECTORY entt PROPERTY FOLDER "ThirdParty")
set_property(DIRECTORY glm PROPERTY FOLDER "ThirdParty")
set_property(DIRECTORY sdl3 PROPERTY FOLDER "ThirdParty")

if(CORE_ENGINE_ENABLE_DILIGENT)
    if(CORE_ENGINE_DILIGENT_CORE_DIR STREQUAL "")
        set(CORE_ENGINE_DILIGENT_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/DiligentCore" CACHE PATH "Path to a DiligentCore checkout" FORCE)
    endif()

    if(NOT EXISTS "${CORE_ENGINE_DILIGENT_CORE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "CORE_ENGINE_ENABLE_DILIGENT is ON, but DiligentCore was not found. Set CORE_ENGINE_DILIGENT_CORE_DIR or place DiligentCore in core_engine/third_party/DiligentCore.")
    endif()

    set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_FX OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_SAMPLES OFF CACHE BOOL "" FORCE)
    set(DILIGENT_NO_OPENGL ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_VULKAN ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_WEBGPU ON CACHE BOOL "" FORCE)

    add_subdirectory("${CORE_ENGINE_DILIGENT_CORE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/DiligentCore" EXCLUDE_FROM_ALL)
endif()

add_library(ThirdParty INTERFACE)

target_include_directories(ThirdParty INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"                # Entt
        "${CMAKE_CURRENT_SOURCE_DIR}/glm"            # GLM with strip prefix of glm/
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl3/include"   # sdl with strip prefix of sdl3/include/
)

target_link_libraries(ThirdParty INTERFACE glm Entt SDL3::SDL3)

if(CORE_ENGINE_ENABLE_DILIGENT)
    target_link_libraries(ThirdParty INTERFACE
            Diligent-BuildSettings
            Diligent-GraphicsEngineD3D11-shared
            Diligent-GraphicsEngineD3D12-shared
    )
endif()
