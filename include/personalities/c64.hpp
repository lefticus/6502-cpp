#ifndef INC_6502_C_C64_HPP
#define INC_6502_C_C64_HPP

#include "../personality.hpp"

struct C64 : Personality
{

  void insert_autostart_sequence(std::vector<mos6502> &new_instructions) const override
  {
    constexpr static auto start_address = 0x0801;

    // first 2 bytes is the load address for a PRG file.
    new_instructions.emplace_back(ASMLine::Type::Directive, ".word " + std::to_string(start_address));
    new_instructions.emplace_back(ASMLine::Type::Directive, "* = " + std::to_string(start_address));
    new_instructions.emplace_back(ASMLine::Type::Directive, "; jmp to start of program with BASIC");
    new_instructions.emplace_back(ASMLine::Type::Directive, ".byt $0B,$08,$0A,$00,$9E,$32,$30,$36,$31,$00,$00,$00");
  }

  [[nodiscard]] Operand get_register(const int reg_num) const override
  {
    switch (reg_num) {
      //  http://sta.c64.org/cbm64mem.html
    case 0: return Operand(Operand::Type::literal, "$4e");// unused, int->fp routine pointer
    case 1: return Operand(Operand::Type::literal, "$4f");
    case 2: return Operand(Operand::Type::literal, "$50");// unused
    case 3: return Operand(Operand::Type::literal, "$51");// unused
    case 4: return Operand(Operand::Type::literal, "$52");// bit buffer for rs232
    case 5: return Operand(Operand::Type::literal, "$53");// counter for rs232
    case 6: return Operand(Operand::Type::literal, "$54");// unused
    case 7: return Operand(Operand::Type::literal, "$55");// unused
    case 8: return Operand(Operand::Type::literal, "$56");// unused
    case 9: return Operand(Operand::Type::literal, "$57");// unused
    case 10: return Operand(Operand::Type::literal, "$58");// Current BASIC line number
    case 11: return Operand(Operand::Type::literal, "$59");// Current BASIC line number
    case 12: return Operand(Operand::Type::literal, "$5a");// arithmetic register #3
    case 13: return Operand(Operand::Type::literal, "$5b");
    case 14: return Operand(Operand::Type::literal, "$5c");
    case 15: return Operand(Operand::Type::literal, "$5d");
    case 16: return Operand(Operand::Type::literal, "$5e");
    case 17: return Operand(Operand::Type::literal, "$5f");
    case 18: return Operand(Operand::Type::literal, "$60");
    case 19: return Operand(Operand::Type::literal, "$61");
    case 20: return Operand(Operand::Type::literal, "$62");
    case 21: return Operand(Operand::Type::literal, "$63");
    case 22: return Operand(Operand::Type::literal, "$64");
    case 23: return Operand(Operand::Type::literal, "$65");
    case 24: return Operand(Operand::Type::literal, "$66");
    case 25: return Operand(Operand::Type::literal, "$67");
    case 26: return Operand(Operand::Type::literal, "$68");
    case 27: return Operand(Operand::Type::literal, "$69");
    case 28: return Operand(Operand::Type::literal, "$6a");
    case 29: return Operand(Operand::Type::literal, "$6b");
    case 30: return Operand(Operand::Type::literal, "$6c");
    case 31: return Operand(Operand::Type::literal, "$6d");
    }
    throw std::runtime_error("Unhandled register number: " + std::to_string(reg_num));
  }
};

#endif// INC_6502_C_C64_HPP
