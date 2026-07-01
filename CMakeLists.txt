cmake_minimum_required(VERSION 3.20)
project(VaticMatchingEngineOptimized VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -pthread")

add_executable(matching_engine main.cpp MatchingEngine.cpp FastIO.cpp)
add_executable(matching_engine_tests test.cpp MatchingEngine.cpp FastIO.cpp)
add_executable(matching_engine_benchmark benchmark.cpp MatchingEngine.cpp FastIO.cpp)