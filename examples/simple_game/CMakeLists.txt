
# Note: this is compiling for the host OS, so not really useful
# but it is handy as a sanity check
add_executable(game game.cpp geometry.hpp)

target_link_libraries(
        game
        PRIVATE project_options
        project_warnings)

target_compile_options(game PRIVATE -Wno-volatile)
