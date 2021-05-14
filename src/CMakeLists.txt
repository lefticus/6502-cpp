option(CPP_STARTER_USE_SDL "Enable compilation of SDL sample" OFF)

# sdl
if(CPP_STARTER_USE_SDL)
  message("Using SDL2")
  add_subdirectory(sdl)
endif()

# Generic test that uses conan libs
add_executable(6502-c++ 6502-c++.cpp)
target_link_libraries(
  6502-c++
  PRIVATE project_options
          project_warnings
          CONAN_PKG::ctre
          CONAN_PKG::cli11
          CONAN_PKG::fmt
          CONAN_PKG::spdlog)

target_include_directories(6502-c++
        PRIVATE "${CMAKE_SOURCE_DIR}")