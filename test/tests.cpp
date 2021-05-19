#include <catch2/catch.hpp>

#include <fmt/format.h>
#include <fstream>

enum struct OptimizationLevel : char { O0='0', O1='1', O2='2', O3='3', Os='s' };


std::vector<std::uint8_t> execute_c64_program(const std::string_view &name,
  const std::string_view script,
  [[maybe_unused]] OptimizationLevel o,
  std::uint16_t start_address_dump,
  std::uint16_t end_address_dump)
{

  const char *x64_executable = std::getenv("X64");
  REQUIRE(x64_executable != nullptr);
  const char *mos6502_cpp_executable = std::getenv("CXX_6502");
  REQUIRE(mos6502_cpp_executable != nullptr);

  const auto optimization_level = [&]() -> std::string_view {
    switch(o) {
    case OptimizationLevel::Os: return "-Os";
    case OptimizationLevel::O1: return "-O1";
    case OptimizationLevel::O2: return "-O2";
    case OptimizationLevel::O3: return "-O3";
    case OptimizationLevel::O0: return "-O0";
    }

    return "unknown";
  }();
  const auto source_filename{ fmt::format("{}{}.cpp", name, optimization_level) };
  const auto vice_script_filename{fmt::format("{}{}-vice_script", name, optimization_level)};
  const auto prg_filename{ fmt::format("{}{}.prg", name, optimization_level) };
  const auto ram_dump_filename{ fmt::format("{}{}-ram_dump", name, optimization_level) };


  {
    std::ofstream source(source_filename);
    source << script;
  }

  {
    std::ofstream vice_script(vice_script_filename);
    vice_script << fmt::format(
      R"(
z 100000
l "{}" 0
keybuf run\n
z 100000
bsave "{}" 0 {:x} {:x}
quit
)",
      prg_filename,
      ram_dump_filename,
      start_address_dump,
      end_address_dump);
  }


  REQUIRE(system(fmt::format("{} -f {} -t C64 {}", mos6502_cpp_executable, source_filename, optimization_level).c_str()) == EXIT_SUCCESS);
  REQUIRE(system(fmt::format("xvfb-run {} +saveres -warp -moncommands {}", x64_executable, vice_script_filename).c_str()) == EXIT_SUCCESS);

  std::ifstream memory_dump(ram_dump_filename, std::ios::binary);

  std::vector<char> data;
  data.resize(static_cast<std::size_t>(end_address_dump - start_address_dump + 1));
  memory_dump.read(data.data(), std::ssize(data));

  std::vector<std::uint8_t> return_value{ data.begin(), data.end() };
  return return_value;
}

TEST_CASE("Can write to screen memory")
{
  constexpr static std::string_view program =
    R"(
int main()
{
  *reinterpret_cast<volatile unsigned char *>(0x400) = 10;
}
)";

  const auto o_level = GENERATE(OptimizationLevel::O0, OptimizationLevel::O1, OptimizationLevel::O2, OptimizationLevel::O3, OptimizationLevel::Os);
  const auto result = execute_c64_program("write_to_screen_memory", program, o_level, 0x400, 0x400);

  REQUIRE(result.size() == 1);
  CHECK(result[0] == 10);
}

TEST_CASE("Can write to screen memory via function call")
{
  constexpr static std::string_view program =
    R"(

void poke(unsigned int location, unsigned char value) {
  *reinterpret_cast<volatile unsigned char *>(location) = 10;
}

int main()
{
  poke(0x400, 10);
  poke(0x401, 11);
}
)";

  const auto o_level = GENERATE(
    OptimizationLevel::O0, OptimizationLevel::O1, OptimizationLevel::O2, OptimizationLevel::O3, OptimizationLevel::Os);
  const auto result = execute_c64_program("write_to_screen_memory_via_function", program, o_level, 0x400, 0x401);

  REQUIRE(result.size() == 2);
  CHECK(result[0] == 10);
  CHECK(result[0] == 11);

}
