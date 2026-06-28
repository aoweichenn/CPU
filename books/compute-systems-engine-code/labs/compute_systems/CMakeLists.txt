add_library(cse_compsys STATIC
    src/chapter_models.cpp
    src/parallel_reduce.cpp
    src/futex_lab.cpp
    src/sync_primitives.cpp
    src/task_runtime.cpp
    src/wait_channels.cpp
)

target_include_directories(cse_compsys
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(cse_compsys PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cse_compsys PRIVATE -Wall -Wextra -Wpedantic)
endif()

function(cse_add_compsys_executable target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} PRIVATE cse_compsys)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(cse_add_compsys_probe test_name source_file pass_regex)
    cse_add_compsys_executable(${test_name} ${source_file})
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
    set_tests_properties(${test_name} PROPERTIES
        PASS_REGULAR_EXPRESSION "${pass_regex}"
    )
endfunction()

cse_add_compsys_executable(cse_compsys_demo src/main.cpp)
cse_add_compsys_executable(cse_compsys_tests tests/compsys_tests.cpp)
add_test(NAME cse_compsys_tests COMMAND $<TARGET_FILE:cse_compsys_tests>)

cse_add_compsys_probe(
    cse_compsys_chapter_models_probe
    src/chapter_models_probe.cpp
    "chapter_models,21"
)
cse_add_compsys_probe(
    cse_compsys_wait_channels_probe
    src/wait_channels_probe.cpp
    "contract,2,1,1,2,2,3,3,1,1"
)
cse_add_compsys_probe(
    cse_compsys_task_runtime_probe
    src/task_runtime_probe.cpp
    "task_runtime,3,3,3,0,1,5,5,15,"
)
cse_add_compsys_probe(
    cse_compsys_futex_lab_probe
    src/futex_lab_probe.cpp
    "futex_mutex,"
)
