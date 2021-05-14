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

static void decrement_border_color() { --memory_loc(0xd020); }

static void increment_border_color() { ++memory_loc(0xd020); }

static bool joystick_down() {
  uint8_t joystick_state = peek(0xDC00);
  return (joystick_state & 2) == 0;
}

void use_data(std::array<char, 1024> &data);

static void puts(uint8_t x, uint8_t y, std::string_view str) {
  const auto start = 0x400 + (y * 40 + x);

  std::memcpy(const_cast<uint8_t *>(&memory_loc(start)), str.data(),
              str.size());
}


template<std::uint8_t Width, std::uint8_t Height>
struct Graphic
{
  std::array<std::uint8_t, Width * Height> data;

  static constexpr auto width() noexcept {
    return Width;
  }

  static constexpr auto height() noexcept {
    return Height;
  }

  constexpr Graphic() = default;

  constexpr Graphic(std::array<uint8_t, Width * Height> data_) noexcept : data(data_) {}
  constexpr Graphic(std::initializer_list<uint8_t> data_) noexcept {
    std::copy(begin(data_), end(data_), begin(data));
  }

  constexpr auto &operator()(const std::uint8_t x, const std::uint8_t y) noexcept {
    return data[y * Width + x];
  }

  constexpr const auto &operator()(const std::uint8_t x, const std::uint8_t y) const noexcept {
    return data[y * Width + x];
  }
};

static void putc(uint8_t x, uint8_t y, uint8_t c) {
  const auto start = 0x400 + (y * 40 + x);
  poke(start, c);
}

static void put_hex(uint8_t x, uint8_t y, uint8_t value) {
  const auto put_nibble = [](auto x, auto y, uint8_t nibble) {
    if (nibble <= 9) {
      putc(x, y, nibble + 48);
    } else {
      putc(x, y, nibble - 9);
    }
  };

  put_nibble(x + 1, y, 0xF & value);
  put_nibble(x, y, 0xF & (value >> 4));
}

static void put_hex(uint8_t x, uint8_t y, uint16_t value) {
  put_hex(x+2,y, static_cast<std::uint8_t>(0xFF & value));
  put_hex(x,y, static_cast<std::uint8_t>(0xFF & (value >> 8)));
}

static void put_graphic(uint8_t x, uint8_t y, const auto &graphic)
{
  for (uint8_t cur_y = 0; cur_y < graphic.height(); ++cur_y) {
    for (uint8_t cur_x = 0; cur_x < graphic.width(); ++cur_x) {
      putc(cur_x + x, cur_y + y, graphic(cur_x, cur_y));
    }
  }
}

static void cls() {
  for (std::uint16_t i = 0x400; i < 0x400 + 1000; ++i) {
    poke(i, 32);
  }
}

struct Clock {
  using milliseconds = std::chrono::duration<std::uint16_t, std::milli>;

  // return elapsed time since last restart
  [[nodiscard]] milliseconds restart() {
    // stop Timer A
    poke(0xDC0E, 0b00000000);

    // last value
    const auto previous_value = static_cast<std::uint16_t>(
        peek(0xDC04) | (static_cast<uint16_t>(peek(0xDC05)) << 8));

    // reset timer
    poke(0xDC04, 0xFF);
    poke(0xDC05, 0xFF);

    // restart timer A
    poke(0xDC0E, 0b00010001);

    return milliseconds{0xFFFF - previous_value};
  }

  Clock() { [[maybe_unused]] const auto value = restart(); }
};

int main() {
  cls();

  auto fib = [f0 = 0u, f1 = 1u] mutable {
    f0 = std::exchange(f1, f0 + f1);
    return f0;
  };

  for (std::uint8_t y = 0; y < 25; ++y) {
    put_hex(30, y, fib());
  }
}
