#ifndef INC_6502_CPP_6502_HPP
#define INC_6502_CPP_6502_HPP

#include "assembly.hpp"

struct mos6502 : ASMLine
{
  enum class OpCode {
    unknown,

    adc,
    AND,
    asl,

    bcc,
    bcs,
    beq,
    bit,
    bmi,
    bne,
    bpl,

    cpy,
    cmp,
    clc,

    dec,

    eor,

    inc,

    jmp,
    jsr,

    lda,
    ldx,
    ldy,
    lsr,

    ORA,

    pha,
    php,
    pla,
    plp,

    rol,
    ror,
    rts,

    sbc,
    sec,
    sta,
    stx,
    sty,

    tax,
    tay,
    tsx,
    txa,
    txs,
    tya,
  };

  static bool get_is_branch(const OpCode o)
  {
    switch (o) {
    case OpCode::beq:
    case OpCode::bne:
    case OpCode::bmi:
    case OpCode::bpl:
    case OpCode::bcc:
    case OpCode::bcs:
      return true;
    case OpCode::adc:
    case OpCode::AND:
    case OpCode::asl:
    case OpCode::bit:
    case OpCode::cpy:
    case OpCode::cmp:
    case OpCode::clc:
    case OpCode::dec:
    case OpCode::eor:
    case OpCode::inc:
    case OpCode::jmp:
    case OpCode::jsr:
    case OpCode::lda:
    case OpCode::ldx:
    case OpCode::ldy:
    case OpCode::lsr:
    case OpCode::ORA:
    case OpCode::pha:
    case OpCode::php:
    case OpCode::pla:
    case OpCode::plp:
    case OpCode::rol:
    case OpCode::ror:
    case OpCode::rts:
    case OpCode::sbc:
    case OpCode::sec:
    case OpCode::sta:
    case OpCode::sty:
    case OpCode::stx:
    case OpCode::tax:
    case OpCode::tay:
    case OpCode::tsx:
    case OpCode::txa:
    case OpCode::txs:
    case OpCode::tya:

    case OpCode::unknown:
      break;
    }
    return false;
  }

  static bool get_is_comparison(const OpCode o)
  {
    switch (o) {
    case OpCode::bit:
    case OpCode::cmp:
    case OpCode::cpy:
      return true;
    case OpCode::adc:
    case OpCode::AND:
    case OpCode::asl:
    case OpCode::beq:
    case OpCode::bne:
    case OpCode::bmi:
    case OpCode::bpl:
    case OpCode::bcc:
    case OpCode::bcs:
    case OpCode::clc:
    case OpCode::dec:
    case OpCode::eor:
    case OpCode::inc:
    case OpCode::jmp:
    case OpCode::jsr:
    case OpCode::lda:
    case OpCode::ldx:
    case OpCode::ldy:
    case OpCode::lsr:
    case OpCode::ORA:
    case OpCode::pha:
    case OpCode::php:
    case OpCode::pla:
    case OpCode::plp:
    case OpCode::rol:
    case OpCode::ror:
    case OpCode::rts:
    case OpCode::sbc:
    case OpCode::sec:
    case OpCode::sta:
    case OpCode::stx:
    case OpCode::sty:
    case OpCode::tax:
    case OpCode::tay:
    case OpCode::tsx:
    case OpCode::txa:
    case OpCode::txs:
    case OpCode::tya:
    case OpCode::unknown:
      break;
    }
    return false;
  }


  explicit mos6502(const OpCode o)
    : ASMLine(Type::Instruction, std::string{ to_string(o) }), opcode(o), is_branch(get_is_branch(o)), is_comparison(get_is_comparison(o))
  {
  }

  mos6502(const Type t, std::string s)
    : ASMLine(t, std::move(s))
  {
  }

  mos6502(const OpCode o, Operand t_o)
    : ASMLine(Type::Instruction, std::string{ to_string(o) }), opcode(o), op(std::move(t_o)), is_branch(get_is_branch(o)), is_comparison(get_is_comparison(o))
  {
  }

  constexpr static std::string_view to_string(const OpCode o)
  {
    switch (o) {
    case OpCode::lda: return "lda";
    case OpCode::asl: return "asl";
    case OpCode::rol: return "rol";
    case OpCode::ldx: return "ldx";
    case OpCode::ldy: return "ldy";
    case OpCode::tay: return "tay";
    case OpCode::tya: return "tya";
    case OpCode::tax: return "tax";
    case OpCode::tsx: return "tsx";
    case OpCode::txa: return "txa";
    case OpCode::txs: return "txs";
    case OpCode::cpy: return "cpy";
    case OpCode::eor: return "eor";
    case OpCode::sta: return "sta";
    case OpCode::sty: return "sty";
    case OpCode::stx: return "stx";
    case OpCode::pha: return "pha";
    case OpCode::pla: return "pla";
    case OpCode::php: return "php";
    case OpCode::plp: return "plp";
    case OpCode::lsr: return "lsr";
    case OpCode::ror: return "ror";
    case OpCode::AND: return "and";
    case OpCode::inc: return "inc";
    case OpCode::dec: return "dec";
    case OpCode::ORA: return "ora";
    case OpCode::cmp: return "cmp";
    case OpCode::bne: return "bne";
    case OpCode::bmi: return "bmi";
    case OpCode::beq: return "beq";
    case OpCode::jmp: return "jmp";
    case OpCode::adc: return "adc";
    case OpCode::sbc: return "sbc";
    case OpCode::rts: return "rts";
    case OpCode::clc: return "clc";
    case OpCode::sec: return "sec";
    case OpCode::bit: return "bit";
    case OpCode::jsr: return "jsr";
    case OpCode::bpl: return "bpl";
    case OpCode::bcc: return "bcc";
    case OpCode::bcs: return "bcs";
    case OpCode::unknown: return "";
    }

    return "";
  }

  [[nodiscard]] std::string to_string() const
  {
    switch (type) {
    case ASMLine::Type::Label:
      return text;// + ':';
    case ASMLine::Type::Directive:
    case ASMLine::Type::Instruction: {
      return fmt::format("\t{} {:15}; {}", text, op.value, comment);
    }
    }
    throw std::runtime_error("Unable to render: " + text);
  }


  OpCode      opcode = OpCode::unknown;
  Operand     op;
  std::string comment;
  bool        is_branch     = false;
  bool        is_comparison = false;
};

#endif//INC_6502_CPP_6502_HPP
