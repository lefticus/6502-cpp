#ifndef INC_6502_C_PETSCII_HPP
#define INC_6502_C_PETSCII_HPP

#include "geometry.hpp"
#include <algorithm>
#include <array>

namespace petscii {


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
    return match_count(graphic, p) == (graphic.width() * graphic.height());
  }
};

template<geometry::size size_> struct ColoredGraphic
{
  Graphic<size_> data;
  Graphic<size_> colors;
};


[[nodiscard]] constexpr char charToPETSCII(char c) noexcept
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
}// namespace petscii


#endif// INC_6502_C_PETSCII_HPP
