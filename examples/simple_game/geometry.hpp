#ifndef INC_6502_C_GEOMETRY_HPP
#define INC_6502_C_GEOMETRY_HPP

#include <cstdint>
#include <compare>

namespace geometry {
struct point
{
  std::uint8_t x;
  std::uint8_t y;

  [[nodiscard]] constexpr auto operator<=>(const point &other) const = default;
};

constexpr point operator+(const point &lhs, const point &rhs) noexcept
{
  return point{ static_cast<std::uint8_t>(lhs.x + rhs.x), static_cast<std::uint8_t>(lhs.y + rhs.y) };
}

struct size
{
  std::uint8_t width;
  std::uint8_t height;
  [[nodiscard]] constexpr bool operator==(const size &other) const = default;
  [[nodiscard]] constexpr bool operator!=(const size &other) const = default;
};

struct rect
{
  [[nodiscard]] constexpr std::uint8_t left() const noexcept { return tl.x; }
  [[nodiscard]] constexpr std::uint8_t top() const noexcept { return tl.y; }
  [[nodiscard]] constexpr std::uint8_t bottom() const noexcept
  {
    return static_cast<std::uint8_t>(tl.y + size_.height);
  }
  [[nodiscard]] constexpr std::uint8_t right() const noexcept { return static_cast<std::uint8_t>(tl.x + size_.width); }

  [[nodiscard]] constexpr const point &top_left() const noexcept { return tl; }
  [[nodiscard]] constexpr point bottom_right() const noexcept { return point{ right(), bottom() }; }
  [[nodiscard]] constexpr point bottom_left() const noexcept { return point{ left(), bottom() }; }
  [[nodiscard]] constexpr point top_right() const noexcept { return point{ right(), top() }; }
  [[nodiscard]] constexpr std::uint8_t width() const noexcept { return size_.width; }
  [[nodiscard]] constexpr std::uint8_t height() const noexcept { return size_.height; }

  [[nodiscard]] constexpr const auto &size() const noexcept { return size_; }

  // returns a rectangle of this size, but centered in the screen (40x20)
  [[nodiscard]] constexpr auto centered() const noexcept
  {
    return rect{
      { static_cast<std::uint8_t>((40 - size_.width) / 2), static_cast<std::uint8_t>((20 - size_.height) / 2) }, size_
    };
  }

  [[nodiscard]] constexpr bool intersects(const rect &other) const noexcept
  {
    const auto my_tl = top_left();
    const auto my_br = bottom_right();

    const auto other_tl = other.top_left();
    const auto other_br = other.bottom_right();

    return my_tl.x < other_br.x && my_br.x > other_tl.x && my_tl.y < other_br.y && my_br.y > other_tl.y;
  };

  point tl;
  geometry::size size_;
};

struct point_in_size
{
  point p;
  size s;

  constexpr auto &operator++() noexcept
  {
    ++p.x;
    if (p.x == s.width) {
      p.x = 0;
      ++p.y;
    }
    return *this;
  }
  [[nodiscard]] constexpr bool operator==(const point_in_size &other) const = default;
  [[nodiscard]] constexpr bool operator!=(const point_in_size &other) const = default;
  [[nodiscard]] constexpr const point &operator*() const { return p; }
};


constexpr auto begin(const size s) { return point_in_size{ { 0, 0 }, s }; }

constexpr auto end(const size s) { return point_in_size{ { 0, s.height }, s }; }
}// namespace geometry

#endif// INC_6502_C_GEOMETRY_HPP
