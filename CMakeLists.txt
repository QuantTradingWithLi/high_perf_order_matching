cmake_minimum_required(VERSION 3.20)
project(VaticMatchingEngineOptimized VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(ENABLE_NATIVE_OPTIMIZATIONS "Build with -march=native for the current CPU" ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread")
if(ENABLE_NATIVE_OPTIMIZATIONS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

add_executable(matching_engine main.cpp MatchingEngine.cpp FastIO.cpp)
add_executable(matching_engine_tests test.cpp MatchingEngine.cpp FastIO.cpp)
add_executable(matching_engine_benchmark benchmark.cpp MatchingEngine.cpp FastIO.cpp)

enable_testing()

add_test(NAME matching_engine_tests COMMAND matching_engine_tests)

add_executable(matching_engine_differential_tests
    differential_test.cpp MatchingEngine.cpp FastIO.cpp)
add_test(NAME matching_engine_differential_tests
    COMMAND matching_engine_differential_tests)

add_executable(matching_engine_bitmap_tests price_bitmap_test.cpp)
add_test(NAME matching_engine_bitmap_tests
    COMMAND matching_engine_bitmap_tests)
