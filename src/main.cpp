#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <regex>
#include <cassert>
#include <map>
#include <cctype>

struct ASMLine
{
  enum class Type
  {
    Label,
    Instruction,
    Directive
  };

  ASMLine(Type t, std::string te) : type(t), text(std::move(te)) {}

  Type type;
  std::string text;
};

int parse_8bit_literal(const std::string &s)
{
  return std::stoi(std::string(std::next(std::begin(s)), std::end(s)));
}

std::string fixup_8bit_literal(const std::string &s)
{
  if (s[0] == '$')
  {
    return "#" + std::to_string(static_cast<uint8_t>(parse_8bit_literal(s)));
  } else {
    return s;
  }
}

struct Operand
{
  enum class Type
  {
    empty,
    literal,
    reg /*ister*/
  };

  Type type = Type::empty;
  int reg_num = 0;
  std::string value;

  Operand() = default;

  bool operator==(const Operand &other) const {
    return type == other.type && reg_num == other.reg_num && value == other.value;
  }

  Operand(const Type t, std::string v)
    : type(t), value(std::move(v))
  {
    assert(type == Type::literal);
  }

  Operand(const Type t, const int num)
    : type(t), reg_num(num)
  {
    assert(type == Type::reg);
  }
};

Operand get_register(const int reg_num, const int offset = 0) {
  switch (reg_num) {
    //  http://sta.c64.org/cbm64mem.html
    case 0x00: return Operand(Operand::Type::literal, "$03"); // unused, fp->int routine pointer
    case 0x01: return Operand(Operand::Type::literal, "$04");
    case 0x02: return Operand(Operand::Type::literal, "$05"); // unused, int->fp routine pointer
    case 0x03: return Operand(Operand::Type::literal, "$06");
    case 0x04: return Operand(Operand::Type::literal, "$fb"); // unused 
    case 0x05: return Operand(Operand::Type::literal, "$fc"); // unused
    case 0x06: return Operand(Operand::Type::literal, "$fd"); // unused
    case 0x07: return Operand(Operand::Type::literal, "$fe"); // unused
    case 0x08: return Operand(Operand::Type::literal, "$22"); // unused
    case 0x09: return Operand(Operand::Type::literal, "$23"); // unused
    case 0x0A: return Operand(Operand::Type::literal, "$39"); // Current BASIC line number
    case 0x0B: return Operand(Operand::Type::literal, "$3a"); // Current BASIC line number
    case 0x10: return get_register(0x00 + offset);
    case 0x11: return get_register(0x02 + offset);
    case 0x12: return get_register(0x04 + offset);
    case 0x13: return get_register(0x06 + offset);
    case 0x14: return get_register(0x08 + offset);
    case 0x15: return get_register(0x0A + offset);
  };
  throw std::runtime_error("Unhandled register number: " + std::to_string(reg_num));
}

struct mos6502 : ASMLine
{
  enum class OpCode 
  {
    unknown,
    lda,
    ldy,
    tay,
    tya,
    cpy,
    eor,
    sta,
    sty,
    pha,
    pla,
    php,
    plp,
    lsr,
    AND,
    inc,
    dec,
    ORA,
    cmp,
    bne,
    beq,
    bmi,
    jmp,
    adc,
    sbc,
    rts,
    clc,
    sec,
    bit
  };

  static bool get_is_branch(const OpCode o) {
    switch (o) {
      case OpCode::beq:
      case OpCode::bne:
      case OpCode::bmi:
        return true;
      case OpCode::lda:
      case OpCode::ldy:
      case OpCode::tay:
      case OpCode::tya:
      case OpCode::cpy:
      case OpCode::eor:
      case OpCode::sta:
      case OpCode::sty:
      case OpCode::pha:
      case OpCode::pla:
      case OpCode::php:
      case OpCode::plp:
      case OpCode::lsr:
      case OpCode::AND:
      case OpCode::inc:
      case OpCode::dec:
      case OpCode::ORA:
      case OpCode::cmp:
      case OpCode::jmp:
      case OpCode::adc:
      case OpCode::sbc:
      case OpCode::rts:
      case OpCode::clc:
      case OpCode::sec:
      case OpCode::bit:
      case OpCode::unknown:
        break;
    }
    return false;
  }

  static bool get_is_comparison(const OpCode o) {
    switch (o) {
      case OpCode::cmp:
      case OpCode::cpy:
      case OpCode::bit:
        return true;
      case OpCode::lda:
      case OpCode::ldy:
      case OpCode::tay:
      case OpCode::tya:
      case OpCode::eor:
      case OpCode::sta:
      case OpCode::sty:
      case OpCode::pha:
      case OpCode::pla:
      case OpCode::php:
      case OpCode::plp:
      case OpCode::lsr:
      case OpCode::AND:
      case OpCode::inc:
      case OpCode::dec:
      case OpCode::ORA:
      case OpCode::jmp:
      case OpCode::bne:
      case OpCode::bmi:
      case OpCode::beq:
      case OpCode::adc:
      case OpCode::sbc:
      case OpCode::rts:
      case OpCode::clc:
      case OpCode::sec:
      case OpCode::unknown:
        break;
    }
    return false;
  }


  explicit mos6502(const OpCode o)
    : ASMLine(Type::Instruction, to_string(o)), opcode(o), is_branch(get_is_branch(o)), is_comparison(get_is_comparison(o))
  {
  }

  mos6502(const Type t, std::string s)
    : ASMLine(t, std::move(s))
  {
  }

  mos6502(const OpCode o, Operand t_o)
    : ASMLine(Type::Instruction, to_string(o)), opcode(o), op(std::move(t_o)), is_branch(get_is_branch(o)), is_comparison(get_is_comparison(o))
  {
  }

  static std::string to_string(const OpCode o)
  {
    switch (o) {
      case OpCode::lda:
        return "lda";
      case OpCode::ldy:
        return "ldy";
      case OpCode::tay:
        return "tay";
      case OpCode::tya:
        return "tya";
      case OpCode::cpy:
        return "cpy";
      case OpCode::eor:
        return "eor";
      case OpCode::sta:
        return "sta";
      case OpCode::sty:
        return "sty";
      case OpCode::pha:
        return "pha";
      case OpCode::pla:
        return "pla";
      case OpCode::php:
        return "php";
      case OpCode::plp:
        return "plp";
      case OpCode::lsr:
        return "lsr";
      case OpCode::AND:
        return "and";
      case OpCode::inc:
        return "inc";
      case OpCode::dec:
        return "dec";
      case OpCode::ORA:
        return "ora";
      case OpCode::cmp:
        return "cmp";
      case OpCode::bne:
        return "bne";
      case OpCode::bmi:
        return "bmi";
      case OpCode::beq:
        return "beq";
      case OpCode::jmp:
        return "jmp";
      case OpCode::adc:
        return "adc";
      case OpCode::sbc:
        return "sbc";
      case OpCode::rts:
        return "rts";
      case OpCode::clc:
        return "clc";
      case OpCode::sec:
        return "sec";
      case OpCode::bit:
        return "bit";
      case OpCode::unknown:
        return "";
    };

    return "";
  }

  std::string to_string() const
  {
    switch (type) {
      case ASMLine::Type::Label:
        return text; // + ':';
      case ASMLine::Type::Directive:
      case ASMLine::Type::Instruction:
        return '\t' + text + ' ' + op.value + "; " + comment;
    };
    throw std::runtime_error("Unable to render: " + text);
  }


  OpCode opcode = OpCode::unknown;
  Operand op;
  std::string comment;
  bool is_branch = false;
  bool is_comparison = false;
};



struct i386 : ASMLine
{
  enum class OpCode 
  {
    unknown,
    movzbl,
    movzwl,
    shrb,
    xorl,
    andl,
    andb,
    addb,
    ret,
    movb,
    cmpb,
    movl,
    jmp,
    jne,
    je,
    js,
    testb,
    incl,
    incb,
    decl,
    decb,
    sarl,
    addl,
    subl,
    subb,
    sall,
    orl,
    orb,
    rep,
    pushl,
    sbbb,
    negb,
    notb
  };

  static OpCode parse_opcode(Type t, const std::string &o)
  {
    switch(t)
    {
      case Type::Label:
        return OpCode::unknown;
      case Type::Directive:
        return OpCode::unknown;
      case Type::Instruction:
        {
          if (o == "movzwl") return OpCode::movzwl;
          if (o == "movzbl") return OpCode::movzbl;
          if (o == "shrb") return OpCode::shrb;
          if (o == "xorl") return OpCode::xorl;
          if (o == "andl") return OpCode::andl;
          if (o == "ret") return OpCode::ret;
          if (o == "movb") return OpCode::movb;
          if (o == "cmpb") return OpCode::cmpb;
          if (o == "movl") return OpCode::movl;
          if (o == "jmp") return OpCode::jmp;
          if (o == "testb") return OpCode::testb;
          if (o == "incl") return OpCode::incl;
          if (o == "sarl") return OpCode::sarl;
          if (o == "decl") return OpCode::decl;
          if (o == "jne") return OpCode::jne;
          if (o == "je") return OpCode::je;
          if (o == "js") return OpCode::js;
          if (o == "subl") return OpCode::subl;
          if (o == "subb") return OpCode::subb;
          if (o == "addl") return OpCode::addl;
          if (o == "addb") return OpCode::addb;
          if (o == "sall") return OpCode::sall;
          if (o == "orl") return OpCode::orl;
          if (o == "andb") return OpCode::andb;
          if (o == "orb") return OpCode::orb;
          if (o == "decb") return OpCode::decb;
          if (o == "incb") return OpCode::incb;
          if (o == "rep") return OpCode::rep;
          if (o == "notb") return OpCode::notb;
          if (o == "negb") return OpCode::negb;
          if (o == "sbbb") return OpCode::sbbb;
          if (o == "pushl") return OpCode::pushl;
        }
    }
    throw std::runtime_error("Unknown opcode: " + o);
  }

  static Operand parse_operand(std::string o)
  {
    if (o.empty()) {
      return Operand();
    }

    if (o[0] == '%') {
      if (o == "%al") {
        return Operand(Operand::Type::reg, 0x00);
      } else if (o == "%ah") {
        return Operand(Operand::Type::reg, 0x01);
      } else if (o == "%bl") {
        return Operand(Operand::Type::reg, 0x02);
      } else if (o == "%bh") {
        return Operand(Operand::Type::reg, 0x03);
      } else if (o == "%cl") {
        return Operand(Operand::Type::reg, 0x04);
      } else if (o == "%ch") {
        return Operand(Operand::Type::reg, 0x05);
      } else if (o == "%dl") {
        return Operand(Operand::Type::reg, 0x06);
      } else if (o == "%dh") {
        return Operand(Operand::Type::reg, 0x07);
      } else if (o == "%sil") {
        return Operand(Operand::Type::reg, 0x08);
      } else if (o == "%dil") {
        return Operand(Operand::Type::reg, 0x0A);
      } else if (o == "%ax" || o == "%eax") {
        return Operand(Operand::Type::reg, 0x10);
      } else if (o == "%bx" || o == "%ebx") {
        return Operand(Operand::Type::reg, 0x11);
      } else if (o == "%cx" || o == "%ecx") {
        return Operand(Operand::Type::reg, 0x12);
      } else if (o == "%dx" || o == "%edx") {
        return Operand(Operand::Type::reg, 0x13);
      } else if (o == "%si" || o == "%esi") {
        return Operand(Operand::Type::reg, 0x14);
      } else if (o == "%di" || o == "%edi") {
        return Operand(Operand::Type::reg, 0x15);
      } else {
        throw std::runtime_error("Unknown register operand: '" + o + "'");
      }
    } else {
      return Operand(Operand::Type::literal, std::move(o));
    }
  }

  i386(const int t_line_num, std::string t_line_text, Type t, std::string t_opcode, std::string o1="", std::string o2="")
    : ASMLine(t, t_opcode), line_num(t_line_num), line_text(std::move(t_line_text)), 
      opcode(parse_opcode(t, t_opcode)), operand1(parse_operand(o1)), operand2(parse_operand(o2))
  {
  }

  int line_num;
  std::string line_text;
  OpCode opcode;
  Operand operand1;
  Operand operand2;
};

void translate_instruction(std::vector<mos6502> &instructions, const i386::OpCode op, const Operand &o1, const Operand &o2)
{
  switch(op)
  {
    case i386::OpCode::ret:
      instructions.emplace_back(mos6502::OpCode::rts);
      break;
    case i386::OpCode::movl:
      if (o1.type == Operand::Type::reg && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num, 1));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num, 1));
      } else {
        throw std::runtime_error("Cannot translate movl instruction");
      }
      break;
    case i386::OpCode::xorl:
      if (o1.type == Operand::Type::reg && o2.type == Operand::Type::reg
          && o1.reg_num == o2.reg_num) {
        instructions.emplace_back(mos6502::OpCode::lda, Operand(Operand::Type::literal, "#$00"));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num, 1));
      } else {
        throw std::runtime_error("Cannot translate movl instruction");
      }
      break;
    case i386::OpCode::movb:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::lda, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::sta, o2);
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::lda, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
      } else if (o1.type == Operand::Type::reg && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::sta, o2);
      } else if (o1.type == Operand::Type::reg && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
      } else {
        throw std::runtime_error("Cannot translate movb instruction");
      }
      break;
    case i386::OpCode::orb:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::lda, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::ORA, o2);
        instructions.emplace_back(mos6502::OpCode::sta, o2);
      } else if (o1.type == Operand::Type::reg && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::ORA, get_register(o2.reg_num));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
      } else {
        throw std::runtime_error("Cannot translate orb instruction");
      }
      break;

    case i386::OpCode::movzbl:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::lda, o1);
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
      } else {
        throw std::runtime_error("Cannot translate movzbl instruction");
      }
      break;
    case i386::OpCode::shrb:
      if (o1.type == Operand::Type::reg || o2.type == Operand::Type::reg) {
        const auto do_shift = [&instructions](const int reg_num) {
          instructions.emplace_back(mos6502::OpCode::lsr, get_register(reg_num));
        };

        if (o1.type == Operand::Type::literal) {
          const auto count = parse_8bit_literal(o1.value);
          for (int i = 0; i < count; ++i) {
            do_shift(o2.reg_num);
          }
        } else {
          do_shift(o1.reg_num);
        }
      } else {
        throw std::runtime_error("Cannot translate shrb instruction");
      }
      break;
    case i386::OpCode::testb:
      if (o1.type == Operand::Type::reg && o2.type == Operand::Type::reg && o1.reg_num == o2.reg_num) {
        // this just tests the register for 0
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num));
//        instructions.emplace_back(mos6502::OpCode::bit, Operand(Operand::Type::literal, "#$00"));
      } else if (o1.type == Operand::Type::reg && o2.type == Operand::Type::reg) {
        // ands the values
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::bit, get_register(o2.reg_num));
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg) {
        // ands the values
        instructions.emplace_back(mos6502::OpCode::lda, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::bit, get_register(o2.reg_num));
      } else {
        throw std::runtime_error("Cannot translate testb instruction");
      }
      break;
    case i386::OpCode::decb:
      if (o1.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::dec, get_register(o1.reg_num));
      } else {
        instructions.emplace_back(mos6502::OpCode::dec, o1);
      }
      break;
    case i386::OpCode::incb:
      if (o1.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::inc, get_register(o1.reg_num));
      } else {
        instructions.emplace_back(mos6502::OpCode::inc, o1);
      }
      break;
    case i386::OpCode::jne:
      instructions.emplace_back(mos6502::OpCode::bne, o1);
      break;
    case i386::OpCode::je:
      instructions.emplace_back(mos6502::OpCode::beq, o1);
      break;
    case i386::OpCode::js:
      instructions.emplace_back(mos6502::OpCode::bmi, o1);
      break;
    case i386::OpCode::jmp:
      instructions.emplace_back(mos6502::OpCode::jmp, o1);
      break;
    case i386::OpCode::addb:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o2.reg_num));
        instructions.emplace_back(mos6502::OpCode::clc);
        instructions.emplace_back(mos6502::OpCode::adc, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::lda, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::clc);
        instructions.emplace_back(mos6502::OpCode::adc, o2);
        instructions.emplace_back(mos6502::OpCode::sta, o2);
      } else if (o1.type == Operand::Type::reg && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::clc);
        instructions.emplace_back(mos6502::OpCode::adc, o2);
        instructions.emplace_back(mos6502::OpCode::sta, o2);
      } else {
        throw std::runtime_error("Cannot translate addb instruction");
      }
      break;
    case i386::OpCode::cmpb:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::lda, o2);
        instructions.emplace_back(mos6502::OpCode::cmp, Operand(o1.type, fixup_8bit_literal(o1.value)));
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o2.reg_num));
        instructions.emplace_back(mos6502::OpCode::cmp, Operand(o1.type, fixup_8bit_literal(o1.value)));
      } else {
        throw std::runtime_error("Cannot translate cmb instruction");
      }
      break;
    case i386::OpCode::andb:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg)  {
        const auto reg = get_register(o2.reg_num);
        instructions.emplace_back(mos6502::OpCode::lda, reg);
        instructions.emplace_back(mos6502::OpCode::AND, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::sta, reg);
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::literal)  {
        const auto reg = get_register(o2.reg_num);
        instructions.emplace_back(mos6502::OpCode::lda, o2);
        instructions.emplace_back(mos6502::OpCode::AND, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::sta, o2);
      } else {
        throw std::runtime_error("Cannot translate andb instruction");
      }
      break;
    case i386::OpCode::negb:
      if (o1.type == Operand::Type::reg) {
        // perform a two's complement of the register location
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::eor, Operand(Operand::Type::literal, "#$ff"));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::inc, get_register(o1.reg_num));
      } else {
        throw std::runtime_error("Cannot translate negb instruction");
      }
      break;
    case i386::OpCode::notb:
      if (o1.type == Operand::Type::reg) {
        // exclusive or against 0xff to perform a logical not
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::eor, Operand(Operand::Type::literal, "#$ff"));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o1.reg_num));
      } else {
        throw std::runtime_error("Cannot translate notb instruction");
      }
      break;
    case i386::OpCode::subb:
      // DEST <- DEST - SRC
      // o2 = o2 - o1
      // Ensure that we set the carry flag before performing the subtraction
      if (o1.type == Operand::Type::reg && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::lda, o2);
        instructions.emplace_back(mos6502::OpCode::sec);
        instructions.emplace_back(mos6502::OpCode::sbc, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::sta, o2);
      } else {
        throw std::runtime_error("Cannot translate subb instruction");
      }
      break;
    case i386::OpCode::pushl:
      if (o1.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::pha);
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o1.reg_num, 1));
        instructions.emplace_back(mos6502::OpCode::pha);
      } else {
        throw std::runtime_error("Cannot translate sbb instruction");
      }
      break;

    case i386::OpCode::sbbb:
      // DEST <- (DEST – (SRC + CF))
      // o2 <- (o2 - (o1 + cf))
      // if o1 and o2 are the same we get
      // o2 <- (o2 - (o2 + cf))
      // o2 <- -cf
      if (o1.type == Operand::Type::reg && o2.type == Operand::Type::reg
          && o1.reg_num == o2.reg_num) {
        instructions.emplace_back(mos6502::OpCode::lda, Operand(Operand::Type::literal, "#$00")); // reset a
        instructions.emplace_back(mos6502::OpCode::sbc, Operand(Operand::Type::literal, "#$00")); // subtract out the carry flag
        instructions.emplace_back(mos6502::OpCode::eor, Operand(Operand::Type::literal, "#$ff")); // invert the bits
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num)); // place the value 
      } else {
        throw std::runtime_error("Cannot translate sbb instruction");
      }
      break;

    default:
      throw std::runtime_error("Cannot translate unhandled instruction");

  };

}

enum class LogLevel
{
  Warning,
  Error
};

std::string to_string(const LogLevel ll)
{
  switch (ll)
  {
    case LogLevel::Warning:
      return "warning";
    case LogLevel::Error:
      return "error";
  }
  return "unknown";
}

void log(LogLevel ll, const i386 &i, const std::string &message)
{
  std::cout << to_string(ll) << ": " << i.line_num << ": " << message << ": `" << i.line_text << "`\n";
}

void log(LogLevel ll, const int line_no, const std::string &line, const std::string &message)
{
  std::cout << to_string(ll) << ": "  << line_no << ": " << message << ": `" << line << "`\n";
}

void to_mos6502(const i386 &i, std::vector<mos6502> &instructions)
{
  try {
    switch(i.type)
    {
      case ASMLine::Type::Label:
        instructions.emplace_back(i.type, i.text);
        return;
      case ASMLine::Type::Directive:
//        instructions.emplace_back(i.type, i.text);
        return;
      case ASMLine::Type::Instruction:
//        instructions.emplace_back(ASMLine::Type::Directive, "; " + i.line_text);

        const auto head = instructions.size();
        translate_instruction(instructions, i.opcode, i.operand1, i.operand2);
        for_each(std::next(instructions.begin(), head), instructions.end(), [ text = i.line_text ](auto &ins){ ins.comment = text; });
        return;
    }
  } catch (const std::exception &e) {
    log(LogLevel::Error, i, e.what());
  }
}

bool optimize(std::vector<mos6502> &instructions)
{
  if (instructions.size() < 2) {
    return false;
  }

  for (size_t op = 0; op < instructions.size() - 1; ++op)
  {
    if (instructions[op].opcode == mos6502::OpCode::tya
        && instructions[op+1].opcode == mos6502::OpCode::tay)
    {
      instructions.erase(std::next(std::begin(instructions), op + 1), std::next(std::begin(instructions), op+2));
      return true;
    }
    if (instructions[op].opcode == mos6502::OpCode::sta
        && instructions[op+1].opcode == mos6502::OpCode::lda
        && instructions[op].op == instructions[op+1].op)
    {
      instructions.erase(std::next(std::begin(instructions), op + 1), std::next(std::begin(instructions), op+2));
      return true;
    }
  }

  for (size_t op = 0; op < instructions.size() - 1; ++op)
  {
    if (instructions[op].opcode == mos6502::OpCode::lda
        && instructions[op].op.type == Operand::Type::literal)
    {
      const auto operand = instructions[op].op;
      auto op2 = op+1;
      while (op2 < instructions.size() && instructions[op2].opcode == mos6502::OpCode::sta) {
        ++op2;
      }
      if (instructions[op2].opcode == mos6502::OpCode::lda
          && operand == instructions[op2].op)
      {
        instructions.erase(std::next(std::begin(instructions), op2), std::next(std::begin(instructions), op2+1));
        return true;
      }

    }
  }


  return false;
}

bool fix_long_branches(std::vector<mos6502> &instructions, int &branch_patch_count)
{
  std::map<std::string, size_t> labels;
  for (size_t op = 0; op < instructions.size(); ++op)
  {
    if (instructions[op].type == ASMLine::Type::Label) {
      labels[instructions[op].text] = op;
    }
  }

  for (size_t op = 0; op < instructions.size(); ++op)
  {
    if (instructions[op].is_branch && std::abs(static_cast<int>(labels[instructions[op].op.value]) - static_cast<int>(op)) * 3 > 255)
    {
      ++branch_patch_count;
      const auto going_to = instructions[op].op.value;
      const auto new_pos = "branch_patch_" + std::to_string(branch_patch_count);
      // uh-oh too long of a branch, have to convert this to a jump...
      if (instructions[op].opcode == mos6502::OpCode::bne) {
        instructions[op] = mos6502(mos6502::OpCode::beq, Operand(Operand::Type::literal, new_pos));
        instructions.insert(std::next(std::begin(instructions), op + 1), mos6502(mos6502::OpCode::jmp, Operand(Operand::Type::literal, going_to)));
        instructions.insert(std::next(std::begin(instructions), op + 2), mos6502(ASMLine::Type::Label, new_pos));
        return true;
      } else {
        throw std::runtime_error("Don't know how to reorg this branch");
      }
    }
  }
  return false;
}


bool fix_overwritten_flags(std::vector<mos6502> &instructions)
{
  if (instructions.size() < 3) {
    return false;
  }

  for (size_t op = 0; op < instructions.size(); ++op)
  {
    if (instructions[op].is_comparison) {
      auto op2 = op + 1;
      while (op2 < instructions.size() 
             && !instructions[op2].is_comparison
             && !instructions[op2].is_branch)
      {
        ++op2;
      }

      if (op2 < instructions.size() 
          && (op2 - op) > 1
          && instructions[op2-1].opcode != mos6502::OpCode::plp) {
        if (instructions[op2].is_comparison) {
          continue;
        }

        if (instructions[op2].is_branch) {
          // insert a pull of processor status before the branch
          instructions.insert(std::next(std::begin(instructions), op2), mos6502(mos6502::OpCode::plp));
          // insert a push of processor status after the comparison
          instructions.insert(std::next(std::begin(instructions), op+1), mos6502(mos6502::OpCode::php));

          return true;
        }

      }
    }
  }

  return false;
}


int main()
{
  std::regex Comment(R"(\s*\#.*)");
  std::regex Label(R"(^(\S+):.*)");
  std::regex Directive(R"(^\t(\..+))");
  std::regex UnaryInstruction(R"(^\t(\S+)\s+(\S+))");
  std::regex BinaryInstruction(R"(^\t(\S+)\s+(\S+),\s+(\S+))");
  std::regex Instruction(R"(^\t(\S+))");

  int lineno = 0;

  std::vector<i386> instructions;

  while (std::cin.good())
  {
    std::string line;
    getline(std::cin, line);

    try {
      std::smatch match;
      if (std::regex_match(line, match, Label))
      {
        instructions.emplace_back(lineno, line, ASMLine::Type::Label, match[1]);
      } else if (std::regex_match(line, match, Comment)) {
        // don't care about comments
      } else if (std::regex_match(line, match, Directive)) {
        instructions.emplace_back(lineno, line, ASMLine::Type::Directive, match[1]);
      } else if (std::regex_match(line, match, UnaryInstruction)) {
        instructions.emplace_back(lineno, line, ASMLine::Type::Instruction, match[1], match[2]);
      } else if (std::regex_match(line, match, BinaryInstruction)) {
        instructions.emplace_back(lineno, line, ASMLine::Type::Instruction, match[1], match[2], match[3]);
      } else if (std::regex_match(line, match, Instruction)) {
        instructions.emplace_back(lineno, line, ASMLine::Type::Instruction, match[1]);
      } else if (line == "") {
        //std::cout << "EmptyLine\n";
      } else {
        throw std::runtime_error("Unparsed Input, Line: " + std::to_string(lineno));
      }
    } catch (const std::exception &e) {
      log(LogLevel::Error, lineno, line, e.what());
    }

    ++lineno;
  }

  std::set<std::string> labels;

  for (const auto i : instructions)
  {
    if (i.type == ASMLine::Type::Label) {
      labels.insert(i.text);
    }
  }

  std::set<std::string> used_labels{"main"};

  for (const auto i : instructions)
  {
    if (i.type == ASMLine::Type::Instruction)
    {
      if (labels.count(i.operand1.value) != 0) {
        used_labels.insert(i.operand1.value);
        used_labels.insert(i.operand2.value);
      }
    }
  }

  // remove all labels and directives that we don't need
  instructions.erase(
    std::remove_if(std::begin(instructions), std::end(instructions),
        [&used_labels](const auto &i){
  //        if (i.type == ASMLine::Type::Directive) return true;
          if (i.type == ASMLine::Type::Label) {
            if (used_labels.count(i.text) == 0) {
              // remove all unused labels that aren't 'main'
              return true;
            }
          }
          return false;
        }
        ),
    std::end(instructions)
  );


  // remove everything up to the first label
  // this will probably leave some dead code around at some point
  // but it's a start
  /*
  instructions.erase(
      std::begin(instructions),
      std::find_if(std::begin(instructions),
                   std::end(instructions),
                   [](const auto &i){ return i.type == ASMLine::Type::Label; }
        )
      );
*/

  const auto new_labels = 
    [&used_labels](){
      std::map<std::string, std::string> result;
      for (const auto &l : used_labels) {
        auto newl = l;
        std::transform(newl.begin(), newl.end(), newl.begin(), [](const auto c) { return std::tolower(c); });
        newl.erase(std::remove_if(newl.begin(), newl.end(), [](const auto c){ return !std::isalnum(c); }), std::end(newl));
//        std::cout << "Old label: '" << l << "' new label: '" << newl << "'\n";
        result.emplace(std::make_pair(l, newl));
      }
      return result;
    }();


//  for (const auto &i : instructions)
//  {
//    std::cout << "'" << i.text << "' '" << i.operand1.value << "' '" << i.operand2.value << "'\n";
//  }


  for (auto &i : instructions)
  {
    if (i.type == ASMLine::Type::Label)
    {
//      std::cout << "Updating label " << i.text << '\n';;
      i.text = new_labels.at(i.text);
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

//  for (const auto &i : instructions)
//  {
//    std::cout << "'" << i.text << "' '" << i.operand1.value << "' '" << i.operand2.value << "'\n";
//  }

  std::vector<mos6502> new_instructions;

  for (const auto &i : instructions)
  {
    to_mos6502(i, new_instructions);
  }

  while (fix_overwritten_flags(new_instructions))
  {
    // do it however many times it takes
  }

  while (optimize(new_instructions))
  {
    // do it however many times it takes
  }

  int branch_patch_count = 0;
  while (fix_long_branches(new_instructions, branch_patch_count))
  {
    // do it however many times it takes
  }


  for (const auto i : new_instructions)
  {
    std::cout << i.to_string() << '\n';
  }

}
