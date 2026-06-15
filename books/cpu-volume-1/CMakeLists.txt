cmake_minimum_required(VERSION 3.31)
project(CPUVolume1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPU_VOLUME_1_BUILD_LABS "Build CPU Volume 1 labs" ON)
option(CPU_VOLUME_1_ENABLE_COVERAGE "Build CPU Volume 1 labs with GCC/gcov coverage instrumentation" OFF)

if(CPU_VOLUME_1_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "CPU_VOLUME_1_ENABLE_COVERAGE currently expects GCC/gcov.")
    endif()

    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
endif()

set(CPU_VOLUME_1_RUNTIME_OUTPUT_DIR
    "${CMAKE_BINARY_DIR}/bin/cpu-volume-1"
    CACHE PATH "Runtime output directory for CPU Volume 1 lab executables"
)

if(CPU_VOLUME_1_BUILD_LABS)
    enable_testing()
    add_subdirectory(labs/lab00_benchmark_foundation)
endif()
