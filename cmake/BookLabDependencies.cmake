include(FetchContent)

function(book_labs_enable_gtest)
    if(NOT TARGET GTest::gmock_main)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        if(NOT googletest_POPULATED)
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG v1.17.0
            )
        endif()
        if(NOT TARGET GTest::gmock_main)
            FetchContent_MakeAvailable(googletest)
        endif()
    endif()
endfunction()

function(book_labs_enable_benchmark)
    if(NOT TARGET benchmark::benchmark)
        find_package(benchmark QUIET CONFIG)
    endif()

    if(NOT TARGET benchmark::benchmark)
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
        if(NOT googlebenchmark_POPULATED)
            FetchContent_Declare(
                googlebenchmark
                GIT_REPOSITORY https://github.com/google/benchmark.git
                GIT_TAG v1.9.5
            )
        endif()
        if(NOT TARGET benchmark::benchmark)
            FetchContent_MakeAvailable(googlebenchmark)
        endif()
    endif()
endfunction()
