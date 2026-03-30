cmake_minimum_required(VERSION 3.31)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(lowl_audio_bench_stub)
    message(FATAL_ERROR
            "Configure benchmarks from the repository root, not from bench/.\n"
            "Example:\n"
            "  cmake -S /path/to/lowl_audio -B /path/to/lowl_audio/build-bench "
            "-DCMAKE_BUILD_TYPE=Release -DLOWL_BUILD_BENCHMARKS=ON")
endif ()

set(LOWL_BENCH_MAIN lowl_audio_bench)
set(LOWL_BENCH_FILES
        bench_audio_device.cpp
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
set(BENCHMARK_FORCE_WERROR OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/benchmark
                 ${CMAKE_CURRENT_BINARY_DIR}/third_party/benchmark
                 EXCLUDE_FROM_ALL)

add_executable(${LOWL_BENCH_MAIN} ${LOWL_BENCH_FILES})
target_link_libraries(${LOWL_BENCH_MAIN} PRIVATE ${LOWL_LIB_MAIN} benchmark::benchmark_main)
