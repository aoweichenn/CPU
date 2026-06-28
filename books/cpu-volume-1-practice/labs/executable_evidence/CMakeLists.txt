cmake_minimum_required(VERSION 3.31)
project(CPU1ExecutableEvidence LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../cmake")
include(BookLabDependencies)
book_labs_enable_gtest()
book_labs_enable_benchmark()

add_library(cpu1_executable_evidence STATIC
    src/executable_evidence.cpp
)

target_include_directories(cpu1_executable_evidence
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(cpu1_executable_evidence PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cpu1_executable_evidence PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cpu1ee_cli
    src/main.cpp
)

target_link_libraries(cpu1ee_cli PRIVATE cpu1_executable_evidence)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cpu1ee_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cpu1ee_tests
    tests/executable_evidence_tests.cpp
)

target_link_libraries(cpu1ee_tests
    PRIVATE
        cpu1_executable_evidence
        GTest::gmock_main
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cpu1ee_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME cpu1ee_tests COMMAND $<TARGET_FILE:cpu1ee_tests>)

add_executable(cpu1ee_bench
    bench/executable_evidence_bench.cpp
)

target_link_libraries(cpu1ee_bench
    PRIVATE
        cpu1_executable_evidence
        benchmark::benchmark
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cpu1ee_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()
