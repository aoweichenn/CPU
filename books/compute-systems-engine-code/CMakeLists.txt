cmake_minimum_required(VERSION 3.31)
project(ComputeSystemsEngineCode LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(COMPUTE_SYSTEMS_ENGINE_CODE_BUILD_LABS "Build Compute Systems Engine code labs" ON)

if(COMPUTE_SYSTEMS_ENGINE_CODE_BUILD_LABS)
    enable_testing()
    add_subdirectory(labs/reference_pipeline)
endif()
