cmake_minimum_required(VERSION 3.5)

set (CMAKE_CXX_STANDARD 14)

project(x86-to-6502)

if(CMAKE_COMPILER_IS_GNUCC)
  add_definitions(-Wall -Wextra -Wconversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wcast-qual -Wunused -Woverloaded-virtual  -pedantic -std=c++14)
endif()

add_executable(x86-to-6502 src/main.cpp)


