project(ThirdParty)

option(CORE_ENGINE_REQUIRE_DIRECT3D_ON_WINDOWS
        "Fail configuration when Diligent Direct3D backends cannot be built on Windows"
        ON)
option(CORE_ENGINE_COPY_MSVC_RUNTIME_DLLS
        "Copy MSVC runtime DLLs next to Windows executables"
        ON)
option(CORE_ENGINE_COPY_DEBUG_MSVC_RUNTIME_DLLS
        "Copy MSVC Debug CRT DLLs for developer-machine handoff builds"
        ON)
option(CORE_ENGINE_COPY_WINDOWS_UCRT_DLLS
        "Copy Windows UCRT DLLs next to Windows executables"
        ON)
set(CORE_ENGINE_MSVC_REDIST_DIR "" CACHE PATH "Path to the MSVC Redist/MSVC/<toolset> directory")

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

function(core_engine_find_msvc_redist_root OUT_VARIABLE)
    set(REDIST_CANDIDATES)

    if (CORE_ENGINE_MSVC_REDIST_DIR)
        list(APPEND REDIST_CANDIDATES "${CORE_ENGINE_MSVC_REDIST_DIR}")
    endif ()

    if (DEFINED ENV{VCToolsRedistDir} AND NOT "$ENV{VCToolsRedistDir}" STREQUAL "")
        list(APPEND REDIST_CANDIDATES "$ENV{VCToolsRedistDir}")
    endif ()
    if (DEFINED ENV{VCTOOLSREDISTDIR} AND NOT "$ENV{VCTOOLSREDISTDIR}" STREQUAL "")
        list(APPEND REDIST_CANDIDATES "$ENV{VCTOOLSREDISTDIR}")
    endif ()

    foreach (VCTOOLS_INSTALL_DIR IN ITEMS "$ENV{VCToolsInstallDir}" "$ENV{VCTOOLSINSTALLDIR}")
        if (VCTOOLS_INSTALL_DIR)
            file(TO_CMAKE_PATH "${VCTOOLS_INSTALL_DIR}" NORMALIZED_VCTOOLS_INSTALL_DIR)
            string(REGEX REPLACE "/+$" "" NORMALIZED_VCTOOLS_INSTALL_DIR "${NORMALIZED_VCTOOLS_INSTALL_DIR}")
            get_filename_component(TOOLSET_VERSION "${NORMALIZED_VCTOOLS_INSTALL_DIR}" NAME)
            get_filename_component(MSVC_ROOT "${NORMALIZED_VCTOOLS_INSTALL_DIR}" DIRECTORY)
            get_filename_component(TOOLS_ROOT "${MSVC_ROOT}" DIRECTORY)
            get_filename_component(VC_ROOT "${TOOLS_ROOT}" DIRECTORY)
            list(APPEND REDIST_CANDIDATES "${VC_ROOT}/Redist/MSVC/${TOOLSET_VERSION}")
        endif ()
    endforeach ()

    if (CMAKE_CXX_COMPILER)
        get_filename_component(COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(HOST_DIR "${COMPILER_DIR}" DIRECTORY)
        get_filename_component(BIN_DIR "${HOST_DIR}" DIRECTORY)
        get_filename_component(TOOLSET_DIR "${BIN_DIR}" DIRECTORY)
        file(REAL_PATH "${TOOLSET_DIR}" TOOLSET_DIR)
        get_filename_component(TOOLSET_VERSION "${TOOLSET_DIR}" NAME)
        get_filename_component(MSVC_ROOT "${TOOLSET_DIR}" DIRECTORY)
        get_filename_component(TOOLS_ROOT "${MSVC_ROOT}" DIRECTORY)
        get_filename_component(VC_ROOT "${TOOLS_ROOT}" DIRECTORY)
        file(REAL_PATH "${VC_ROOT}" VC_ROOT)
        list(APPEND REDIST_CANDIDATES "${VC_ROOT}/Redist/MSVC/${TOOLSET_VERSION}")
    endif ()

    foreach (REDIST_CANDIDATE IN LISTS REDIST_CANDIDATES)
        file(TO_CMAKE_PATH "${REDIST_CANDIDATE}" NORMALIZED_REDIST_CANDIDATE)
        if (EXISTS "${NORMALIZED_REDIST_CANDIDATE}")
            set(${OUT_VARIABLE} "${NORMALIZED_REDIST_CANDIDATE}" PARENT_SCOPE)
            return()
        endif ()
    endforeach ()

    set(${OUT_VARIABLE} "" PARENT_SCOPE)
endfunction()

function(core_engine_msvc_runtime_arch OUT_VARIABLE)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(${OUT_VARIABLE} "arm64" PARENT_SCOPE)
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(${OUT_VARIABLE} "x64" PARENT_SCOPE)
    else ()
        set(${OUT_VARIABLE} "x86" PARENT_SCOPE)
    endif ()
endfunction()

function(core_engine_find_windows_sdk_root OUT_VARIABLE)
    set(SDK_CANDIDATES)

    if (DEFINED WINDOWS_SDK_BIN_DIR AND NOT "${WINDOWS_SDK_BIN_DIR}" STREQUAL "")
        get_filename_component(WINDOWS_SDK_VERSION_DIR "${WINDOWS_SDK_BIN_DIR}" DIRECTORY)
        get_filename_component(WINDOWS_SDK_BIN_PARENT "${WINDOWS_SDK_VERSION_DIR}" DIRECTORY)
        get_filename_component(WINDOWS_SDK_ROOT_FROM_BIN "${WINDOWS_SDK_BIN_PARENT}" DIRECTORY)
        list(APPEND SDK_CANDIDATES "${WINDOWS_SDK_ROOT_FROM_BIN}")
    elseif (DEFINED D3D_COMPILER_PATH AND NOT "${D3D_COMPILER_PATH}" STREQUAL "")
        get_filename_component(WINDOWS_SDK_BIN_FROM_D3D "${D3D_COMPILER_PATH}" DIRECTORY)
        get_filename_component(WINDOWS_SDK_VERSION_DIR "${WINDOWS_SDK_BIN_FROM_D3D}" DIRECTORY)
        get_filename_component(WINDOWS_SDK_BIN_PARENT "${WINDOWS_SDK_VERSION_DIR}" DIRECTORY)
        get_filename_component(WINDOWS_SDK_ROOT_FROM_D3D "${WINDOWS_SDK_BIN_PARENT}" DIRECTORY)
        list(APPEND SDK_CANDIDATES "${WINDOWS_SDK_ROOT_FROM_D3D}")
    endif ()

    if (DEFINED ENV{WindowsSdkDir} AND NOT "$ENV{WindowsSdkDir}" STREQUAL "")
        list(APPEND SDK_CANDIDATES "$ENV{WindowsSdkDir}")
    endif ()

    list(APPEND SDK_CANDIDATES "C:/Program Files (x86)/Windows Kits/10")

    foreach (SDK_CANDIDATE IN LISTS SDK_CANDIDATES)
        file(TO_CMAKE_PATH "${SDK_CANDIDATE}" NORMALIZED_SDK_CANDIDATE)
        string(REGEX REPLACE "/$" "" NORMALIZED_SDK_CANDIDATE "${NORMALIZED_SDK_CANDIDATE}")
        if (EXISTS "${NORMALIZED_SDK_CANDIDATE}/Redist" OR EXISTS "${NORMALIZED_SDK_CANDIDATE}/bin")
            set(${OUT_VARIABLE} "${NORMALIZED_SDK_CANDIDATE}" PARENT_SCOPE)
            return()
        endif ()
    endforeach ()

    set(${OUT_VARIABLE} "" PARENT_SCOPE)
endfunction()

function(core_engine_windows_sdk_version OUT_VARIABLE)
    set(SDK_VERSION "")
    if (DEFINED WINDOWS_SDK_BIN_DIR AND NOT "${WINDOWS_SDK_BIN_DIR}" STREQUAL "")
        get_filename_component(WINDOWS_SDK_ARCH_DIR "${WINDOWS_SDK_BIN_DIR}" DIRECTORY)
        get_filename_component(SDK_VERSION "${WINDOWS_SDK_ARCH_DIR}" NAME)
    elseif (DEFINED D3D_COMPILER_PATH AND NOT "${D3D_COMPILER_PATH}" STREQUAL "")
        get_filename_component(WINDOWS_SDK_BIN_FROM_D3D "${D3D_COMPILER_PATH}" DIRECTORY)
        get_filename_component(WINDOWS_SDK_VERSION_DIR "${WINDOWS_SDK_BIN_FROM_D3D}" DIRECTORY)
        get_filename_component(SDK_VERSION "${WINDOWS_SDK_VERSION_DIR}" NAME)
    endif ()

    if (SDK_VERSION STREQUAL "")
        set(SDK_VERSION "$ENV{WindowsSDKVersion}")
    endif ()
    if (SDK_VERSION STREQUAL "" AND DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        set(SDK_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
    endif ()

    string(REPLACE "\\" "/" SDK_VERSION "${SDK_VERSION}")
    string(REGEX REPLACE "/$" "" SDK_VERSION "${SDK_VERSION}")
    set(${OUT_VARIABLE} "${SDK_VERSION}" PARENT_SCOPE)
endfunction()

function(core_engine_find_windows_ucrt_dirs OUT_RELEASE_DIR OUT_DEBUG_DIR)
    set(${OUT_RELEASE_DIR} "" PARENT_SCOPE)
    set(${OUT_DEBUG_DIR} "" PARENT_SCOPE)

    core_engine_find_windows_sdk_root(WINDOWS_SDK_ROOT)
    if (WINDOWS_SDK_ROOT STREQUAL "")
        return()
    endif ()

    core_engine_windows_sdk_version(WINDOWS_SDK_VERSION)
    core_engine_msvc_runtime_arch(WINDOWS_UCRT_ARCH)

    set(RELEASE_CANDIDATES)
    set(DEBUG_CANDIDATES)
    if (DEFINED WINDOWS_SDK_BIN_DIR AND NOT "${WINDOWS_SDK_BIN_DIR}" STREQUAL "")
        list(APPEND DEBUG_CANDIDATES "${WINDOWS_SDK_BIN_DIR}/ucrt")
    elseif (DEFINED D3D_COMPILER_PATH AND NOT "${D3D_COMPILER_PATH}" STREQUAL "")
        get_filename_component(WINDOWS_SDK_BIN_FROM_D3D "${D3D_COMPILER_PATH}" DIRECTORY)
        list(APPEND DEBUG_CANDIDATES "${WINDOWS_SDK_BIN_FROM_D3D}/ucrt")
    endif ()

    if (NOT WINDOWS_SDK_VERSION STREQUAL "")
        list(APPEND RELEASE_CANDIDATES "${WINDOWS_SDK_ROOT}/Redist/${WINDOWS_SDK_VERSION}/ucrt/DLLs/${WINDOWS_UCRT_ARCH}")
        list(APPEND DEBUG_CANDIDATES "${WINDOWS_SDK_ROOT}/bin/${WINDOWS_SDK_VERSION}/${WINDOWS_UCRT_ARCH}/ucrt")
    endif ()
    list(APPEND RELEASE_CANDIDATES "${WINDOWS_SDK_ROOT}/Redist/ucrt/DLLs/${WINDOWS_UCRT_ARCH}")

    foreach (RELEASE_CANDIDATE IN LISTS RELEASE_CANDIDATES)
        if (EXISTS "${RELEASE_CANDIDATE}/ucrtbase.dll")
            set(${OUT_RELEASE_DIR} "${RELEASE_CANDIDATE}" PARENT_SCOPE)
            break()
        endif ()
    endforeach ()

    foreach (DEBUG_CANDIDATE IN LISTS DEBUG_CANDIDATES)
        if (EXISTS "${DEBUG_CANDIDATE}/ucrtbased.dll")
            set(${OUT_DEBUG_DIR} "${DEBUG_CANDIDATE}" PARENT_SCOPE)
            break()
        endif ()
    endforeach ()
endfunction()

function(copy_msvc_runtime_dlls TARGET_NAME)
    if (NOT WIN32 OR NOT MSVC OR NOT CORE_ENGINE_COPY_MSVC_RUNTIME_DLLS)
        return()
    endif ()

    core_engine_find_msvc_redist_root(MSVC_REDIST_ROOT)
    if (MSVC_REDIST_ROOT STREQUAL "")
        message(WARNING "MSVC runtime redist root was not found. Runtime CRT DLLs will not be copied.")
        return()
    endif ()

    core_engine_msvc_runtime_arch(MSVC_RUNTIME_ARCH)
    set(MSVC_RELEASE_RUNTIME_DIR "${MSVC_REDIST_ROOT}/${MSVC_RUNTIME_ARCH}/Microsoft.VC143.CRT")
    set(MSVC_DEBUG_RUNTIME_DIR "${MSVC_REDIST_ROOT}/debug_nonredist/${MSVC_RUNTIME_ARCH}/Microsoft.VC143.DebugCRT")

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                    "-DCORE_ENGINE_MSVC_RUNTIME_CONFIG=$<CONFIG>"
                    "-DCORE_ENGINE_MSVC_RELEASE_RUNTIME_DIR=${MSVC_RELEASE_RUNTIME_DIR}"
                    "-DCORE_ENGINE_MSVC_DEBUG_RUNTIME_DIR=${MSVC_DEBUG_RUNTIME_DIR}"
                    "-DCORE_ENGINE_MSVC_RUNTIME_TARGET_DIR=$<TARGET_FILE_DIR:${TARGET_NAME}>"
                    "-DCORE_ENGINE_COPY_DEBUG_MSVC_RUNTIME_DLLS=${CORE_ENGINE_COPY_DEBUG_MSVC_RUNTIME_DLLS}"
                    -P "${CMAKE_SOURCE_DIR}/cmake/copy_msvc_runtime.cmake"
            COMMENT "Copying MSVC runtime DLLs"
    )
endfunction()

function(copy_windows_ucrt_dlls TARGET_NAME)
    if (NOT WIN32 OR NOT MSVC OR NOT CORE_ENGINE_COPY_WINDOWS_UCRT_DLLS)
        return()
    endif ()

    core_engine_find_windows_ucrt_dirs(WINDOWS_UCRT_RELEASE_DIR WINDOWS_UCRT_DEBUG_DIR)
    if (WINDOWS_UCRT_RELEASE_DIR STREQUAL "" AND WINDOWS_UCRT_DEBUG_DIR STREQUAL "")
        message(WARNING "Windows UCRT runtime directories were not found. UCRT DLLs will not be copied.")
        return()
    endif ()

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                    "-DCORE_ENGINE_WINDOWS_UCRT_CONFIG=$<CONFIG>"
                    "-DCORE_ENGINE_WINDOWS_UCRT_RELEASE_DIR=${WINDOWS_UCRT_RELEASE_DIR}"
                    "-DCORE_ENGINE_WINDOWS_UCRT_DEBUG_DIR=${WINDOWS_UCRT_DEBUG_DIR}"
                    "-DCORE_ENGINE_WINDOWS_UCRT_TARGET_DIR=$<TARGET_FILE_DIR:${TARGET_NAME}>"
                    -P "${CMAKE_SOURCE_DIR}/cmake/copy_windows_ucrt.cmake"
            COMMENT "Copying Windows UCRT DLLs"
    )
endfunction()

function(copy_required_dlls TARGET_NAME)
    if (NOT WIN32)
        return()
    endif ()

    set(RUNTIME_DLLS)

    if (TARGET SDL3-shared)
        list(APPEND RUNTIME_DLLS "$<TARGET_FILE:SDL3-shared>")
    endif ()

    if (TARGET Assimp)
        get_target_property(ASSIMP_RUNTIME_LIBRARY Assimp CORE_ENGINE_RUNTIME_LIBRARY)
        if (ASSIMP_RUNTIME_LIBRARY AND EXISTS "${ASSIMP_RUNTIME_LIBRARY}")
            list(APPEND RUNTIME_DLLS "${ASSIMP_RUNTIME_LIBRARY}")
        endif ()
    endif ()

    if (CORE_ENGINE_ENABLE_DILIGENT)
        foreach (DILIGENT_RUNTIME_TARGET IN ITEMS
                Diligent-GraphicsEngineD3D11-shared
                Diligent-GraphicsEngineD3D12-shared
                Diligent-GraphicsEngineVk-shared)
            if (TARGET ${DILIGENT_RUNTIME_TARGET})
                list(APPEND RUNTIME_DLLS "$<TARGET_FILE:${DILIGENT_RUNTIME_TARGET}>")
            endif ()
        endforeach ()
    endif ()

    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
        list(APPEND RUNTIME_DLLS "$<TARGET_RUNTIME_DLLS:${TARGET_NAME}>")
    endif ()

    if (RUNTIME_DLLS)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${RUNTIME_DLLS}
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                COMMAND_EXPAND_LISTS
                COMMENT "Copying runtime DLL dependencies"
        )
    endif ()

    if (CORE_ENGINE_ENABLE_DILIGENT AND COMMAND copy_shader_compiler_dlls)
        copy_shader_compiler_dlls(${TARGET_NAME}
                D3D_COMPILER YES
                DXCOMPILER YES
                DXCOMPILER_FOR_SPIRV YES
        )
    endif ()

    copy_msvc_runtime_dlls(${TARGET_NAME})
    copy_windows_ucrt_dlls(${TARGET_NAME})
endfunction()
