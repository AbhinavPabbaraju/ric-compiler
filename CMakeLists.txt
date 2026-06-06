cmake_minimum_required(VERSION 3.25)

project(
    ric
    VERSION 0.1.0
    DESCRIPTION "A modern optimizing compiler for a Rust-inspired statically typed systems language"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)

add_compile_options("$<$<CONFIG:Debug>:-g3>")
add_compile_options("$<$<CONFIG:Debug>:-O0>")
add_compile_options("$<$<CONFIG:Release>:-O3>")
add_compile_options("$<$<CONFIG:Release>:-march=native>")
add_compile_options("$<$<CONFIG:Release>:-DNDEBUG>")

add_subdirectory(compiler)

option(BUILD_TESTS "Build tests (requires internet for Catch2)" OFF)
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
    )
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()
