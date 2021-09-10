#ifndef INC_6502_C_CPP_X16
#define INC_6502_C_CPP_X16

#include <6502.hpp>
#include <geometry.hpp>
#include <span>
#include <petscii.hpp>

namespace x16::vera {
static constexpr std::uint16_t ADDR_L = 0x9f20;
static constexpr std::uint16_t ADDR_M = 0x9f21;
static constexpr std::uint16_t ADDR_H = 0x9f22;
static constexpr std::uint16_t DATA0 = 0x9f23;

// this assumes we are already in a text mode, initialized by the kernal
static void puts(const geometry::point &loc, std::span<const char> str) {
  mos6502::poke(ADDR_L, loc.x * 2);
  mos6502::poke(ADDR_M, loc.y);
  mos6502::poke(ADDR_H, 0x20); // auto increment by 2 after each poke of the character, we are skipping the attribute byte

  for (const auto c : str) {
    mos6502::poke(DATA0, c);
  }
}
}

#endif


