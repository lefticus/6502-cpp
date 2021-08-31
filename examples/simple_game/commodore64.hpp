#ifndef INC_6502_C_C64_HPP
#define INC_6502_C_C64_HPP

#include <algorithm>
#include <array>
#include <chrono>

#include "6502.hpp"
#include "petscii.hpp"
#include "vicii.hpp"

namespace c64 {

struct Joystick
{
  std::uint8_t state{};

  [[nodiscard]] constexpr bool up() const noexcept { return (state & 1) == 0; }
  [[nodiscard]] constexpr bool left() const noexcept { return (state & 4) == 0; }
  [[nodiscard]] constexpr bool fire() const noexcept { return (state & 16) == 0; }
  [[nodiscard]] constexpr bool right() const noexcept { return (state & 8) == 0; }
  [[nodiscard]] constexpr bool down() const noexcept { return (state & 2) == 0; }

  [[nodiscard]] constexpr bool operator==(const Joystick &other) const = default;
  [[nodiscard]] constexpr bool operator!=(const Joystick &other) const = default;
};

struct Clock
{
  using milliseconds = std::chrono::duration<std::uint16_t, std::milli>;
  static constexpr std::uint16_t TimerAControl = 0xDC0E;
  static constexpr std::uint16_t TimerALowByte = 0xDC04;
  static constexpr std::uint16_t TimerAHighByte = 0xDC05;


  // return elapsed time since last restart
  [[nodiscard]] milliseconds restart() noexcept
  {
    // stop Timer A
    mos6502::poke(TimerAControl, 0b00000000);

    // last value
    const auto previous_value = static_cast<std::uint16_t>(
      mos6502::peek(TimerALowByte) | (static_cast<uint16_t>(mos6502::peek(TimerAHighByte)) << 8));

    // reset timer
    mos6502::poke(TimerALowByte, 0xFF);
    mos6502::poke(TimerAHighByte, 0xFF);

    // restart timer A
    mos6502::poke(TimerAControl, 0b00010001);

    return milliseconds{ 0xFFFF - previous_value };
  }

  Clock() noexcept { [[maybe_unused]] const auto value = restart(); }
};
}// namespace c64

#endif// INC_6502_C_C64_HPP
