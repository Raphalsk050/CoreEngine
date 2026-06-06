project(CoreEngineSandbox LANGUAGES CXX)

add_executable(sandbox
    main.cc
    terrain_generator.cc
    terrain_generator.h
)

target_link_libraries(sandbox PRIVATE CoreEngine)
target_compile_features(sandbox PRIVATE cxx_std_23)

if(COMMAND copy_core_engine_runtime_dlls)
    copy_core_engine_runtime_dlls(sandbox)
endif()
