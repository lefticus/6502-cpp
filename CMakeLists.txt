cmake_minimum_required(VERSION 3.5)

project(x86-to-6502)

add_executable(x86-to-6502 src/main.cpp)

target_compile_features(x86-to-6502 PRIVATE cxx_std_20)

if (CMAKE_COMPILER_IS_GNUCC)
  target_compile_options(x86-to-6502 PRIVATE -Wall -Wextra -Wconversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wcast-qual -Wunused -Woverloaded-virtual -Wpedantic)
endif ()
