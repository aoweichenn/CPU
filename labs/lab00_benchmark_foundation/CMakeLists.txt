add_library(lab00_benchmark STATIC
    src/benchmark.cpp
    src/kernels.cpp
)

target_include_directories(lab00_benchmark
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(lab00_benchmark PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lab00_benchmark PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lab00_bench
    src/main.cpp
)

target_link_libraries(lab00_bench PRIVATE lab00_benchmark)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lab00_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lab00_bad_benchmarks
    src/bad_benchmarks.cpp
)

target_link_libraries(lab00_bad_benchmarks PRIVATE lab00_benchmark)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lab00_bad_benchmarks PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lab00_tests
    tests/lab00_tests.cpp
)

target_link_libraries(lab00_tests PRIVATE lab00_benchmark)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lab00_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME lab00_tests COMMAND lab00_tests)
