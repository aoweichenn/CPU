cmake_minimum_required(VERSION 3.31)
project(CppPracticeWorkbook LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../cmake")
include(BookLabDependencies)
book_labs_enable_gtest()
book_labs_enable_benchmark()

add_library(cppbook_practice_workbook STATIC
    src/practice_workbook.cpp
)

target_include_directories(cppbook_practice_workbook
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(cppbook_practice_workbook PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cppbook_practice_workbook PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cppbook_practice_demo
    src/main.cpp
)

target_link_libraries(cppbook_practice_demo PRIVATE cppbook_practice_workbook)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cppbook_practice_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cppbook_practice_tests
    tests/practice_workbook_tests.cpp
)

target_link_libraries(cppbook_practice_tests
    PRIVATE
        cppbook_practice_workbook
        GTest::gmock_main
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cppbook_practice_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME cppbook_practice_tests COMMAND $<TARGET_FILE:cppbook_practice_tests>)

add_executable(cppbook_practice_bench
    bench/practice_workbook_bench.cpp
)

target_link_libraries(cppbook_practice_bench
    PRIVATE
        cppbook_practice_workbook
        benchmark::benchmark
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cppbook_practice_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()
