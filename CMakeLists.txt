cmake_minimum_required(VERSION 3.31)
project(TechnicalBooks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(book_workspace main.cpp)

option(BOOKS_BUILD_CPU_VOLUME_1 "Build CPU Volume 1 labs" ON)
option(BOOKS_BUILD_ALGORITHM_INTERVIEW "Build algorithm interview book labs" ON)

if(BOOKS_BUILD_CPU_VOLUME_1)
    enable_testing()
    add_subdirectory(books/cpu-volume-1)
endif()

if(BOOKS_BUILD_ALGORITHM_INTERVIEW)
    enable_testing()
    add_subdirectory(books/algorithm-interview)
endif()
