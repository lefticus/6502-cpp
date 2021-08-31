#ifndef INC_6502_C_VICII_HPP
#define INC_6502_C_VICII_HPP

#include "6502.hpp"
#include "geometry.hpp"
#include <cstdint>
#include <algorithm>
#include <iterator>

namespace vicii {
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


[[maybe_unused]] static void decrement_border_color() { mos6502::poke(0xd020, mos6502::peek(0xd020) - 1); }

// static void increment_border_color() { mos6502::poke(0xd020, mos6502::peek(0xd020) + 1); }

static inline void putc(geometry::point location, std::uint8_t c, Colors color)
{
  const auto offset = static_cast<std::uint16_t>(location.y * 40 + location.x);
  const auto start = static_cast<std::uint16_t>(0x400 + offset);
  mos6502::poke(start, c);
  mos6502::poke(offset + 0xD800, static_cast<std::uint8_t>(color));
}

static inline std::uint8_t getc(geometry::point location)
{
  const auto start = static_cast<std::uint16_t>(0x400 + (location.y * 40 + location.x));
  return mos6502::peek(start);
}

static inline void invertc(geometry::point location)
{
  const auto start = static_cast<std::uint16_t>(0x400 + (location.y * 40 + location.x));
  mos6502::memory_loc(start) += 128;
}

static inline void set_background(const vicii::Colors color) {
  mos6502::poke(53280, static_cast<std::uint8_t>(color));
}

static inline void set_border(const vicii::Colors color) {
  mos6502::poke(53281, static_cast<std::uint8_t>(color));
}


static inline void puts(const geometry::point loc, const auto &range, const vicii::Colors color = vicii::Colors::white)
{
  const auto offset = static_cast<std::uint16_t>(loc.y * 40 + loc.x);

  const std::uint16_t start = 0x400 + offset;

  using namespace std;

  std::copy(begin(range), end(range), &mos6502::memory_loc(start));

  for (std::uint16_t color_loc = 0; color_loc < size(range); ++color_loc) {
    mos6502::poke(static_cast<std::uint16_t>(0xD800 + color_loc + offset), static_cast<std::uint8_t>(color));
  }
}


static inline void put_hex(geometry::point start, uint8_t value, Colors color)
{
  const auto put_nibble = [color](geometry::point location, std::uint8_t nibble) {
    if (nibble <= 9) {
      putc(location, nibble + 48, color);
    } else {
      putc(location, nibble - 9, color);
    }
  };

  put_nibble(start + geometry::point{ 1, 0 }, 0xF & value);
  put_nibble(start , 0xF & (value >> 4));
}

static inline void put_hex(geometry::point location, std::uint16_t value, Colors color)
{
  put_hex(location, static_cast<std::uint8_t>(0xFF & (value >> 8)), color);
  put_hex(location + geometry::point{ 2, 0 }, static_cast<std::uint8_t>(0xFF & value), color);
}

static inline void put_graphic(geometry::point location, const auto &graphic)
{
  for (const auto &p : graphic.size()) { putc(p + location, graphic[p], Colors::white); }
}

static inline void put_graphic(geometry::point location, const auto &graphic) requires requires { graphic.colors[{0, 0}]; }
{
  for (const auto &p : graphic.data.size()) {
    putc(p + location, graphic.data[p], static_cast<Colors>(graphic.colors[p]));
  }
}


static inline void cls()
{
  for (std::uint16_t i = 0x400; i < 0x400 + 1000; ++i) { mos6502::poke(i, 32); }
}

struct Screen
{
  void load(geometry::point location, auto &s)
  {
    for (const auto &p : s.size()) {
      s[p] = getc(p + location);
    }
  }

  void hide(auto &s)
  {
    if (s.is_shown) {
      put_graphic(s.location, s.saved_background);
      s.is_shown = false;
    }
  }

  void show(geometry::point loc, auto &s)
  {
    if (s.is_shown) { put_graphic(s.location, s.saved_background); }

    s.is_shown = true;

    s.location = loc;

    load(loc, s.saved_background);

    put_graphic(loc, s.graphic);
  }
};

static void draw_vline(geometry::point begin, const geometry::point end, Colors c)
{
  while (begin < end) {
    putc(begin, 93, c);
    begin = begin + geometry::point{ 0, 1 };
  }
}

static void draw_hline(geometry::point begin, const geometry::point end, Colors c)
{
  while (begin < end) {
    putc(begin, 67, c);
    begin = begin + geometry::point{1,0};
  }
}

static inline void draw_box(geometry::rect geo, Colors color)
{
  putc(geo.top_left(), 85, color);
  putc(geo.top_right(), 73, color);
  putc(geo.bottom_right(), 75, color);
  putc(geo.bottom_left(), 74, color);

  draw_hline(geo.top_left() + geometry::point{1,0}, geo.top_right(), color);
  draw_hline(geo.bottom_left() + geometry::point{ 1, 0 }, geo.bottom_right(), color);

  draw_vline(geo.top_left() + geometry::point{ 0, 1 }, geo.bottom_left(), color);
  draw_vline(geo.top_right() + geometry::point{ 0, 1 }, geo.bottom_right(), color);
}

static inline void clear(geometry::rect box, Colors color) {
  for (const auto &p : box.size()) {
    putc(p + box.top_left(), ' ', color);
  }
}

}// namespace vicii

#endif// INC_6502_C_VICII_HPP
