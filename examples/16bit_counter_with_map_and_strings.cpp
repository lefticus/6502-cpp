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

  static constexpr auto pic = 
    Graphic<5,4>{
      78,119,77,32,32,
      101,32,32,80,32,
      101,79,101,103,32,
      76,101,76,122,88
    };

  static constexpr auto map1 =
    Graphic<4, 2>{
      1,0,1,0,
      1,1,1,1
      };

  static constexpr auto map2 =
    Graphic<6, 3>{
      1,0,1,0,0,0,
      1,1,1,1,0,1,
      0,0,0,1,0,0
    };

/* 
  static constexpr auto map =
    Graphic<10, 2>{
      0,1,0,1,0,0,0,1,0,0,
      1,0,0,0,0,0,1,0,0,0,
    };
*/
 // put_graphic(10,10,pic);


  const auto draw_map = [](const auto &map) {
  for (std::uint8_t y=0; y < map.height(); ++y) {
    for (std::uint8_t x = 0; x < map.width(); ++x) {
      if (map(x, y) == 1) {
        put_graphic(x*4, y*4, pic);
      }
    }
  }
  };

  puts(5, 17, "timing history");
  puts(21, 17, "16bit counter");


//  draw_map(map1);
  draw_map(map2);


  Clock game_clock{};
 

  std::uint16_t counter = 0;
  std::uint8_t y = 19;

  while (true) {
    const auto us_elapsed = game_clock.restart().count();

    put_hex(5, y, us_elapsed);
    put_hex(21, y, counter);

    if (y++ == 24) {
      y = 19;
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
