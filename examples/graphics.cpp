#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <variant>
#include <functional>

#include "chargen.hpp"

enum struct Colors : std::uint8_t {
  black = 0,
  white = 1,
  red = 2,
  cyan = 3,
  violet = 4,
  green = 5,
  blue = 6,
  yellow = 7,
  orange = 8,
  brown = 9,
  light_red = 10,
  dark_grey = 11,
  grey = 12,
  light_green = 13,
  light_blue = 14,
  light_grey = 15
};

constexpr char charToPETSCII(char c) noexcept
{
  if (c >= 'A' && c <= 'Z') { return c - 'A' + 1; }
  return c;
}

template<std::size_t Size> constexpr auto PETSCII(const char (&value)[Size]) noexcept
{
  std::array<char, Size - 1> result{};
  std::transform(std::begin(value), std::prev(std::end(value)), std::begin(result), charToPETSCII);
  return result;
}


static volatile uint8_t &memory_loc(const uint16_t loc) { return *reinterpret_cast<volatile uint8_t *>(loc); }

static void poke(const uint16_t loc, const uint8_t value) { memory_loc(loc) = value; }

static std::uint8_t peek(const std::uint16_t loc) { return memory_loc(loc); }

static void decrement_border_color() { --memory_loc(0xd020); }

static void increment_border_color() { ++memory_loc(0xd020); }


struct Joystick
{
  std::uint8_t state{};

  constexpr bool up() const noexcept { return (state & 1) == 0; }
  constexpr bool left() const noexcept { return (state & 4) == 0; }
  constexpr bool fire() const noexcept { return (state & 16) == 0; }
  constexpr bool right() const noexcept { return (state & 8) == 0; }
  constexpr bool down() const noexcept { return (state & 2) == 0; }
};

static bool joystick_down()
{
  uint8_t joystick_state = peek(0xDC00);
  return (joystick_state & 2) == 0;
}

void use_data(std::array<char, 1024> &data);

static void puts(uint8_t x, uint8_t y, const auto &range, const Colors color = Colors::white)
{
  const auto offset = y * 40 + x;

  const auto start = 0x400 + offset;
  std::copy(begin(range), end(range), &memory_loc(start));

  for (std::uint16_t color_loc = 0; color_loc < range.size(); ++color_loc) {
    poke(0xD800 + color_loc + offset, static_cast<std::uint8_t>(color));
  }
}

template<std::uint8_t Width, std::uint8_t Height> struct Graphic
{
  std::array<std::uint8_t, Width * Height> data{};

  static constexpr auto width() noexcept { return Width; }

  static constexpr auto height() noexcept { return Height; }

  constexpr Graphic() = default;

  constexpr Graphic(std::array<uint8_t, Width * Height> data_) noexcept : data(data_) {}
  constexpr Graphic(std::initializer_list<uint8_t> data_) noexcept { std::copy(begin(data_), end(data_), begin(data)); }

  constexpr auto &operator()(const std::uint8_t x, const std::uint8_t y) noexcept { return data[y * Width + x]; }

  constexpr const auto &operator()(const std::uint8_t x, const std::uint8_t y) const noexcept
  {
    return data[y * Width + x];
  }

  constexpr std::size_t match_count(const auto &graphic, const std::uint8_t x, const std::uint8_t y) const
  {
    std::size_t count = 0;
    for (std::uint8_t cur_x = 0; cur_x < graphic.width(); ++cur_x) {
      for (std::uint8_t cur_y = 0; cur_y < graphic.height(); ++cur_y) {
        if (graphic(cur_x, cur_y) == (*this)(cur_x + x, cur_y + y)) { ++count; }
      }
    }

    return count;
  }


  constexpr bool match(const auto &graphic, const std::uint8_t x, const std::uint8_t y) const
  {
    return match_count(graphic, x, y) == (graphic.width() * graphic.height());
  }
};

template<std::uint8_t Width, std::uint8_t Height> struct ColoredGraphic
{
  Graphic<Width, Height> data;
  Graphic<Width, Height> colors;
};


static constexpr auto load_charset(const std::span<const std::uint8_t, 256 * 8> &bits)
{
  std::array<Graphic<8, 8>, 256> results{};

  for (std::size_t idx = 0; idx < 256; ++idx) {
    Graphic<8, 8> glyph{};

    for (std::uint8_t row = 0; row < 8; ++row) {
      const auto input_row = bits[idx * 8 + row];
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
static constexpr auto from_pixels_to_petscii(const Graphic<InWidth, InHeight> &pixels)
{
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

      result(x / 8, y / 8) = best_match;
    }
  }

  return result;
}


template<std::uint8_t InWidth, std::uint8_t InHeight>
static constexpr auto from_pixels_to_2x2(const Graphic<InWidth, InHeight> &pixels)
{
  Graphic<InWidth / 2, InHeight / 2> result{};

  using GlyphType = std::pair<Graphic<2, 2>, std::uint8_t>;
  constexpr std::array<GlyphType, 17> lookup_map{ GlyphType{ { 0, 0, 0, 0 }, 32 },
    GlyphType{ { 1, 0, 0, 0 }, 126 },
    GlyphType{ { 0, 1, 0, 0 }, 124 },
    GlyphType{ { 1, 1, 0, 0 }, 226 },
    GlyphType{ { 0, 0, 1, 0 }, 123 },
    GlyphType{ { 1, 0, 1, 0 }, 97 },
    GlyphType{ { 0, 1, 1, 0 }, 255 },
    GlyphType{ { 1, 1, 1, 0 }, 236 },
    GlyphType{ { 0, 0, 0, 1 }, 108 },
    GlyphType{ { 1, 0, 0, 1 }, 127 },
    GlyphType{ { 0, 1, 0, 1 }, 225 },
    GlyphType{ { 1, 1, 0, 1 }, 251 },
    GlyphType{ { 0, 0, 1, 1 }, 98 },
    GlyphType{ { 1, 0, 1, 1 }, 252 },
    GlyphType{ { 0, 1, 1, 1 }, 254 },
    GlyphType{ { 1, 1, 1, 1 }, 160 } };


  for (uint8_t x = 0; x < pixels.width(); x += 2) {
    for (uint8_t y = 0; y < pixels.height(); y += 2) {
      for (const auto &glyph : lookup_map) {
        if (pixels.match(glyph.first, x, y)) {
          result(x / 2, y / 2) = glyph.second;
          break;// go to next Y, we found our match
        }
      }
    }
  }

  return result;
}

static void putc(uint8_t x, uint8_t y, uint8_t c, Colors color)
{
  const auto offset = (y * 40 + x);
  const auto start = 0x400 + offset;
  poke(start, c);
  poke(offset + 0xD800, static_cast<std::uint8_t>(color));
}

static std::uint8_t loadc(uint8_t x, uint8_t y)
{
  const auto start = 0x400 + (y * 40 + x);
  return peek(start);
}

static void invertc(uint8_t x, uint8_t y)
{
  const auto start = 0x400 + (y * 40 + x);
  memory_loc(start) += 128;
}


static void put_hex(uint8_t x, uint8_t y, uint8_t value, Colors color)
{
  const auto put_nibble = [color](auto x, auto y, uint8_t nibble) {
    if (nibble <= 9) {
      putc(x, y, nibble + 48, color);
    } else {
      putc(x, y, nibble - 9, color);
    }
  };

  put_nibble(x + 1, y, 0xF & value);
  put_nibble(x, y, 0xF & (value >> 4));
}

static void put_hex(uint8_t x, uint8_t y, uint16_t value, Colors color)
{
  put_hex(x + 2, y, static_cast<std::uint8_t>(0xFF & value), color);
  put_hex(x, y, static_cast<std::uint8_t>(0xFF & (value >> 8)), color);
}

static void put_graphic(uint8_t x, uint8_t y, const auto &graphic)
{
  for (uint8_t cur_y = 0; cur_y < graphic.height(); ++cur_y) {
    for (uint8_t cur_x = 0; cur_x < graphic.width(); ++cur_x) {
      putc(cur_x + x, cur_y + y, graphic(cur_x, cur_y), Colors::white);
    }
  }
}

static void put_graphic(uint8_t x, uint8_t y, const auto &graphic) requires requires { graphic.colors(0, 0); }
{
  for (uint8_t cur_y = 0; cur_y < graphic.data.height(); ++cur_y) {
    for (uint8_t cur_x = 0; cur_x < graphic.data.width(); ++cur_x) {
      putc(cur_x + x, cur_y + y, graphic.data(cur_x, cur_y), static_cast<Colors>(graphic.colors(cur_x, cur_y)));
    }
  }
}


struct Clock
{
  using milliseconds = std::chrono::duration<std::uint16_t, std::milli>;

  // return elapsed time since last restart
  [[nodiscard]] milliseconds restart() noexcept
  {
    // stop Timer A
    poke(0xDC0E, 0b00000000);

    // last value
    const auto previous_value = static_cast<std::uint16_t>(peek(0xDC04) | (static_cast<uint16_t>(peek(0xDC05)) << 8));

    // reset timer
    poke(0xDC04, 0xFF);
    poke(0xDC05, 0xFF);

    // restart timer A
    poke(0xDC0E, 0b00010001);

    return milliseconds{ 0xFFFF - previous_value };
  }

  Clock() noexcept { [[maybe_unused]] const auto value = restart(); }
};

static void cls()
{
  for (std::uint16_t i = 0x400; i < 0x400 + 1000; ++i) { poke(i, 32); }
}

template<std::uint8_t Width, std::uint8_t Height> struct SimpleSprite
{
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  bool is_shown = false;
  Graphic<Width, Height> graphic;
  Graphic<Width, Height> saved_background{};

  constexpr SimpleSprite(std::initializer_list<uint8_t> data_) noexcept : graphic(data_) {}
};


// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts...
{
  using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct Screen
{
  void load(std::uint8_t x, std::uint8_t y, auto &s)
  {
    for (std::uint8_t cur_y = 0; cur_y < s.height(); ++cur_y) {
      for (std::uint8_t cur_x = 0; cur_x < s.width(); ++cur_x) { s(cur_x, cur_y) = loadc(cur_x + x, cur_y + y); }
    }
  }

  void hide(auto &s)
  {
    if (s.is_shown) {
      put_graphic(s.x, s.y, s.saved_background);
      s.is_shown = false;
    }
  }

  void show(std::uint8_t x, std::uint8_t y, auto &s)
  {
    if (s.is_shown) { put_graphic(s.x, s.y, s.saved_background); }

    s.is_shown = true;

    s.x = x;
    s.y = y;
    load(x, y, s.saved_background);

    put_graphic(x, y, s.graphic);
  }
};

template<std::uint8_t Width, std::uint8_t Height> struct Map;

struct GameState
{

  std::uint8_t endurance{ 10 };
  std::uint8_t stamina{ max_stamina() };
  std::uint16_t cash{ 100 };
  std::uint8_t x = 20;
  std::uint8_t y = 12;

  bool redraw = true;

  Clock game_clock{};

  Map<10, 5> const *current_map = nullptr;

  constexpr void set_current_map(const Map<10, 5> &new_map)
  {
    current_map = &new_map;
    redraw = true;
  }

  constexpr void execute_actions(std::uint8_t new_x, std::uint8_t new_y, const auto &character) noexcept
  {
    if (new_x + character.width() > 40) { new_x = x; }

    if (new_y + character.height() > 20) { new_y = y; }

    x = new_x;
    y = new_y;

    if (current_map) {
      for (auto &action : current_map->actions) {
        action.execute_if_collision(x, y, character.width(), character.height(), *this);
      }
    }
  }

  constexpr std::uint8_t max_stamina() const noexcept { return endurance * 5; }

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

  Event next_event() noexcept
  {
    const auto new_joystick_2_state = peek(0xDC00);

    if (new_joystick_2_state != last_joystick_2_state) {
      last_joystick_2_state = new_joystick_2_state;
      return JoyStick2StateChanged{ Joystick{ new_joystick_2_state } };
    }

    return TimeElapsed{ game_clock.restart() };
  }
};

struct Map_Action
{
  std::uint8_t x{};
  std::uint8_t y{};

  std::uint8_t width{};
  std::uint8_t height{};

  using Action_Func = void (*)(GameState &);
  Action_Func action = nullptr;

  constexpr void execute_if_collision(std::uint8_t obj_x,
    std::uint8_t obj_y,
    std::uint8_t obj_width,
    std::uint8_t obj_height,
    GameState &game) const
  {
    if (action == nullptr) { return; }

    const std::uint8_t rect1_x1 = x;
    const std::uint8_t rect1_x2 = x + width;
    const std::uint8_t rect1_y1 = y;
    const std::uint8_t rect1_y2 = y + height;

    const std::uint8_t rect2_x1 = obj_x;
    const std::uint8_t rect2_x2 = obj_x + obj_width;
    const std::uint8_t rect2_y1 = obj_y;
    const std::uint8_t rect2_y2 = obj_y + obj_height;

    if (rect1_x1 < rect2_x2 && rect1_x2 > rect2_x1 && rect1_y1 < rect2_y2 && rect1_y2 > rect2_y1) { action(game); }
  }
};


template<std::uint8_t Width, std::uint8_t Height> struct Map
{
  std::string_view name;
  Graphic<Width, Height> layout;

  std::span<const Map_Action> actions;
};

  void draw_box(uint8_t x, uint8_t y, uint8_t width, uint8_t height, Colors color) {
    putc(x, y, 85, color);
    putc(x + width - 1, y, 73, color);
    putc(x + width - 1, y + height - 1, 75, color);
    putc(x, y + height - 1, 74, color);

    for (uint8_t cur_x = x + 1; cur_x < x + width - 1; ++cur_x) {
      putc(cur_x, y, 67, color);
      putc(cur_x, y + height - 1, 67, color);
    }

    for (uint8_t cur_y = y + 1; cur_y < y + height - 1; ++cur_y) {
      putc(x, cur_y, 93, color);
      putc(x + width - 1, cur_y, 93, color);
    }
  }



int main()
{
  // static constexpr auto charset = load_charset(uppercase);

  // clang-format off
  static constexpr auto inn = Graphic<6,5> {
    32,233,160,160,223,32,
    233,160,160,160,160,223,
    160,137,142,142,160,160,
    160,160,160,160,79,160,
    160,160,160,160,76,160,
  };

  static constexpr auto gym = Graphic<6,5> {
    32,233,160,160,223,32,
    233,160,160,160,160,223,
    160,135,153,141,160,160,
    160,160,160,160,79,160,
    160,160,160,160,76,160,
  };

  static constexpr auto trading_post = Graphic<6,5> {
    32,233,160,160,223,32,
    233,160,160,160,160,223,
    148,146,129,132,133,160,
    160,160,160,160,79,160,
    160,160,160,160,76,160,
  };

/*
  static constexpr auto town = Graphic<4, 4>{ 
    85, 67, 67, 73,
    93, 233, 223, 93,
    93, 160, 160, 93,
    74, 67, 67, 75 };
*/

  static constexpr auto town = ColoredGraphic<4, 4>{
    { 
      32, 32,32, 32,
      233,223,233,223,
      224,224,224,224,
      104,104,104,104
    },
    {
      2,2,10,10,
      4,4,7,7,
      4,4,7,7,
      11,11,11,11
    }
  };


  static constexpr auto mountain = Graphic<4, 4>{ 
    32, 78, 77, 32,
    32, 32, 78, 77,
    78, 77, 32, 32,
    32, 78, 77, 32 };

  static constexpr auto colored_mountain = ColoredGraphic<4, 4>{ 
    {
    32, 78, 77, 32,
    32, 32, 233, 223,
    233, 223, 32, 32,
    32, 78, 77, 32 },
    {
      1, 9, 9, 1,
      1, 1, 8, 8,
      9, 9, 1, 1,
      1, 8, 8, 1

    }
  };


  auto character = SimpleSprite<2, 3>{ 
    32, 87,
    78, 79,
    78, 77 };

  static constexpr auto city_map = Map<10, 5>{
    "wood town",
    {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 4, 0, 0, 0, 0, 6, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    std::span<const Map_Action>{}
  };


  static constexpr auto overview_actions = std::array {
    Map_Action { 16,0,4,4, [](GameState &g) { g.set_current_map(city_map); } }
  };

  static constexpr auto overview_map = Map<10, 5>{
    "the world",
    {
      3, 1, 1, 0, 3, 0, 0, 0, 0, 0,
      0, 0, 1, 1, 0, 0, 0, 0, 3, 0,
      0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 3, 0, 0, 0, 0, 0, 0, 0,
      0, 1, 1, 1, 0, 0, 3, 0, 0, 0,
    },
    std::span<const Map_Action>(begin(overview_actions), end(overview_actions))
  };
  // clang-format on


  static constexpr std::array<void (*)(std::uint8_t, std::uint8_t), 7> tile_types{
    [](std::uint8_t x, std::uint8_t y) {
      /* do nothing for 0th */
    },
    [](std::uint8_t x, std::uint8_t y) { put_graphic(x, y, colored_mountain); },
    [](std::uint8_t x, std::uint8_t y) {
      /* do nothing for 2 */
    },
    [](std::uint8_t x, std::uint8_t y) { put_graphic(x, y, town); },
    [](std::uint8_t x, std::uint8_t y) { put_graphic(x, y, inn); },
    [](std::uint8_t x, std::uint8_t y) { put_graphic(x, y, gym); },
    [](std::uint8_t x, std::uint8_t y) { put_graphic(x, y, trading_post); },
  };


  const auto draw_map = [](const auto &map) {
    for (std::size_t tile = 0; tile < tile_types.size(); ++tile) {
      for (std::uint8_t map_large_y = 0; map_large_y < map.height(); ++map_large_y) {
        for (std::uint8_t map_large_x = 0; map_large_x < map.width(); ++map_large_x) {
          if (map(map_large_x, map_large_y) == tile) { tile_types[tile](map_large_x * 4, map_large_y * 4); }
        }
      }
    }
  };

  GameState game;
  game.current_map = &overview_map;

  constexpr auto show_stats = [](const auto &cur_game) {
    puts(1, 21, PETSCII("STAMINA:"), Colors::light_grey);
    put_hex(12, 21, cur_game.stamina, Colors::white);
    put_hex(15, 21, cur_game.max_stamina(), Colors::white);
    puts(14, 21, PETSCII("/"), Colors::light_grey);
    puts(1, 22, PETSCII("ENDURANCE:"), Colors::light_grey);
    put_hex(12, 22, cur_game.endurance, Colors::white);
    puts(1, 23, PETSCII("CASH:"), Colors::light_grey);
    put_hex(12, 23, cur_game.cash, Colors::white);
  };

  Screen screen;

  static constexpr auto menu_options =
    std::array {
      std::string_view{"info"},
      std::string_view{"test2"},
      std::string_view{"test3"},
      std::string_view{"an even longer thing"}
  };
  struct Menu {
    consteval Menu(std::span<const std::string_view> t_options)
      : options{t_options},         width{std::max_element(begin(options), end(options), 
            [](std::string_view lhs, std::string_view rhs) {
              return lhs.size() < rhs.size();
            }
            )->size() + 2},
      height{options.size() + 2},
      x{(40 - width) / 2},
      y{(20 - height) / 2}
    {

    }

    void highlight(std::uint8_t selection) {
      const std::uint8_t cur_y = selection + 1 + y;
      for (auto cur_x = 1; cur_x < width - 1; ++cur_x) {
        invertc(x + cur_x, cur_y);
      }
    }

    void unhighlight(std::uint8_t selection)
    {
      highlight(selection);
    }

    void hide(GameState &game) {
      displayed = false;
      game.redraw = true;
    }

    bool show(GameState &game, std::uint8_t &selection) {
      if (!displayed) {
        displayed = true;

        draw_box(x, y, width, height, Colors::white);

        for (auto cur_y = y + 1; const auto &str : options) {
          puts(x+1, cur_y++, str, Colors::grey);
        }

        highlight(current_selection);
      }

      if (current_selection != next_selection) {
        unhighlight(current_selection);
        highlight(next_selection);
        current_selection = next_selection;
      }

      if (selected) {
        selected = false;
        selection = current_selection;
        return true;
      } else {
        return false;
      }
    }

    bool process_event(const GameState::Event &e) {
      if (not displayed) {
        return false;
      }

      if (const auto *ptr = std::get_if<GameState::JoyStick2StateChanged>(&e); ptr) {
        if (ptr->state.up()) {
          next_selection = current_selection - 1;
        }

        if (ptr->state.down()) {
          next_selection = current_selection + 1;
        }

        if (next_selection > options.size() - 1) {
          next_selection = options.size() - 1;
        }

        if (ptr->state.fire()) {
          selected = true;
        }

        return true;
      }

      return false;
    }

    std::span<const std::string_view> options;
    std::uint8_t width;
    std::uint8_t height;
    std::uint8_t x;
    std::uint8_t y;
    std::uint8_t current_selection{0};
    std::uint8_t next_selection{0};
    bool selected{false};
    bool displayed{false};

  };

  Menu m(menu_options);

  bool show_game_menu = false;

  auto eventHandler = overloaded{ [&](const GameState::JoyStick2StateChanged &e) {
                                   auto new_x = game.x;
                                   auto new_y = game.y;

                                   if (e.state.fire()) {
                                     show_game_menu = true;
                                     return;
                                   }

                                   if (e.state.up()) { --new_y; }
                                   if (e.state.down()) { ++new_y; }
                                   if (e.state.left()) { --new_x; }
                                   if (e.state.right()) { ++new_x; }


                                   game.execute_actions(new_x, new_y, character.graphic);

                                   screen.show(game.x, game.y, character);

                                   put_hex(36, 1, e.state.state, Colors::dark_grey);
                                 },
    [](const GameState::TimeElapsed &e) { put_hex(36, 0, e.us.count(), Colors::dark_grey); } };

  while (true) {
    const auto next_event = game.next_event();

    if (not m.process_event(next_event)) {
      // if no gui elements needed the event, then we handle it
      std::visit(eventHandler, next_event);
    }

    if (game.redraw) {
      screen.hide(character);
      cls();

      poke(53280, 0);
      poke(53281, 0);

      game.redraw = false;
      draw_map(game.current_map->layout);
      draw_box(0, 20, 40, 5, Colors::dark_grey);

      puts(10, 20, game.current_map->name, Colors::white);
      show_stats(game);
      screen.show(game.x, game.y, character);
    }

    if (std::uint8_t result = 0; show_game_menu && m.show(game, result)) {
      // we had a menu item selected
      m.hide(game);
      show_game_menu = false;
    }




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
