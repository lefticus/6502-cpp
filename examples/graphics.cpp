#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <span>
#include <variant>

#include "chargen.hpp"

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


struct Joystick
{
  std::uint8_t state{};

  constexpr bool up() const noexcept {
    return (state & 1) == 0;
  }
  constexpr bool left() const noexcept {
    return (state & 4) == 0;
  }
  constexpr bool fire() const noexcept {
    return (state & 16) == 0;
  }
  constexpr bool right() const noexcept {
    return (state & 8) == 0;
  }
  constexpr bool down() const noexcept {
    return (state & 2) == 0;
  }
};

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
  std::array<std::uint8_t, Width * Height> data{};

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

  constexpr std::size_t match_count(const auto &graphic, const std::uint8_t x, const std::uint8_t y) const
  {
    std::size_t count = 0;
    for (std::uint8_t cur_x = 0; cur_x < graphic.width(); ++cur_x) {
      for (std::uint8_t cur_y = 0; cur_y < graphic.height(); ++cur_y) {
        if (graphic(cur_x, cur_y) == (*this)(cur_x + x, cur_y + y)) {
          ++count;
        }
      }
    }

    return count;
  }


  constexpr bool match(const auto &graphic, const std::uint8_t x, const std::uint8_t y) const
  {
    return match_count(graphic, x, y) == (graphic.width() * graphic.height());
  }


};


static constexpr auto load_charset(const std::span<const std::uint8_t, 256*8> &bits) {
  std::array<Graphic<8,8>, 256> results{};
  
  for (std::size_t idx = 0; idx < 256; ++idx) {
    Graphic<8,8> glyph{};

    for (std::uint8_t row = 0; row < 8; ++row) {
      const auto input_row = bits[idx*8+row];
      glyph(0, row) = (0b1000'0000 & input_row) == 0 ? 0 : 1;
      glyph(1, row) = (0b0100'0000 & input_row) == 0 ? 0 : 1;
      glyph(2, row) = (0b0010'0000 & input_row) == 0 ? 0 : 1;
      glyph(3, row) = (0b0001'0000 & input_row) == 0 ? 0 : 1;
      glyph(4, row) = (0b0000'1000 & input_row) == 0 ? 0 : 1;
      glyph(5, row) = (0b0000'0100 & input_row) == 0 ? 0 : 1;
      glyph(6, row) = (0b0000'0010 & input_row) == 0 ? 0 : 1;
      glyph(7, row) = (0b0000'0001 & input_row) == 0 ? 0 : 1;
    }

    results[idx] = glyph;
  }

  return results;
}

  template<std::uint8_t InWidth, std::uint8_t InHeight>
  static constexpr auto from_pixels_to_petscii(const Graphic<InWidth, InHeight> &pixels) {
    Graphic<InWidth / 8, InHeight / 8> result{};

    constexpr auto charset = load_charset(uppercase);

    for (uint8_t x = 0; x < pixels.width(); x += 8) {
      for (uint8_t y = 0; y < pixels.height(); y += 8) {
        std::uint8_t best_match = 32;
        std::size_t match_count = 0;

        std::uint8_t cur_char = 0;
        for (const auto &glyph : charset) {

          const auto count = pixels.match_count(glyph, x, y);
          if (count > match_count) {
            best_match = cur_char;
            match_count = count;
          }

          ++cur_char;
        }

        result(x/8, y/8) = best_match;
      }
    }

    return result;
  }


  template<std::uint8_t InWidth, std::uint8_t InHeight>
  static constexpr auto from_pixels_to_2x2(const Graphic<InWidth, InHeight> &pixels) {
    Graphic<InWidth / 2, InHeight / 2> result{};

    using GlyphType = std::pair<Graphic<2,2>, std::uint8_t>;
    constexpr std::array<GlyphType, 17> lookup_map {
      GlyphType{ {0,0,
         0,0}, 32 },
      GlyphType{ {1,0,
         0,0}, 126 },
      GlyphType{ {0,1,
         0,0}, 124 },
      GlyphType{ {1,1,
         0,0}, 226 },
      GlyphType{ {0,0,
         1,0}, 123 },
      GlyphType{ {1,0,
         1,0}, 97 },
      GlyphType{ {0,1,
         1,0}, 255 },
      GlyphType{ {1,1,
         1,0}, 236 },
      GlyphType{ {0,0,
         0,1}, 108 },
      GlyphType{ {1,0,
         0,1}, 127 },
      GlyphType{ {0,1,
         0,1}, 225 },
      GlyphType{ {1,1,
         0,1}, 251 },
      GlyphType{ {0,0,
         1,1}, 98 },
      GlyphType{ {1,0,
         1,1}, 252 },
      GlyphType{ {0,1,
         1,1}, 254 },
      GlyphType{ {1,1,
         1,1}, 160 }
    };


    for (uint8_t x = 0; x < pixels.width(); x += 2) {
      for (uint8_t y = 0; y < pixels.height(); y += 2) {
        for (const auto &glyph : lookup_map) {
          if (pixels.match(glyph.first, x, y)) {
            result(x/2, y/2) = glyph.second;
            break; // go to next Y, we found our match
          }
        }
      }
    }
    
    return result;
  }

static void putc(uint8_t x, uint8_t y, uint8_t c) {
  const auto start = 0x400 + (y * 40 + x);
  poke(start, c);
}

static std::uint8_t loadc(uint8_t x, uint8_t y) {
  const auto start = 0x400 + (y * 40 + x);
  return peek(start);
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
  [[nodiscard]] milliseconds restart() noexcept {
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

  Clock() noexcept { [[maybe_unused]] const auto value = restart(); }
};

static void cls() {
  for (std::uint16_t i = 0x400; i < 0x400 + 1000; ++i) {
    poke(i, 32);
  }
}

template<std::uint8_t Width, std::uint8_t Height>
struct SimpleSprite
{
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  bool is_shown = false;
  Graphic<Width, Height> graphic;
  Graphic<Width, Height> saved_background{};

  constexpr SimpleSprite(std::initializer_list<uint8_t> data_) noexcept 
    : graphic(data_)
  {
  }
};


// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct Screen
{
  void load(std::uint8_t x, std::uint8_t y, auto &s) {
    for (std::uint8_t cur_y = 0; cur_y < s.height(); ++cur_y) {
      for (std::uint8_t cur_x = 0; cur_x < s.width(); ++cur_x) {
        s(cur_x, cur_y) = loadc(cur_x + x, cur_y + y);
      }
    }
  }

  void show(std::uint8_t x, std::uint8_t y, auto &s)  {
    if (s.is_shown) {
      put_graphic(s.x, s.y, s.saved_background);
    }

    s.is_shown = true;

    s.x = x;
    s.y = y;
    load(x, y, s.saved_background);

    put_graphic(x,y,s.graphic);
  }
};

int main() {
  //static constexpr auto charset = load_charset(uppercase);

  static constexpr auto town =
    Graphic<4,4> {
      85,67,67,73,
      93,233,223,93,
      93,160,160,93,
      74,67,67,75};

  static constexpr auto mountain =
    Graphic<4,4> {
      32,78,77,32,
      32,32,78,77,
      78,77,32,32,
      32,78,77,32
    };


  auto character =
    SimpleSprite<2,3> {
      32,87,
      78,79,
      78,77
    };


  static constexpr auto overview_map = 
    Graphic<10, 5> {
      3,1,1,0,3,0,0,0,0,0,
      0,0,1,1,0,0,0,0,3,0,
      0,0,1,0,0,0,0,0,0,0,
      0,3,0,0,0,0,0,0,0,0,
      0,1,1,1,0,0,3,0,0,0,
    };

  

  cls();

  poke(53280,0);
  poke(53281,0);

  static constexpr std::array<void (*)(std::uint8_t, std::uint8_t), 4> tile_types{
    [](std::uint8_t x, std::uint8_t y) {
      /* do nothing for 0th */
    },
    [](std::uint8_t x, std::uint8_t y) {
      put_graphic(x,y,mountain);
    },
    [](std::uint8_t x, std::uint8_t y) {
      /* do nothing for 2 */
    },
    [](std::uint8_t x, std::uint8_t y) {
      put_graphic(x,y,town);
    },
  };


  const auto draw_map = [](const auto &map) {
    for (std::size_t tile = 0; tile < tile_types.size(); ++tile) {
      for (std::uint8_t map_large_y =0; map_large_y < map.height(); ++map_large_y) {
        for (std::uint8_t map_large_x = 0; map_large_x < map.width(); ++map_large_x) {
          if (map(map_large_x,map_large_y) == tile) {
            tile_types[tile](map_large_x*4,map_large_y*4);
          }
        }
      }
    }
  };

  constexpr auto draw_box = [](uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
    putc(x,y, 85);
    putc(x+width-1,y, 73);
    putc(x+width-1,y+height-1,75);
    putc(x,y+height-1,74);

    for (uint8_t cur_x = x+1; cur_x < x+width-1; ++cur_x) {
      putc(cur_x, y, 67);
      putc(cur_x, y+height-1, 67);
    }

    for (uint8_t cur_y = y+1; cur_y < y+height-1; ++cur_y) {
      putc(x, cur_y, 93);
      putc(x+width-1, cur_y, 93);
    }
  };


  struct GameState {

    std::uint8_t endurance{10};
    std::uint8_t stamina{max_stamina()};
    std::uint16_t cash{100};

    Clock game_clock{};

    constexpr std::uint8_t max_stamina() const noexcept {
      return endurance * 5;
    }

    struct JoyStick1StateChanged
    {
      Joystick state;
    };


    struct JoyStick2StateChanged
    {
      Joystick state;
    };

    struct TimeElapsed
    {
      Clock::milliseconds us;
    };

    using Event = std::variant<JoyStick2StateChanged, TimeElapsed>;

    std::uint8_t last_joystick_2_state = peek(0xDC00);

    Event next_event() noexcept {
      const auto new_joystick_2_state = peek(0xDC00);

      if (new_joystick_2_state != last_joystick_2_state) {
        last_joystick_2_state = new_joystick_2_state;
        return JoyStick2StateChanged{Joystick{new_joystick_2_state}};
      }

      return TimeElapsed{game_clock.restart()};
    }
  };

  GameState game;

//  draw_map(map1);
  draw_map(overview_map);
  draw_box(0,20,40,5);

  constexpr auto show_stats = [](const auto &cur_game) {
    puts(1, 21, "stamina:");
    put_hex(12, 21, cur_game.stamina);
    put_hex(15, 21, cur_game.max_stamina());
    puts(14,21, "/");
    puts(1, 22, "endurance:");
    put_hex(12,22, cur_game.endurance);
    puts(1,23, "cash: ");
    put_hex(12,23, cur_game.cash);
  };

  Screen screen;

  show_stats(game);

  std::uint8_t x = 20;
  std::uint8_t y = 12;

  auto eventHandler = overloaded {
    [&](const GameState::JoyStick2StateChanged &e) {
      if (e.state.up()) {
        --y;
      }
      if (e.state.down()) {
        ++y;
      }
      if (e.state.left()) {
        --x;
      }
      if (e.state.right()) {
        ++x;
      }
      screen.show(x, y, character);

      put_hex(36, 1, e.state.state);
    },
    [](const GameState::TimeElapsed &e) {
      put_hex(36, 0, e.us.count());
    }
  };

  while (true) {
    std::visit(eventHandler, game.next_event());

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
