cmake_minimum_required(VERSION 3.31)
project(CPU LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(CPU main.cpp)

option(CPU_BUILD_LABS "Build CPU study labs" ON)
option(CPU_ENABLE_COVERAGE "Build with GCC/gcov coverage instrumentation" OFF)

if(CPU_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "CPU_ENABLE_COVERAGE currently expects GCC/gcov.")
    endif()

    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
endif()

if(CPU_BUILD_LABS)
    enable_testing()
    add_subdirectory(labs/lab00_benchmark_foundation)
endif()
