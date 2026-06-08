project(CoreEnginePbrDemo LANGUAGES CXX)

add_executable(PbrDemo
    main.cc
)

target_link_libraries(PbrDemo PRIVATE CoreEngine)
target_compile_features(PbrDemo PRIVATE cxx_std_23)

if(COMMAND copy_core_engine_runtime_dlls)
    copy_core_engine_runtime_dlls(PbrDemo)
endif()
