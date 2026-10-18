cmake_minimum_required(VERSION 3.10)
project(dethrace_android C)

# Build SDL2 from source as a shared library
add_subdirectory(SDL)

# Tell dethrace's CMake we're embedded in an Android build
set(DETHRACE_ANDROID_BUILD ON CACHE BOOL "" FORCE)
set(DETHRACE_WERROR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DETHRACE_INSTALL OFF CACHE BOOL "" FORCE)

# Provide SDL2::SDL2 alias expected by dethrace and ensure find_package short-circuits
if(NOT TARGET SDL2::SDL2)
    if(TARGET SDL2)
        add_library(SDL2::SDL2 ALIAS SDL2)
    endif()
endif()
set(SDL2_FOUND TRUE CACHE BOOL "" FORCE)

# Path to dethrace repo root (3 levels up from android/app/jni)
get_filename_component(DETHRACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_subdirectory(${DETHRACE_ROOT} dethrace_build)
