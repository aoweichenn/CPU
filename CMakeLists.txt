cmake_minimum_required(VERSION 3.31)
project(TechnicalBooks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(book_workspace main.cpp)

option(BOOKS_BUILD_CPU_VOLUME_1 "Build CPU Volume 1 labs" ON)
option(BOOKS_BUILD_CPU_VOLUME_1_PRACTICE "Build CPU Volume 1 practice labs" ON)
option(BOOKS_BUILD_CPU_VOLUME_2 "Build CPU Volume 2 labs" ON)
option(BOOKS_BUILD_CPU_VOLUME_3 "Build CPU Volume 3 labs" ON)
option(BOOKS_BUILD_ALGORITHM_INTERVIEW "Build algorithm interview book labs" ON)
option(BOOKS_BUILD_CPP_ZERO_TO_ADVANCED "Build Cpp zero-to-advanced labs" ON)
option(BOOKS_BUILD_COMPUTE_SYSTEMS_ENGINE_CODE "Build Compute Systems Engine code labs" ON)

if(BOOKS_BUILD_CPU_VOLUME_1)
    enable_testing()
    add_subdirectory(books/cpu-volume-1)
endif()

if(BOOKS_BUILD_CPU_VOLUME_1_PRACTICE)
    enable_testing()
    add_subdirectory(books/cpu-volume-1-practice)
endif()

if(BOOKS_BUILD_CPU_VOLUME_2)
    enable_testing()
    add_subdirectory(books/cpu-volume-2)
endif()

if(BOOKS_BUILD_CPU_VOLUME_3)
    enable_testing()
    add_subdirectory(books/cpu-volume-3)
endif()

if(BOOKS_BUILD_ALGORITHM_INTERVIEW)
    enable_testing()
    add_subdirectory(books/algorithm-interview)
endif()

if(BOOKS_BUILD_CPP_ZERO_TO_ADVANCED)
    enable_testing()
    add_subdirectory(books/cpp-zero-to-advanced)
endif()

if(BOOKS_BUILD_COMPUTE_SYSTEMS_ENGINE_CODE)
    enable_testing()
    add_subdirectory(books/compute-systems-engine-code)
endif()
