add_library(lab00_benchmark STATIC
    src/benchmark.cpp
    src/kernels.cpp
)

set(LAB00_RUNTIME_OUTPUT_DIR "${CPU_VOLUME_1_RUNTIME_OUTPUT_DIR}/lab00")

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
set_target_properties(lab00_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${LAB00_RUNTIME_OUTPUT_DIR}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lab00_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lab00_bad_benchmarks
    src/bad_benchmarks.cpp
)

target_link_libraries(lab00_bad_benchmarks PRIVATE lab00_benchmark)
set_target_properties(lab00_bad_benchmarks PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${LAB00_RUNTIME_OUTPUT_DIR}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lab00_bad_benchmarks PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lab00_tests
    tests/lab00_tests.cpp
)

target_link_libraries(lab00_tests PRIVATE lab00_benchmark)
set_target_properties(lab00_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${LAB00_RUNTIME_OUTPUT_DIR}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lab00_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME lab00_tests COMMAND lab00_tests)
