project(ThirdParty)

option(CORE_ENGINE_REQUIRE_DIRECT3D_ON_WINDOWS
        "Fail configuration when Diligent Direct3D backends cannot be built on Windows"
        ON)

add_subdirectory(entt EXCLUDE_FROM_ALL)
add_subdirectory(glm EXCLUDE_FROM_ALL)
add_subdirectory(sdl3 EXCLUDE_FROM_ALL)
add_subdirectory(robin-map EXCLUDE_FROM_ALL)
add_subdirectory(Assimp EXCLUDE_FROM_ALL)

set_property(DIRECTORY entt PROPERTY FOLDER "ThirdParty")
set_property(DIRECTORY glm PROPERTY FOLDER "ThirdParty")
set_property(DIRECTORY sdl3 PROPERTY FOLDER "ThirdParty")
set_property(DIRECTORY robin-map PROPERTY FOLDER "ThirdParty")
set_property(DIRECTORY Assimp PROPERTY FOLDER "ThirdParty")

set(CORE_ENGINE_DEAR_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/DiligentTools/ThirdParty/imgui")

add_library(imgui STATIC
        "${CORE_ENGINE_DEAR_IMGUI_DIR}/imgui.cpp"
        "${CORE_ENGINE_DEAR_IMGUI_DIR}/imgui_draw.cpp"
        "${CORE_ENGINE_DEAR_IMGUI_DIR}/imgui_tables.cpp"
        "${CORE_ENGINE_DEAR_IMGUI_DIR}/imgui_widgets.cpp"
        "${CORE_ENGINE_DEAR_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
)

target_include_directories(imgui PUBLIC
        "${CORE_ENGINE_DEAR_IMGUI_DIR}"
        "${CORE_ENGINE_DEAR_IMGUI_DIR}/backends"
)

target_link_libraries(imgui PUBLIC SDL3::SDL3)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
set_target_properties(imgui PROPERTIES FOLDER "ThirdParty")

function(core_engine_require_msvc_developer_environment)
    if (NOT WIN32 OR NOT MSVC OR NOT CORE_ENGINE_REQUIRE_DIRECT3D_ON_WINDOWS)
        return()
    endif ()

    if (CMAKE_GENERATOR MATCHES "^Visual Studio")
        return()
    endif ()

    set(MISSING_ENVIRONMENT_VARIABLES)
    foreach (ENVIRONMENT_VARIABLE IN ITEMS VCToolsInstallDir WindowsSdkDir WindowsSDKVersion INCLUDE LIB)
        if ("$ENV{${ENVIRONMENT_VARIABLE}}" STREQUAL "")
            list(APPEND MISSING_ENVIRONMENT_VARIABLES "${ENVIRONMENT_VARIABLE}")
        endif ()
    endforeach ()

    if (MISSING_ENVIRONMENT_VARIABLES)
        list(JOIN MISSING_ENVIRONMENT_VARIABLES ", " MISSING_ENVIRONMENT_VARIABLES_TEXT)
        message(FATAL_ERROR
                "CoreEngine requires the Visual Studio developer environment when configuring Diligent Direct3D "
                "with the ${CMAKE_GENERATOR} generator. Missing environment variables: "
                "${MISSING_ENVIRONMENT_VARIABLES_TEXT}. "
                "Configure from a Developer Command Prompt or run: "
                "cmd.exe /d /s /c '\"%ProgramFiles%\\Microsoft Visual Studio\\2022\\Community\\Common7\\Tools\\VsDevCmd.bat\" "
                "-arch=x64 -host_arch=x64 && cmake -S . -B cmake-build-debug -G Ninja "
                "-DCMAKE_BUILD_TYPE=Debug -DCORE_ENGINE_ENABLE_DILIGENT=ON'")
    endif ()
endfunction()

function(core_engine_require_diligent_direct3d_backend BACKEND_NAME BACKEND_TARGET BACKEND_SUPPORTED)
    if (NOT WIN32 OR NOT CORE_ENGINE_REQUIRE_DIRECT3D_ON_WINDOWS)
        return()
    endif ()

    if (NOT ${BACKEND_SUPPORTED})
        message(FATAL_ERROR
                "Diligent ${BACKEND_NAME} support was not detected, but CoreEngine requires Direct3D on Windows. "
                "The CMake cache was not accepted because this would make runtime requests fall back away from "
                "Direct3D. Reconfigure from a Visual Studio Developer Command Prompt so d3d11.h/d3d12.h, atlbase.h, "
                "and the Windows SDK are visible.")
    endif ()

    if (NOT TARGET ${BACKEND_TARGET})
        message(FATAL_ERROR
                "Diligent ${BACKEND_NAME} was detected but target ${BACKEND_TARGET} was not generated.")
    endif ()
endfunction()

if (CORE_ENGINE_ENABLE_DILIGENT)
    core_engine_require_msvc_developer_environment()

    if (CORE_ENGINE_DILIGENT_CORE_DIR STREQUAL "")
        set(CORE_ENGINE_DILIGENT_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/DiligentCore" CACHE PATH "Path to a DiligentCore checkout" FORCE)
    endif ()

    if (NOT EXISTS "${CORE_ENGINE_DILIGENT_CORE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "CORE_ENGINE_ENABLE_DILIGENT is ON, but DiligentCore was not found. Set CORE_ENGINE_DILIGENT_CORE_DIR or place DiligentCore in core_engine/third_party/DiligentCore.")
    endif ()

    set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_FX OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_SAMPLES OFF CACHE BOOL "" FORCE)
    set(DILIGENT_NO_DIRECT3D11 OFF CACHE BOOL "" FORCE)
    set(DILIGENT_NO_DIRECT3D12 OFF CACHE BOOL "" FORCE)
    set(DILIGENT_NO_OPENGL ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_VULKAN OFF CACHE BOOL "" FORCE)
    set(DILIGENT_NO_WEBGPU ON CACHE BOOL "" FORCE)

    add_subdirectory("${CORE_ENGINE_DILIGENT_CORE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/DiligentCore" EXCLUDE_FROM_ALL)

    core_engine_require_diligent_direct3d_backend("D3D11" "Diligent-GraphicsEngineD3D11-shared" D3D11_SUPPORTED)
    core_engine_require_diligent_direct3d_backend("D3D12" "Diligent-GraphicsEngineD3D12-shared" D3D12_SUPPORTED)

    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/DiligentTools/ThirdParty" "${CMAKE_CURRENT_BINARY_DIR}/DiligentTools/ThirdParty" EXCLUDE_FROM_ALL)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/DiligentTools/Imgui" "${CMAKE_CURRENT_BINARY_DIR}/DiligentTools/Imgui" EXCLUDE_FROM_ALL)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/DiligentTools/TextureLoader" "${CMAKE_CURRENT_BINARY_DIR}/DiligentTools/TextureLoader" EXCLUDE_FROM_ALL)
endif ()

add_library(ThirdParty INTERFACE)

target_include_directories(ThirdParty INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"                     # Entt
        "${CMAKE_CURRENT_SOURCE_DIR}/glm"                 # GLM with strip prefix of glm/
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl3/include"        # sdl with strip prefix of sdl3/include/
        "${CMAKE_CURRENT_SOURCE_DIR}/robin-map/include"   # robin-map with strip prefix of robin-map/include/
        "${CMAKE_CURRENT_SOURCE_DIR}/Assimp/include"      # Assimp with strip prefix of Assimp/include/
)

target_link_libraries(ThirdParty INTERFACE glm Entt SDL3::SDL3 imgui robin_map Assimp)

if (CORE_ENGINE_ENABLE_DILIGENT)
    target_link_libraries(ThirdParty INTERFACE Diligent-BuildSettings Diligent-Imgui Diligent-TextureLoader)

    if (TARGET Diligent-GraphicsEngineD3D11-shared)
        target_link_libraries(ThirdParty INTERFACE Diligent-GraphicsEngineD3D11-shared)
    endif ()

    if (TARGET Diligent-GraphicsEngineD3D12-shared)
        target_link_libraries(ThirdParty INTERFACE Diligent-GraphicsEngineD3D12-shared)
    endif ()

    if (TARGET Diligent-GraphicsEngineVk-shared)
        target_link_libraries(ThirdParty INTERFACE Diligent-GraphicsEngineVk-shared)
    else ()
        message(FATAL_ERROR "Diligent Vulkan backend is required but Diligent-GraphicsEngineVk-shared was not generated.")
    endif ()
endif ()

target_compile_definitions(ThirdParty INTERFACE
        GLM_FORCE_INLINE
        GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
        GLM_FORCE_INTRINSICS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_LEFT_HANDED
        GLM_FORCE_SILENT_WARNINGS
)
