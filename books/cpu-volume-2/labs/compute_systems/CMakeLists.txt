add_library(compsys STATIC
    src/chapter_models.cpp
    src/parallel_reduce.cpp
    src/futex_lab.cpp
    src/sync_primitives.cpp
    src/task_runtime.cpp
    src/wait_channels.cpp
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

add_executable(compsys_chapter_models_probe
    src/chapter_models_probe.cpp
)

target_link_libraries(compsys_chapter_models_probe PRIVATE compsys)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(compsys_chapter_models_probe PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME compsys_chapter_models_probe COMMAND $<TARGET_FILE:compsys_chapter_models_probe>)
set_tests_properties(compsys_chapter_models_probe PROPERTIES
    PASS_REGULAR_EXPRESSION "chapter_models,21"
)

add_executable(compsys_wait_channels_probe
    src/wait_channels_probe.cpp
)

target_link_libraries(compsys_wait_channels_probe PRIVATE compsys)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(compsys_wait_channels_probe PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME compsys_wait_channels_probe COMMAND $<TARGET_FILE:compsys_wait_channels_probe>)
set_tests_properties(compsys_wait_channels_probe PROPERTIES
    PASS_REGULAR_EXPRESSION "contract,2,1,1,2,2,3,3,1,1"
)

add_executable(compsys_task_runtime_probe
    src/task_runtime_probe.cpp
)

target_link_libraries(compsys_task_runtime_probe PRIVATE compsys)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(compsys_task_runtime_probe PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME compsys_task_runtime_probe COMMAND $<TARGET_FILE:compsys_task_runtime_probe>)
set_tests_properties(compsys_task_runtime_probe PROPERTIES
    PASS_REGULAR_EXPRESSION "task_runtime,3,3,3,0,1,5,5,15,"
)

add_executable(compsys_futex_lab_probe
    src/futex_lab_probe.cpp
)

target_link_libraries(compsys_futex_lab_probe PRIVATE compsys)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(compsys_futex_lab_probe PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME compsys_futex_lab_probe COMMAND $<TARGET_FILE:compsys_futex_lab_probe>)
set_tests_properties(compsys_futex_lab_probe PROPERTIES
    PASS_REGULAR_EXPRESSION "futex_mutex,"
)
