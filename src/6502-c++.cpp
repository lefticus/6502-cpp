#include <cassert>
#include <cctype>
#include <ctre.hpp>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>
#include <charconv>

#include "../include/assembly.hpp"
#include "../include/6502.hpp"
#include "../include/optimizer.hpp"
#include "../include/personalities/c64.hpp"


int to_int(const std::string_view sv) {
  int result{};
  std::from_chars(sv.begin(), sv.end(), result);
  return result;
}

int parse_8bit_literal(const std::string_view s)
{
  return to_int(s.substr(1));
}

std::string_view strip_lo_hi(std::string_view s)
{
  const auto matcher = ctre::match<R"((lo|hi)8\((.*)\))">;

  if (const auto results = matcher(s); results) {
    return results.get<2>();
  }

  return s;
}

std::string fixup_8bit_literal(const std::string &s)
{
  if (s[0] == '$') {
    return "#" + std::to_string(static_cast<uint8_t>(parse_8bit_literal(s)));
  }

  if (s.starts_with("0x")) {
    return "#$" + s.substr(2);
  }

  if (s.starts_with("lo8(")) {
    return fmt::format("#<{}", strip_lo_hi(s));
  }
  if (s.starts_with("hi8(")) {
    return fmt::format("#>{}", strip_lo_hi(s));
  }

  const auto is_num = std::all_of(begin(s), end(s), [](const auto c) { return (c >= '0' && c <= '9') || c == '-'; });

  if (is_num) {
    return "#<" + s;
  }

  return s;
}



struct AVR : ASMLine
{
  enum class OpCode {
    unknown,

    adc,
    adiw,
    add,
    andi,

    breq,
    brlo,
    brne,
    brsh,

    clr,
    com,
    cp,
    cpc,
    cpi,
    cpse,
    dec,

    eor,

    in,

    ld,
    ldd,
    ldi,
    lds,
    lsl,
    lsr,

    mov,

    pop,
    push,

    rcall,
    ret,
    rjmp,
    rol,

    sbc,
    sbci,
    sbiw,
    sbrc,
    sbrs,
    st,
    std,
    sts,
    sub,
    subi,
    swap,
  };

  [[nodiscard]] static constexpr OpCode parse_opcode(Type t, std::string_view o)
  {
    switch (t) {
    case Type::Label:
    case Type::Directive:
      return OpCode::unknown;
    case Type::Instruction: {
      if (o == "ldi") { return OpCode::ldi; }
      if (o == "sts") { return OpCode::sts; }
      if (o == "ret") { return OpCode::ret; }
      if (o == "mov") { return OpCode::mov; }
      if (o == "lsl") { return OpCode::lsl; }
      if (o == "rol") { return OpCode::rol; }
      if (o == "rcall") { return OpCode::rcall; }
      if (o == "ld") { return OpCode::ld; }
      if (o == "sub") { return OpCode::subi; }
      if (o == "subi") { return OpCode::subi; }
      if (o == "sbc") { return OpCode::sbci; }
      if (o == "sbci") { return OpCode::sbci; }
      if (o == "st") { return OpCode::st; }
      if (o == "std") { return OpCode::std; }
      if (o == "ldd") { return OpCode::ldd; }
      if (o == "lds") { return OpCode::lds; }
      if (o == "lsr") { return OpCode::lsr; }
      if (o == "andi") { return OpCode::andi; }
      if (o == "eor") { return OpCode::eor; }
      if (o == "sbrc") { return OpCode::sbrc; }
      if (o == "rjmp") { return OpCode::rjmp; }
      if (o == "sbrs") { return OpCode::sbrs; }
      if (o == "brne") { return OpCode::brne; }
      if (o == "dec") { return OpCode::dec; }
      if (o == "adiw") { return OpCode::adiw; }
      if (o == "sbiw") { return OpCode::sbiw; }
      if (o == "push") { return OpCode::push; }
      if (o == "pop") { return OpCode::pop; }
      if (o == "com") { return OpCode::com; }
      if (o == "swap") { return OpCode::swap; }
      if (o == "clr") { return OpCode::clr; }
      if (o == "cpse") { return OpCode::cpse; }
      if (o == "cpi") { return OpCode::cpi; }
      if (o == "brlo") { return OpCode::brlo; }
      if (o == "add") { return OpCode::add; }
      if (o == "adc") { return OpCode::adc; }
      if (o == "cpc") { return OpCode::cpc; }
      if (o == "cp") { return OpCode::cp; }
      if (o == "brsh") { return OpCode::brsh; }
      if (o == "breq") { return OpCode::breq; }
      if (o == "in") { return OpCode::in; }
    }
    }
    throw std::runtime_error(fmt::format("Unknown opcode: {}", o));
  }

  static int get_register_number(const char reg_name)
  {
    if (reg_name == 'X') {
      return 26;
    }
    if (reg_name == 'Y') {
      return 28;
    }
    if (reg_name == 'Z') {
      return 30;
    }

    throw std::runtime_error("Unknown register name");
  }

  static Operand parse_operand(std::string_view o)
  {
    if (o.empty()) {
      return Operand();
    }

    if (o[0] == 'r' && o.size() > 1) {
      return Operand(Operand::Type::reg, to_int(o.substr(1)));
    } else {
      return Operand(Operand::Type::literal, std::string{ o });
    }
  }

  AVR(const int t_line_num, std::string_view t_line_text, Type t, std::string_view t_opcode, std::string_view o1 = "", std::string_view o2 = "")
    : ASMLine(t, std::string(t_opcode)), line_num(t_line_num), line_text(std::string(t_line_text)),
      opcode(parse_opcode(t, t_opcode)), operand1(parse_operand(o1)), operand2(parse_operand(o2))
  {
  }

  int         line_num;
  std::string line_text;
  OpCode      opcode;
  Operand     operand1;
  Operand     operand2;
};

void indirect_load(std::vector<mos6502> &instructions, const std::string &from_address_low_byte, const std::string &to_address, const int offset = 0)
{
  instructions.emplace_back(mos6502::OpCode::ldy, Operand(Operand::Type::literal, fmt::format("#{}", offset)));
  instructions.emplace_back(mos6502::OpCode::lda, Operand(Operand::Type::literal, "(" + from_address_low_byte + "), Y"));
  instructions.emplace_back(mos6502::OpCode::sta, Operand(Operand::Type::literal, to_address));
}

void indirect_store(std::vector<mos6502> &instructions, const std::string &from_address, const std::string &to_address_low_byte, const int offset = 0)
{
  instructions.emplace_back(mos6502::OpCode::lda, Operand(Operand::Type::literal, from_address));
  instructions.emplace_back(mos6502::OpCode::ldy, Operand(Operand::Type::literal, fmt::format("#{}", offset)));
  instructions.emplace_back(mos6502::OpCode::sta, Operand(Operand::Type::literal, "(" + to_address_low_byte + "), Y"));
}

void fixup_16_bit_N_Z_flags(std::vector<mos6502> &instructions)
{

  // need to get both Z and N set appropriately
  // assuming A contains higher order byte and Y contains lower order byte
  instructions.emplace_back(ASMLine::Type::Directive, "; BEGIN remove if next is lda");
  instructions.emplace_back(ASMLine::Type::Directive, "; set CPU flags assuming A holds the higher order byte already");
  std::string set_flag_label = "flags_set_after_16_bit_op_" + std::to_string(instructions.size());
  // if high order is negative, we know it's not 0 and it is negative
  instructions.emplace_back(mos6502::OpCode::bmi, Operand(Operand::Type::literal, set_flag_label));
  // if it's not 0, then branch down, we know the result is not 0 and not negative
  instructions.emplace_back(mos6502::OpCode::bne, Operand(Operand::Type::literal, set_flag_label));
  // if the higher order byte is 0, test the lower order byte, which was saved for us in Y
  instructions.emplace_back(mos6502::OpCode::txa);
  // if low order is not negative, we know it's 0 or not 0
  instructions.emplace_back(mos6502::OpCode::bpl, Operand(Operand::Type::literal, set_flag_label));
  // if low order byte is negative, shift right by one bit, then we'll get the proper Z/N flags
  instructions.emplace_back(mos6502::OpCode::lsr);
  instructions.emplace_back(ASMLine::Type::Label, set_flag_label);
  instructions.emplace_back(ASMLine::Type::Directive, "; END remove if next is lda");
}

void add_16_bit(const Personality &personality, std::vector<mos6502> &instructions, int reg, const std::uint16_t value)
{
  //instructions.emplace_back(mos6502::OpCode::sta, Operand(Operand::Type::literal, address_low_byte));
  instructions.emplace_back(mos6502::OpCode::clc);
  instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(reg));
  instructions.emplace_back(mos6502::OpCode::adc, Operand(Operand::Type::literal, "#" + std::to_string((value & 0xFFu))));
  instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(reg));
  instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(reg + 1));
  instructions.emplace_back(mos6502::OpCode::adc, Operand(Operand::Type::literal, "#" + std::to_string((value >> 8u) & 0xFFu)));
  instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(reg + 1));
  instructions.emplace_back(mos6502::OpCode::tax);
  fixup_16_bit_N_Z_flags(instructions);
}

void subtract_16_bit(const Personality &personality, std::vector<mos6502> &instructions, int reg, const std::uint16_t value)
{
  //instructions.emplace_back(mos6502::OpCode::sta, Operand(Operand::Type::literal, address_low_byte));
  instructions.emplace_back(mos6502::OpCode::sec);
  instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(reg));
  instructions.emplace_back(mos6502::OpCode::sbc, Operand(Operand::Type::literal, "#" + std::to_string((value & 0xFFu))));
  instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(reg));
  instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(reg + 1));
  instructions.emplace_back(mos6502::OpCode::sbc, Operand(Operand::Type::literal, "#" + std::to_string((value >> 8u) & 0xFFu)));
  instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(reg + 1));
  instructions.emplace_back(mos6502::OpCode::tax);
  fixup_16_bit_N_Z_flags(instructions);
}

void increment_16_bit(const Personality &personality, std::vector<mos6502> &instructions, int reg)
{
  //instructions.emplace_back(mos6502::OpCode::sta, Operand(Operand::Type::literal, address_low_byte));
  instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(reg));
  instructions.emplace_back(mos6502::OpCode::clc);
  instructions.emplace_back(mos6502::OpCode::adc, Operand(Operand::Type::literal, "#1"));
  instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(reg));
  instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(reg + 1));
  instructions.emplace_back(mos6502::OpCode::adc, Operand(Operand::Type::literal, "#0"));
  instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(reg + 1));
}

void translate_instruction(const Personality &personality, std::vector<mos6502> &instructions, const AVR::OpCode op, const Operand &o1, const Operand &o2)
{
  const auto translate_register_number = [](const Operand &reg) {
    if (reg.value == "__zero_reg__") {
      return 1;
    } else if (reg.value == "__temp_reg__") {
      return 0;
    } else {
      return reg.reg_num;
    }
  };

  const auto o1_reg_num = translate_register_number(o1);
  const auto o2_reg_num = translate_register_number(o2);

  switch (op) {
  case AVR::OpCode::dec:
    instructions.emplace_back(mos6502::OpCode::dec, personality.get_register(o1_reg_num));
    return;
  case AVR::OpCode::ldi:
    instructions.emplace_back(mos6502::OpCode::lda, Operand(o2.type, fixup_8bit_literal(o2.value)));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    return;
  case AVR::OpCode::sts:
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o2_reg_num));
    instructions.emplace_back(mos6502::OpCode::sta, Operand(o1.type, o1.value));
    return;
  case AVR::OpCode::ret:
    instructions.emplace_back(mos6502::OpCode::rts);
    return;
  case AVR::OpCode::mov:
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o2_reg_num));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    return;
  case AVR::OpCode::lsl:
    instructions.emplace_back(mos6502::OpCode::asl, personality.get_register(o1_reg_num));
    return;
  case AVR::OpCode::rol:
    instructions.emplace_back(mos6502::OpCode::rol, personality.get_register(o1_reg_num));
    return;
  case AVR::OpCode::rcall:
    instructions.emplace_back(mos6502::OpCode::jsr, o1);
    return;
  case AVR::OpCode::ld: {
    if (o2.value == "Z" || o2.value == "X" || o2.value == "Y") {
      indirect_load(instructions, personality.get_register(AVR::get_register_number(o2.value[0])).value, personality.get_register(o1_reg_num).value);
      return;
    }
    if (o2.value == "Z+" || o2.value == "X+" || o2.value == "Y+") {
      indirect_load(instructions, personality.get_register(AVR::get_register_number(o2.value[0])).value, personality.get_register(o1_reg_num).value);
      increment_16_bit(personality, instructions, AVR::get_register_number(o2.value[0]));
      return;
    }
    throw std::runtime_error("Unknown ld indexing");
  }
  case AVR::OpCode::ldd: {
    indirect_load(instructions, personality.get_register(AVR::get_register_number(o2.value[0])).value, personality.get_register(o1_reg_num).value, to_int(o2.value.substr(1)));
    return;
  }
  case AVR::OpCode::std: {
    indirect_store(instructions, personality.get_register(o2_reg_num).value, personality.get_register(AVR::get_register_number(o1.value[0])).value, to_int(o2.value.substr(1)));
    return;
  }
  case AVR::OpCode::sub: {
    // we want to utilize the carry flag, however it was set previously
    // (it's really a borrow flag on the 6502)
    instructions.emplace_back(mos6502::OpCode::clc);
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::sbc, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    fixup_16_bit_N_Z_flags(instructions);
    return;
  }
  case AVR::OpCode::sbc: {
    // we want to utilize the carry flag, however it was set previously
    // (it's really a borrow flag on the 6502)
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::sbc, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    fixup_16_bit_N_Z_flags(instructions);
    return;
  }
  case AVR::OpCode::sbci: {
    // we want to utilize the carry flag, however it was set previously
    // (it's really a borrow flag on the 6502)
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::sbc, Operand(o2.type, fixup_8bit_literal(o2.value)));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    fixup_16_bit_N_Z_flags(instructions);
    return;
  }
  case AVR::OpCode::subi: {
    // to do: deal with Carry bit (and other flags) nonsense from AVR
    // if |x| < |y| -> x-y +carry
    // for these special cases with -(1) and -(-(1))
    if (o2.value == "lo8(-(-1))") {
      instructions.emplace_back(mos6502::OpCode::dec, personality.get_register(o1_reg_num));
      return;
    }
    if (o2.value == "lo8(-(1))") {
      instructions.emplace_back(mos6502::OpCode::inc, personality.get_register(o1_reg_num));
      return;
    }

    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    // have to set carry flag, since it gets inverted by sbc
    instructions.emplace_back(mos6502::OpCode::sec);
    instructions.emplace_back(mos6502::OpCode::sbc, Operand(o2.type, fixup_8bit_literal(o2.value)));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    // temporarily store lower order (not carried substraction) byte into Y for checking
    // later if this is a two byte subtraction operation
    instructions.emplace_back(mos6502::OpCode::tax);
    return;
  }
  case AVR::OpCode::st: {
    if (o1.value == "Z" || o1.value == "Y" || o1.value == "X") {
      indirect_store(instructions, personality.get_register(o2_reg_num).value, personality.get_register(AVR::get_register_number(o1.value[0])).value);
      return;
    }
    if (o1.value == "Z+" || o1.value == "Y+" || o1.value == "X+") {
      indirect_store(instructions, personality.get_register(o2_reg_num).value, personality.get_register(AVR::get_register_number(o1.value[0])).value);
      increment_16_bit(personality, instructions, AVR::get_register_number(o1.value[0]));
      return;
    }
    throw std::runtime_error("Unhandled st");
  }
  case AVR::OpCode::lds: {
    instructions.emplace_back(mos6502::OpCode::lda, o2);
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    return;
  }
  case AVR::OpCode::lsr: {
    instructions.emplace_back(mos6502::OpCode::lsr, personality.get_register(o1_reg_num));
    return;
  }
  case AVR::OpCode::andi: {
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::AND, Operand(o2.type, fixup_8bit_literal(o2.value)));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    return;
  }
  case AVR::OpCode::eor: {
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::eor, personality.get_register(o2_reg_num));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    return;
  }
  case AVR::OpCode::cpse: {
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::bit, personality.get_register(o2_reg_num));
    std::string new_label_name = "skip_next_instruction_" + std::to_string(instructions.size());
    instructions.emplace_back(mos6502::OpCode::beq, Operand(Operand::Type::literal, new_label_name));
    instructions.emplace_back(ASMLine::Type::Directive, new_label_name);
    return;
  }
  case AVR::OpCode::sbrc: {
    instructions.emplace_back(mos6502::OpCode::lda, Operand(o2.type, fixup_8bit_literal("$" + std::to_string(1 << (to_int(o2.value))))));
    instructions.emplace_back(mos6502::OpCode::bit, personality.get_register(o1_reg_num));
    std::string new_label_name = "skip_next_instruction_" + std::to_string(instructions.size());
    instructions.emplace_back(mos6502::OpCode::beq, Operand(Operand::Type::literal, new_label_name));
    instructions.emplace_back(ASMLine::Type::Directive, new_label_name);
    return;
  }
  case AVR::OpCode::sbrs: {
    instructions.emplace_back(mos6502::OpCode::lda, Operand(o2.type, fixup_8bit_literal("$" + std::to_string(1 << (to_int(o2.value.c_str()))))));
    instructions.emplace_back(mos6502::OpCode::bit, personality.get_register(o1_reg_num));
    std::string new_label_name = "skip_next_instruction_" + std::to_string(instructions.size());
    instructions.emplace_back(mos6502::OpCode::bne, Operand(Operand::Type::literal, new_label_name));
    instructions.emplace_back(ASMLine::Type::Directive, new_label_name);
    return;
  }
  case AVR::OpCode::brne: {
    if (o1.value == "0b") {
      instructions.emplace_back(mos6502::OpCode::bne, Operand(Operand::Type::literal, "memcpy_0"));
    } else {
      instructions.emplace_back(mos6502::OpCode::bne, o1);
    }
    return;
  }

  case AVR::OpCode::rjmp: {
    instructions.emplace_back(mos6502::OpCode::jmp, o1);
    return;
  }

  case AVR::OpCode::sbiw: {
    subtract_16_bit(personality, instructions, o1_reg_num, static_cast<std::uint16_t>(to_int(o2.value)));
    return;
  }

  case AVR::OpCode::adiw: {
    add_16_bit(personality, instructions, o1_reg_num, static_cast<std::uint16_t>(to_int(o2.value)));
    return;
  }

  case AVR::OpCode::push: {
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::pha);
    return;
  }
  case AVR::OpCode::pop: {
    instructions.emplace_back(mos6502::OpCode::pla);
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    return;
  }
  case AVR::OpCode::com: {
    // We're doing this in the same way the AVR does it, to make sure the C flag is set properly
    instructions.emplace_back(mos6502::OpCode::clc);
    instructions.emplace_back(mos6502::OpCode::lda, Operand(Operand::Type::literal, "#$FF"));
    instructions.emplace_back(mos6502::OpCode::sbc, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    return;
  }
  case AVR::OpCode::clr: {
    instructions.emplace_back(mos6502::OpCode::lda, Operand(Operand::Type::literal, "#$00"));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    return;
  }
  case AVR::OpCode::cpi: {
    // note that this will leave the C flag in the 6502 borrow state, not normal carry state
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::cmp, Operand(o2.type, fixup_8bit_literal(o2.value)));
    return;
  }
  case AVR::OpCode::brlo: {

    if (o1.value == ".+2") {
      // assumes 6502 'borrow' for Carry flag instead of carry, so bcc instead of bcs
      std::string new_label_name = "skip_next_instruction_" + std::to_string(instructions.size());
      instructions.emplace_back(mos6502::OpCode::bcc, Operand(Operand::Type::literal, new_label_name));
      instructions.emplace_back(ASMLine::Type::Directive, new_label_name);
      return;
    } else {
      instructions.emplace_back(mos6502::OpCode::bcc, o1);
      return;
    }
  }
  case AVR::OpCode::swap: {
    // from http://www.6502.org/source/general/SWN.html
    // ASL  A
    // ADC  #$80
    // ROL  A
    // ASL  A
    // ADC  #$80
    // ROL  A

    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::asl);
    instructions.emplace_back(mos6502::OpCode::adc, Operand(Operand::Type::literal, "#$80"));
    instructions.emplace_back(mos6502::OpCode::rol);
    instructions.emplace_back(mos6502::OpCode::asl);
    instructions.emplace_back(mos6502::OpCode::adc, Operand(Operand::Type::literal, "#$80"));
    instructions.emplace_back(mos6502::OpCode::rol);
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));

    return;
  }
  case AVR::OpCode::add: {
    instructions.emplace_back(mos6502::OpCode::clc);
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::adc, personality.get_register(o2_reg_num));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    return;
  }
  case AVR::OpCode::adc: {
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::adc, personality.get_register(o2_reg_num));
    instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(o1_reg_num));
    return;
  }
  case AVR::OpCode::cp: {
    // note that this will leave the C flag in the 6502 borrow state, not normal carry state
    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::cmp, personality.get_register(o2_reg_num));
    return;
  }
  case AVR::OpCode::cpc: {
    // this instruction seems to need to be used in the case after a sbc operation, where the
    // carry flag is set to the appropriate borrow state, so I'm going to not invert
    // the carry flag here, and assume that it's set how the 6502 wants it to be
    // set from the previous operation

    instructions.emplace_back(mos6502::OpCode::lda, personality.get_register(o1_reg_num));
    instructions.emplace_back(mos6502::OpCode::sbc, personality.get_register(o2_reg_num));
    return;
  }

  case AVR::OpCode::brsh: {
    if (o1.value == ".+2") {
      // assumes 6502 'borrow' for Carry flag instead of carry, so bcs instead of bcc
      std::string new_label_name = "skip_next_instruction_" + std::to_string(instructions.size());
      instructions.emplace_back(mos6502::OpCode::bcs, Operand(Operand::Type::literal, new_label_name));
      instructions.emplace_back(ASMLine::Type::Directive, new_label_name);
      return;
    } else {
      instructions.emplace_back(mos6502::OpCode::bcs, o1);
      return;
    }
  }

  case AVR::OpCode::in: {
    if (o2.value == "__SP_L__") {
      instructions.emplace_back(mos6502::OpCode::lda, Operand(Operand::Type::literal, std::string{personality.stack_low_address()}));
      instructions.emplace_back(mos6502::OpCode::sta, o1);
      return;
    }

    if (o2.value == "__SP_H__") {
      instructions.emplace_back(mos6502::OpCode::lda, Operand(Operand::Type::literal, std::string{ personality.stack_high_address() }));
      instructions.emplace_back(mos6502::OpCode::sta, o1);
      return;
    }

    throw std::runtime_error("Could not translate unknown 'in' instruction");
  }

  case AVR::OpCode::breq: {
    instructions.emplace_back(mos6502::OpCode::beq, o1);
    return;
  }
  case AVR::OpCode::unknown: {
    throw std::runtime_error("Could not translate 'unknown' instruction");
  }
  }

  throw std::runtime_error("Could not translate unhandled instruction");
}

void to_mos6502(const Personality &personality, const AVR &from_instruction, std::vector<mos6502> &instructions)
{
  try {
    switch (from_instruction.type) {
    case ASMLine::Type::Label:
      if (from_instruction.text == "0") {
        instructions.emplace_back(from_instruction.type, "-memcpy_0");
      } else {
        instructions.emplace_back(from_instruction.type, from_instruction.text);
      }
      return;
    case ASMLine::Type::Directive:
      if (from_instruction.text.starts_with(".string")) {
        instructions.emplace_back(ASMLine::Type::Directive, ".asc " + from_instruction.text.substr(7));
      } else if (from_instruction.text.starts_with(".zero")) {
        const auto count = std::stoull(&*std::next(from_instruction.text.begin(), 6), nullptr, 10);

        std::string zeros;
        for (std::size_t i = 0; i < count; ++i) {
          if ((i % 40) == 0) {
            if (!zeros.empty()) {
              instructions.emplace_back(ASMLine::Type::Directive, zeros);
              zeros.clear();
            }
            zeros += ".byt 0";
          } else {
            zeros += ",0";
          }
        }

        if (!zeros.empty()) {
          instructions.emplace_back(ASMLine::Type::Directive, zeros);
        }

      } else {
        instructions.emplace_back(ASMLine::Type::Directive, "; Unknown directive: " + from_instruction.text);
      }
      return;
    case ASMLine::Type::Instruction:
      const auto head = instructions.size();

      try {
        translate_instruction(personality, instructions, from_instruction.opcode, from_instruction.operand1, from_instruction.operand2);
      } catch (const std::exception &e) {
        instructions.emplace_back(ASMLine::Type::Directive, "; Unhandled opcode: '" + from_instruction.text + "' " + e.what());
        spdlog::error("[{}]: Unhandled instruction: '{}': {}", from_instruction.line_num, from_instruction.line_text, e.what());
      }

      auto text = from_instruction.line_text;
      if (text[0] == '\t') {
        text.erase(0, 1);
      }
      for_each(std::next(instructions.begin(), static_cast<std::ptrdiff_t>(head)), instructions.end(), [text](auto &ins) {
        ins.comment = text;
      });
      return;
    }
  } catch (const std::exception &e) {
    spdlog::error("[{}]: Unhandled instruction: '{}': {}", from_instruction.line_num, from_instruction.line_text, e.what());
  }
}


bool fix_long_branches(std::vector<mos6502> &instructions, int &branch_patch_count)
{
  std::map<std::string, size_t> labels;
  for (size_t op = 0; op < instructions.size(); ++op) {
    if (instructions[op].type == ASMLine::Type::Label) {
      labels[instructions[op].text] = op;
    }
  }

  for (size_t op = 0; op < instructions.size(); ++op) {
    if (instructions[op].is_branch && std::abs(static_cast<int>(labels[instructions[op].op.value]) - static_cast<int>(op)) * 3 > 255) {
      ++branch_patch_count;
      const auto going_to = instructions[op].op.value;
      const auto new_pos  = "patch_" + std::to_string(branch_patch_count);
      // uh-oh too long of a branch, have to convert this to a jump...

      std::map<mos6502::OpCode, mos6502::OpCode> branch_mapping;

      branch_mapping[mos6502::OpCode::bne] = mos6502::OpCode::beq;
      branch_mapping[mos6502::OpCode::beq] = mos6502::OpCode::bne;
      branch_mapping[mos6502::OpCode::bcc] = mos6502::OpCode::bcs;
      branch_mapping[mos6502::OpCode::bcs] = mos6502::OpCode::bcc;

      const auto mapping = branch_mapping.find(instructions[op].opcode);

      if (mapping != branch_mapping.end()) {
        const auto comment = instructions[op].comment;
        instructions[op]   = mos6502(mapping->second, Operand(Operand::Type::literal, new_pos));
        instructions.insert(std::next(std::begin(instructions), static_cast<std::ptrdiff_t>(op + 1)), mos6502(mos6502::OpCode::jmp, Operand(Operand::Type::literal, going_to)));
        instructions.insert(std::next(std::begin(instructions), static_cast<std::ptrdiff_t>(op + 2)), mos6502(ASMLine::Type::Label, new_pos));
        instructions[op].comment = instructions[op + 1].comment = instructions[op + 2].comment = comment;
        return true;
      }

      throw std::runtime_error("Don't know how to reorg this branch: " + instructions[op].to_string());
    }
  }
  return false;
}


bool fix_overwritten_flags(std::vector<mos6502> &instructions)
{
  if (instructions.size() < 3) {
    return false;
  }

  for (size_t op = 0; op < instructions.size(); ++op) {
    if (instructions[op].is_comparison) {
      auto op2 = op + 1;
      while (op2 < instructions.size()
             && !instructions[op2].is_comparison
             && !instructions[op2].is_branch) {
        ++op2;
      }

      if (op2 < instructions.size()
          && (op2 - op) > 1
          && instructions[op2 - 1].opcode != mos6502::OpCode::plp) {
        if (instructions[op2].is_comparison) {
          continue;
        }

        if (instructions[op2].is_branch) {
          // insert a pull of processor status before the branch
          instructions.insert(std::next(std::begin(instructions), static_cast<std::ptrdiff_t>(op2)), mos6502(mos6502::OpCode::plp));
          // insert a push of processor status after the comparison
          instructions.insert(std::next(std::begin(instructions), static_cast<std::ptrdiff_t>(op + 1)), mos6502(mos6502::OpCode::php));

          return true;
        }
      }
    }
  }

  return false;
}

void run(const Personality &personality, std::istream &input)
{
  std::regex Comment(R"(\s*\#.*)");
  std::regex Label(R"(^\s*(\S+):.*)");
  std::regex Directive(R"(^\s*(\..+))");
  std::regex UnaryInstruction(R"(^\s+(\S+)\s+(\S+))");
  std::regex BinaryInstruction(R"(^\s+(\S+)\s+(\S+),\s*(\S+))");
  std::regex Instruction(R"(^\s+(\S+))");

  std::size_t lineno = 0;


  std::vector<AVR> instructions;

  while (input.good()) {
    std::string line;
    getline(input, line);
    try {
      std::smatch match;
      if (std::regex_match(line, match, Label)) {
        instructions.emplace_back(lineno, line, ASMLine::Type::Label, match[1].str());
      } else if (std::regex_match(line, match, Comment)) {
        // don't care about comments
      } else if (std::regex_match(line, match, Directive)) {
        instructions.emplace_back(lineno, line, ASMLine::Type::Directive, match[1].str());
      } else if (std::regex_match(line, match, BinaryInstruction)) {
        instructions.emplace_back(lineno, line, ASMLine::Type::Instruction, match[1].str(), match[2].str(), match[3].str());
      } else if (std::regex_match(line, match, UnaryInstruction)) {
        instructions.emplace_back(lineno, line, ASMLine::Type::Instruction, match[1].str(), match[2].str());
      } else if (std::regex_match(line, match, Instruction)) {
        instructions.emplace_back(lineno, line, ASMLine::Type::Instruction, match[1].str());
      } else if (line.empty()) {
        // skip empty lines
      }
    } catch (const std::exception &e) {
      spdlog::error("[{}]: parse exception with '{}': {}", lineno, line, e.what());
    }

    ++lineno;
  }

  std::set<std::string> labels;

  for (const auto &i : instructions) {
    if (i.type == ASMLine::Type::Label) {
      labels.insert(i.text);
    }
  }

  std::set<std::string> used_labels{ "main" };

  for (const auto &i : instructions) {
    if (i.type == ASMLine::Type::Instruction) {

      const auto check_label = [&](const std::string &value) {
        if (labels.count(value) != 0) {
          used_labels.insert(value);
        }
      };

      check_label(i.operand1.value);
      check_label(i.operand2.value);
      check_label(std::string{ strip_lo_hi(i.operand1.value) });
      check_label(std::string{ strip_lo_hi(i.operand2.value) });
    }
  }

  const auto new_labels =
    [&used_labels]() {
      std::map<std::string, std::string> result;
      for (const auto &l : used_labels) {
        auto newl = l;
        std::transform(newl.begin(), newl.end(), newl.begin(), [](const auto c) { return std::tolower(c); });
        newl.erase(std::remove_if(newl.begin(), newl.end(), [](const auto c) { return !std::isalnum(c); }), std::end(newl));

        result.emplace(std::make_pair(l, newl));
      }
      return result;
    }();



  for (auto &i : instructions) {
    if (i.type == ASMLine::Type::Label) {
      if (i.text == "0") {
        i.text = "-memcpy_0";
      } else {
        try {
          i.text = new_labels.at(i.text);
        } catch (...) {
          spdlog::error("Error looking up label name: '{}'", i.text);
          throw;
        }
      }
    }

    if (i.operand2.value.starts_with("lo8(") || i.operand2.value.starts_with("hi8(")) {
      const auto potential_label = strip_lo_hi(i.operand2.value);
      const auto itr1            = new_labels.find(std::string{ potential_label });
      if (itr1 != new_labels.end()) {
        i.operand2.value.replace(4, potential_label.size(), itr1->second);
      }
    }

    const auto itr1 = new_labels.find(i.operand1.value);
    if (itr1 != new_labels.end()) {
      i.operand1.value = itr1->second;
    }

    const auto itr2 = new_labels.find(i.operand2.value);
    if (itr2 != new_labels.end()) {
      i.operand2.value = itr2->second;
    }
  }


  std::vector<mos6502> new_instructions;

  personality.insert_autostart_sequence(new_instructions);
 // set __zero_reg__ (reg 1 on AVR) to 0
  new_instructions.emplace_back(mos6502::OpCode::lda, Operand(Operand::Type::literal, "#$00"));
  new_instructions.emplace_back(mos6502::OpCode::sta, personality.get_register(1));
  new_instructions.emplace_back(mos6502::OpCode::jmp, Operand(Operand::Type::literal, "main"));


  int         instructions_to_skip = 0;
  std::string next_label_name;
  for (const auto &i : instructions) {
    to_mos6502(personality, i, new_instructions);

    // intentionally copy so we don't invalidate the reference
    const auto last_instruction     = new_instructions.back();
    const auto last_instruction_loc = new_instructions.size() - 1;

    if (instructions_to_skip == 1) {
      new_instructions.emplace_back(ASMLine::Type::Label, next_label_name);
    }

    --instructions_to_skip;

    if (last_instruction.type == ASMLine::Type::Directive && last_instruction.text.starts_with("skip_next_instruction")) {
      instructions_to_skip = 1;
      next_label_name      = last_instruction.text;
      new_instructions.erase(std::next(new_instructions.begin(), static_cast<std::ptrdiff_t>(last_instruction_loc)));
    }
    if (last_instruction.type == ASMLine::Type::Directive && last_instruction.text.starts_with("skip_next_2_instructions")) {
      instructions_to_skip = 2;
      next_label_name      = last_instruction.text;
      new_instructions.erase(std::next(new_instructions.begin(), static_cast<std::ptrdiff_t>(last_instruction_loc)));
    }
  }

  while (fix_overwritten_flags(new_instructions)) {
    // do it however many times it takes
  }

  while (optimize(new_instructions)) {
    // do it however many times it takes
  }

  int branch_patch_count = 0;
  while (fix_long_branches(new_instructions, branch_patch_count)) {
    // do it however many times it takes
  }


  for (const auto &i : new_instructions) {
    std::cout << i.to_string() << '\n';
  }
}

int main(const int argc, const char **argv)
{
  std::ifstream input_file;

  std::istream &input = [&]() -> std::istream & {
    if (argc > 1) {
      input_file.open(argv[1]);
      return input_file;
    } else {
      return std::cin;
    }
  }();

  C64 personality;

  std::cout << "; AVR Mode\n";
  run(personality, input);
}
