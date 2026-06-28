cmake_minimum_required(VERSION 3.31)
project(CPUVolume1Practice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPU_VOLUME_1_PRACTICE_BUILD_LABS "Build CPU Volume 1 practice labs" ON)

if(CPU_VOLUME_1_PRACTICE_BUILD_LABS)
    enable_testing()
    add_subdirectory(labs/executable_evidence)
endif()
