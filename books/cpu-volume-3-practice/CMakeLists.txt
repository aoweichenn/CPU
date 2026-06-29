cmake_minimum_required(VERSION 3.31)
project(CPUVolume3Practice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()
add_subdirectory(../cpu-volume-3/labs/linux_cpu_inference linux_cpu_inference)
