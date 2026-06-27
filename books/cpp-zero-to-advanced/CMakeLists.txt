cmake_minimum_required(VERSION 3.31)
project(CppZeroToAdvanced LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPP_ZERO_TO_ADVANCED_BUILD_LABS "Build Cpp zero-to-advanced labs" ON)

if(CPP_ZERO_TO_ADVANCED_BUILD_LABS)
    enable_testing()
    add_subdirectory(labs/cpp_practice_workbook)
endif()
