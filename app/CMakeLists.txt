project(CoreEngineSandbox LANGUAGES CXX)

add_executable(sandbox main.cc)

target_link_libraries(sandbox PRIVATE CoreEngine)
target_compile_features(sandbox PRIVATE cxx_std_23)

if(COMMAND copy_steam_runtime_dlls)
    copy_steam_runtime_dlls(sandbox)
endif()
