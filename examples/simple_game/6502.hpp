#ifndef INC_6502_C_6502_HPP
#define INC_6502_C_6502_HPP

#include <cstdint>


namespace mos6502 {

static volatile std::uint8_t &memory_loc(const std::uint16_t loc)
{
  return *reinterpret_cast<volatile std::uint8_t *>(loc);
}

static void poke(const std::uint16_t loc, const std::uint8_t value) { memory_loc(loc) = value; }

static std::uint8_t peek(const std::uint16_t loc) { return memory_loc(loc); }

}// namespace mos6502

#endif// INC_6502_C_6502_HPP
