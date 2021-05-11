option(CPP_STARTER_USE_SDL "Enable compilation of SDL sample" OFF)

# sdl
if(CPP_STARTER_USE_SDL)
  message("Using SDL2")
  add_subdirectory(sdl)
endif()

# Generic test that uses conan libs
add_executable(6502-c++ 6502-c++.cpp ../include/optimizer.hpp ../include/assembly.hpp ../include/6502.hpp ../include/personality.hpp ../include/personalities/c64.hpp)
target_link_libraries(
  6502-c++
  PRIVATE project_options
          project_warnings
          CONAN_PKG::ctre
          CONAN_PKG::docopt.cpp
          CONAN_PKG::fmt
          CONAN_PKG::spdlog)
