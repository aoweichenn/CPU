cmake_minimum_required(VERSION 3.31)
project(CseReferencePipeline LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../cmake")
include(BookLabDependencies)
book_labs_enable_gtest()
book_labs_enable_benchmark()

add_library(cse_reference_pipeline STATIC
    src/reference_pipeline.cpp
)

target_include_directories(cse_reference_pipeline
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(cse_reference_pipeline PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cse_reference_pipeline PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cse_reference_demo
    src/main.cpp
)

target_link_libraries(cse_reference_demo PRIVATE cse_reference_pipeline)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cse_reference_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cse_reference_tests
    tests/reference_pipeline_tests.cpp
)

target_link_libraries(cse_reference_tests
    PRIVATE
        cse_reference_pipeline
        GTest::gmock_main
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cse_reference_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME cse_reference_tests COMMAND $<TARGET_FILE:cse_reference_tests>)

add_executable(cse_reference_bench
    bench/reference_pipeline_bench.cpp
)

target_link_libraries(cse_reference_bench
    PRIVATE
        cse_reference_pipeline
        benchmark::benchmark
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cse_reference_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()
