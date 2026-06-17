add_library(compsys STATIC
    src/parallel_reduce.cpp
)

target_include_directories(compsys
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(compsys PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(compsys PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(compsys_demo
    src/main.cpp
)

target_link_libraries(compsys_demo PRIVATE compsys)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(compsys_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(compsys_tests
    tests/compsys_tests.cpp
)

target_link_libraries(compsys_tests PRIVATE compsys)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(compsys_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME compsys_tests COMMAND $<TARGET_FILE:compsys_tests>)
