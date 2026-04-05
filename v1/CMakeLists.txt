cmake_minimum_required(VERSION 3.10)
project(MemoryPool)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -O2")

include_directories(include)

add_executable(mempool_test src/MemoryPool.cpp main.cpp)
# mkdir build
# cd build
# cmake ..
# make
# ./mempool_test
