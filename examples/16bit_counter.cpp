#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string_view>

enum Colors : uint8_t { WHITE = 0x01 };

inline volatile uint8_t &memory_loc(const uint16_t loc) {
  return *reinterpret_cast<volatile uint8_t *>(loc);
}

inline void poke(const uint16_t loc, const uint8_t value) {
  memory_loc(loc) = value;
}

inline std::uint8_t peek(const std::uint16_t loc) { return memory_loc(loc); }

inline void decrement_border_color() { --memory_loc(0xd020); }

inline void increment_border_color() { ++memory_loc(0xd020); }

inline bool joystick_down() {
  uint8_t joystick_state = memory_loc(0xDC00);
  return (joystick_state & 2) == 0;
}

void use_data(std::array<char, 1024> &data);

inline void puts(uint8_t x, uint8_t y, std::string_view str) {
  const auto start = 0x400 + (y * 40 + x);

  std::memcpy(const_cast<uint8_t *>(&memory_loc(start)), str.data(),
              str.size());
}

inline void putc(uint8_t x, uint8_t y, uint8_t c) {
  const auto start = 0x400 + (y * 40 + x);
  poke(start, c);
}

inline void put_hex(uint8_t x, uint8_t y, uint8_t value) {
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

inline void put_hex(uint8_t x, uint8_t y, uint16_t value) {
  put_hex(x+2,y, static_cast<std::uint8_t>(0xFF & value));
  put_hex(x,y, static_cast<std::uint8_t>(0xFF & (value >> 8)));
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

  //  static constexpr std::array<std::uint8_t, 1000> data{0};
  //  std::memcpy(const_cast<uint8_t*>(&memory_loc(0x400)), data.data(),
  //  data.size());

  /*
    puts(5, 5, "hello world");
    puts(10, 10, "hellooooo world");
*/
  Clock game_clock{};
  

  std::uint16_t counter = 0;

  std::uint8_t y = 15;
  while (true) {
    const auto us_elapsed = game_clock.restart().count();

    put_hex(5, y, us_elapsed);
    put_hex(11, y, counter);

    if (y++ == 20) {
      y = 15;
    }

    ++counter;
    increment_border_color();
  }

  /*
  const auto background_color = [](Colors col) {
    memory_loc(0xd021) = static_cast<uint8_t>(col);
  };

  background_color(Colors::WHITE);

  while(true) {
    if (joystick_down()) {
      increment_border_color();
    } else {
      decrement_border_color();
    }
  }
  */
}
