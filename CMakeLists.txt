cmake_minimum_required(VERSION 3.12.1)

SET (CMAKE_C_COMPILER "/usr/bin/clang-9")

SET (CMAKE_CXX_COMPILER "/usr/bin/clang++-9")
SET (CMAKE_CXX_FLAGS    "-Wall -std=c++17 -lpthread")

project(queues_processor)

set(SRC_LIST main.cpp
             queues_processor.hpp)

add_executable(main ${SRC_LIST})
