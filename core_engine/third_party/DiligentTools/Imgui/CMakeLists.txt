cmake_minimum_required(VERSION 3.14...4.6)

project(Diligent-Imgui LANGUAGES CXX)

add_library(Diligent-Imgui STATIC
        interface/ImGuiDiligentRenderer.hpp
        interface/ImGuiImplDiligent.hpp
        interface/ImGuiImplSDL3.hpp
        src/ImGuiDiligentRenderer.cpp
        src/ImGuiImplDiligent.cpp
        src/ImGuiImplSDL3.cpp
)

target_include_directories(Diligent-Imgui PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/interface"
)

target_link_libraries(Diligent-Imgui
        PUBLIC
        imgui
        PRIVATE
        Diligent-BuildSettings
        Diligent-Common
        Diligent-PlatformInterface
        Diligent-GraphicsEngineInterface
        Diligent-GraphicsAccessories
        Diligent-GraphicsTools
        SDL3::SDL3
)

target_compile_definitions(Diligent-Imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_compile_features(Diligent-Imgui PUBLIC cxx_std_17)
set_target_properties(Diligent-Imgui PROPERTIES FOLDER "DiligentTools")
