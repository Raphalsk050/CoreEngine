cmake_minimum_required(VERSION 3.10)

# Helper interface library to disable -Werror for the third-party libs
add_library(NO_WERROR INTERFACE)
if(MSVC)
    target_compile_options(NO_WERROR INTERFACE /WX-)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
        CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(NO_WERROR INTERFACE -Wno-error)
endif()

if (NOT TARGET JPEG::JPEG)
    add_subdirectory(libjpeg-9e)
endif()

if (NOT TARGET TIFF::TIFF)
    add_subdirectory(libtiff)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(LibTiff PRIVATE -Wno-shorten-64-to-32)
    endif()
endif()

if (NOT TARGET ZLIB::ZLIB)
    include(zlib.cmake)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(ZLib PRIVATE -Wno-shorten-64-to-32)
    endif()
endif()

if (NOT TARGET PNG::PNG)
    set(PNG_SHARED OFF CACHE BOOL "Build shared version of libpng")
    set(PNG_TOOLS OFF CACHE BOOL "Build libpng tools")
    set(PNG_TESTS OFF CACHE BOOL "Build libpng tests")
    set(SKIP_INSTALL_ALL ON)
    set(PNG_FRAMEWORK OFF)
    add_subdirectory(libpng)
    set_target_properties(png_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(PNG::PNG ALIAS png_static)
endif()
