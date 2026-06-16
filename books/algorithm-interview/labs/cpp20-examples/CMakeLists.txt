add_library(algorithm_book_examples STATIC
    src/array_patterns.cpp
    src/graph_patterns.cpp
)

target_include_directories(algorithm_book_examples
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(algorithm_book_examples PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(algorithm_book_examples PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(algorithm_book_tests
    tests/algorithm_book_tests.cpp
)

target_link_libraries(algorithm_book_tests PRIVATE algorithm_book_examples)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(algorithm_book_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME algorithm_book_tests COMMAND $<TARGET_FILE:algorithm_book_tests>)
