#include <catch2/catch.hpp>

#include <fmt/format.h>
#include <fstream>

enum struct OptimizationLevel : char { O0 = '0', O1 = '1', O2 = '2', O3 = '3', Os = 's' };

enum struct Optimize6502 : char { Enabled = '1', Disabled = '0' };

std::vector<std::uint8_t> execute_c64_program(const std::string_view &name,
  const std::string_view script,
  OptimizationLevel o,
  Optimize6502 o6502,
  std::uint16_t start_address_dump,
  std::uint16_t end_address_dump)
{

  const char *x64_executable = std::getenv("X64");
  REQUIRE(x64_executable != nullptr);
  const char *mos6502_cpp_executable = std::getenv("CXX_6502");
  REQUIRE(mos6502_cpp_executable != nullptr);

  const auto optimization_level = [&]() -> std::string_view {
    switch (o) {
    case OptimizationLevel::Os: return "-Os";
    case OptimizationLevel::O1: return "-O1";
    case OptimizationLevel::O2: return "-O2";
    case OptimizationLevel::O3: return "-O3";
    case OptimizationLevel::O0: return "-O0";
    }

    return "unknown";
  }();

  const auto optimize_6502 = [&]() -> std::string_view {
    switch (o6502) {
    case Optimize6502::Enabled: return "--optimize=1";
    case Optimize6502::Disabled: return "--optimize=0";
    }

    return "unknown";
  }();

  const auto optimize_6502_name = [&]() -> std::string_view {
    switch (o6502) {
    case Optimize6502::Enabled: return "-optimize";
    case Optimize6502::Disabled: return "-no-optimize";
    }

    return "unknown";
  }();

  const auto source_filename{ fmt::format("{}{}{}.cpp", name, optimization_level, optimize_6502_name) };
  const auto vice_script_filename{ fmt::format("{}{}{}-vice_script", name, optimization_level, optimize_6502_name) };
  const auto prg_filename{ fmt::format("{}{}{}.prg", name, optimization_level, optimize_6502_name) };
  const auto ram_dump_filename{ fmt::format("{}{}{}-ram_dump", name, optimization_level, optimize_6502_name) };


  {
    std::ofstream source(source_filename);
    source << script;
  }

  {
    std::ofstream vice_script(vice_script_filename);
    vice_script << fmt::format(
      R"(
until e5d1
l "{}" 0
keybuf run\n
until e147
bsave "{}" 0 {:x} {:x}
quit
)",
      prg_filename,
      ram_dump_filename,
      start_address_dump,
      end_address_dump);
  }


  REQUIRE(system(fmt::format(
            "{} {} -t C64 {} {}", mos6502_cpp_executable, source_filename, optimization_level, optimize_6502)
                   .c_str())
          == EXIT_SUCCESS);
  REQUIRE(
    system(fmt::format(
      "xvfb-run -d {} +vsync -sounddev dummy +saveres -warp -moncommands {}", x64_executable, vice_script_filename)
             .c_str())
    == EXIT_SUCCESS);

  std::ifstream memory_dump(ram_dump_filename, std::ios::binary);

  std::vector<char> data;
  data.resize(static_cast<std::size_t>(end_address_dump - start_address_dump + 1));
  memory_dump.read(data.data(), std::ssize(data));

  std::vector<std::uint8_t> return_value{ data.begin(), data.end() };
  return return_value;
}

TEMPLATE_TEST_CASE_SIG("Can write to memory",
  "",
  ((OptimizationLevel O), O),
  OptimizationLevel::Os,
  OptimizationLevel::O0,
  OptimizationLevel::O1,
  OptimizationLevel::O2,
  OptimizationLevel::O3)
{
  constexpr static std::string_view program =
    R"(
int main()
{
  *reinterpret_cast<volatile unsigned char *>(0x400) = 10;
}
)";

  const auto result = execute_c64_program("write_to_memory", program, O, Optimize6502::Enabled, 0x400, 0x400);

  REQUIRE(result.size() == 1);
  CHECK(result[0] == 10);
}

TEMPLATE_TEST_CASE_SIG("Can write to memory via function call",
  "",
  ((OptimizationLevel O), O),
  OptimizationLevel::Os,
  OptimizationLevel::O0,
  OptimizationLevel::O1,
  OptimizationLevel::O2,
  OptimizationLevel::O3)
{
  constexpr static std::string_view program =
    R"(

void poke(unsigned int location, unsigned char value) {
  *reinterpret_cast<volatile unsigned char *>(location) = value;
}

int main()
{
  poke(0x400, 10);
  poke(0x401, 11);
}

)";

  const auto result =
    execute_c64_program("write_to_memory_via_function", program, O, Optimize6502::Enabled, 0x400, 0x401);

  REQUIRE(result.size() == 2);

  CHECK(result[0] == 10);
  CHECK(result[1] == 11);
}


TEMPLATE_TEST_CASE_SIG("Can execute loop > 256",
  "",
  ((OptimizationLevel O), O),
  OptimizationLevel::Os,
  OptimizationLevel::O0,
  OptimizationLevel::O1,
  OptimizationLevel::O2,
  OptimizationLevel::O3)
{
  constexpr static std::string_view program =
    R"(

int main()
{
  for (unsigned short i = 0x400; i < 0x400 + 1000; ++i) {
    *reinterpret_cast<volatile unsigned char *>(i) = 32;
  }

//  while (true) {
    // don't allow main to exit, otherwise we get READY. on the screen
//  }
}

)";

  const auto result = execute_c64_program("execute_long_loop_cls", program, O, Optimize6502::Enabled, 0x400, 0x7E7);

  REQUIRE(result.size() == 1000);

  CHECK(std::all_of(begin(result), end(result), [](const auto b) { return b == 32; }));
}

TEMPLATE_TEST_CASE_SIG("Write to 2D Array",
  "",
  ((OptimizationLevel O, Optimize6502 O6502), O, O6502),
  (OptimizationLevel::Os, Optimize6502::Disabled),
  (OptimizationLevel::Os, Optimize6502::Enabled),
  (OptimizationLevel::O0, Optimize6502::Disabled),
  (OptimizationLevel::O0, Optimize6502::Enabled),
  (OptimizationLevel::O1, Optimize6502::Enabled),
  (OptimizationLevel::O2, Optimize6502::Enabled),
  (OptimizationLevel::O3, Optimize6502::Disabled),
  (OptimizationLevel::O3, Optimize6502::Enabled))
{
  constexpr static std::string_view program =
    R"(

void poke(unsigned int location, unsigned char value) {
  *reinterpret_cast<volatile unsigned char *>(location) = value;
}

void putc(unsigned char x, unsigned char y, unsigned char c) {
  const auto start = 0x400 + (y * 40 + x);
  poke(start, c);
}


int main()
{
  for (unsigned char y = 0; y < 25; ++y) {
    for (unsigned char x = 0; x < 40; ++x) {
      putc(x, y, y);
    }
  }

//  while (true) {
    // don't allow main to exit, otherwise we get READY. on the screen
//  }
}

)";

  const auto result = execute_c64_program("write_to_2d_array", program, O, O6502, 0x400, 0x7E7);

  REQUIRE(result.size() == 1000);

  for (std::size_t x = 0; x < 40; ++x) {
    for (std::size_t y = 0; y < 25; ++y) { CHECK(result[y * 40 + x] == y); }
  }
}
