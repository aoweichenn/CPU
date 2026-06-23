add_library(lcqi STATIC
    src/inference.cpp
    src/int8_kernels.cpp
    src/model.cpp
    src/reference_decoder.cpp
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_sources(lcqi PRIVATE src/int8_kernels_avx2.cpp)
        set_source_files_properties(src/int8_kernels_avx2.cpp PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
        target_compile_definitions(lcqi PRIVATE LCQI_ENABLE_AVX2=1)
    endif()
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_sources(lcqi PRIVATE src/int8_kernels_neon.cpp)
        target_compile_definitions(lcqi PRIVATE LCQI_ENABLE_NEON=1)
    endif()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(WARNING "LCQI is written for the book's local Linux CPU target; current system is ${CMAKE_SYSTEM_NAME}.")
endif()

target_include_directories(lcqi
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(lcqi PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lcqi PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lcqi_cli
    src/main.cpp
)

target_link_libraries(lcqi_cli PRIVATE lcqi)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lcqi_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lcqi_bench
    src/bench.cpp
)

target_link_libraries(lcqi_bench PRIVATE lcqi)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lcqi_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lcqi_tests
    tests/lcqi_tests.cpp
)

target_link_libraries(lcqi_tests PRIVATE lcqi)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lcqi_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME lcqi_tests COMMAND $<TARGET_FILE:lcqi_tests>)
