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
  std::uint8_t x = 0;
  std::uint8_t y = 0;

  while (true) {
    puts(x, y, "hello commodore!");
    x += 3;
    ++y;
    if (x > 26) {
      x = 0;
    }

    if (y>25) {
      y = 0;
    }

  }
}

