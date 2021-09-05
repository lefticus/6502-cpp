#ifndef INC_6502_C_PETSCII_HPP
#define INC_6502_C_PETSCII_HPP

#include "geometry.hpp"
#include <algorithm>
#include <array>
#include <span>

namespace petscii {

enum named_chars : std::uint8_t {
  closed_circle = 81
};

template<geometry::size size_> struct Graphic
{
  std::array<std::uint8_t, size_.height * size_.width> data{};

  static constexpr const auto &size() noexcept { return size_; }

  constexpr Graphic() = default;

  constexpr explicit Graphic(std::array<uint8_t, size_.height * size_.width> data_) noexcept : data(data_) {}
  constexpr Graphic(std::initializer_list<uint8_t> data_) noexcept { std::copy(begin(data_), end(data_), begin(data)); }

  constexpr auto &operator[](const geometry::point p) noexcept
  {
    return data[static_cast<std::size_t>(p.y * size_.width + p.x)];
  }

  constexpr const auto &operator[](const geometry::point p) const noexcept { return data[static_cast<std::size_t>(p.y * size_.width + p.x)]; }

  constexpr std::size_t match_count(const auto &graphic, const geometry::point start_point) const
  {
    std::size_t count = 0;

    for (const auto &point : graphic.size()) {
      if (graphic[point] == (*this)[point + start_point]) { ++count; }
    }

    return count;
  }


  constexpr bool match(const auto &graphic, const geometry::point p) const
  {
    return match_count(graphic, p) == (graphic.size().width * graphic.size().height);
  }
};

template<geometry::size size_> struct ColoredGraphic
{
  Graphic<size_> data;
  Graphic<size_> colors;
};


[[nodiscard]] constexpr char charToPETSCII2(char c) noexcept
{
  if (c >= 'a' && c <= 'z') { return c - 'a' + 1; }
  if (c >= 'A' && c <= 'Z') { return c - 'A' + 65; }
  if (c == '@') { return 0; }
  if (c =='_') { return 100; }
  return c;
}


[[nodiscard]] constexpr char charToPETSCII(char c) noexcept
{
  if (c == '@') { return 0; }
  if (c >= 'A' && c <= 'Z') { return c - 'A' + 1; }
  return c;
}

template<std::size_t Size> constexpr auto PETSCII2(const char (&value)[Size]) noexcept
{
  std::array<char, Size - 1> result{};
  std::transform(std::begin(value), std::prev(std::end(value)), std::begin(result), charToPETSCII2);
  return result;
}
template<std::size_t Size> constexpr auto PETSCII(const char (&value)[Size]) noexcept
{
  std::array<char, Size - 1> result{};
  std::transform(std::begin(value), std::prev(std::end(value)), std::begin(result), charToPETSCII);
  return result;
}

static constexpr auto load_charset(std::span<const std::uint8_t, 256 * 8> bits)
{
  std::array<petscii::Graphic<geometry::size{ 8, 8 }>, 256> results{};

  for (std::size_t idx = 0; idx < 256; ++idx) {
    petscii::Graphic<geometry::size{ 8, 8 }> glyph{};

    for (std::uint8_t row = 0; row < 8; ++row) {
      const auto input_row = bits[idx * 8 + row];
      glyph[geometry::point{ 0, row }] = (0b1000'0000 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 1, row }] = (0b0100'0000 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 2, row }] = (0b0010'0000 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 3, row }] = (0b0001'0000 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 4, row }] = (0b0000'1000 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 5, row }] = (0b0000'0100 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 6, row }] = (0b0000'0010 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 7, row }] = (0b0000'0001 & input_row) == 0 ? 0 : 1;
    }

    results[idx] = glyph;
  }

  return results;
}

template<geometry::size Size> static constexpr auto from_pixels_to_2x2(const petscii::Graphic<Size> &pixels)
{
  petscii::Graphic<geometry::size{ Size.width / 2, Size.height / 2 }> result{};

  using GlyphType = std::pair<petscii::Graphic<geometry::size{ 2, 2 }>, std::uint8_t>;
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


  for (uint8_t x = 0; x < pixels.size().width; x += 2) {
    for (uint8_t y = 0; y < pixels.size().height; y += 2) {
      for (const auto &glyph : lookup_map) {
        if (pixels.match(glyph.first, {x, y})) {
          result[geometry::point{static_cast<std::uint8_t>(x / 2), static_cast<std::uint8_t>(y / 2)}] = glyph.second;
          break;// go to next Y, we found our match
        }
      }
    }
  }

  return result;
}



}// namespace petscii


#endif// INC_6502_C_PETSCII_HPP
