#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string_view>

enum Colors : uint8_t { WHITE = 0x01 };

static volatile uint8_t &memory_loc(const uint16_t loc) {
  return *reinterpret_cast<volatile uint8_t *>(loc);
}

static void poke(const uint16_t loc, const uint8_t value) {
  memory_loc(loc) = value;
}

static std::uint8_t peek(const std::uint16_t loc) { return memory_loc(loc); }

static void puts(uint8_t x, uint8_t y, std::string_view str) {
  const auto start = 0x400 + (y * 40 + x);

  std::memcpy(const_cast<uint8_t *>(&memory_loc(start)), str.data(),
              str.size());
}

int main() {
  puts(15, 10, "hello commodore!");
}

