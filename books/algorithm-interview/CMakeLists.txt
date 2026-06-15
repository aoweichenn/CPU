cmake_minimum_required(VERSION 3.31)
project(AlgorithmInterviewBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ALGORITHM_INTERVIEW_BUILD_LABS "Build algorithm interview book labs" ON)

if(ALGORITHM_INTERVIEW_BUILD_LABS)
    enable_testing()
    add_subdirectory(labs/cpp20-examples)
endif()
