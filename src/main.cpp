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

Operand get_register(int reg_num) {
  switch (reg_num) {
    case 1: return Operand(Operand::Type::literal, "$fb");
    case 2: return Operand(Operand::Type::literal, "$fc");
    case 3: return Operand(Operand::Type::literal, "$fd");
  };
  throw std::runtime_error("Cannot translate instruction");
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
    rts
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
      case OpCode::unknown:
        break;
    }
    return false;
  }

  static bool get_is_comparison(const OpCode o) {
    switch (o) {
      case OpCode::cmp:
      case OpCode::cpy:
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
      case OpCode::unknown:
        return "";
    };

    return "";
  }

  std::string to_string() const
  {
    switch (type) {
      case ASMLine::Type::Label:
        return text + ':';
      case ASMLine::Type::Directive:
      case ASMLine::Type::Instruction:
        return '\t' + text + ' ' + op.value;
    };
    throw std::runtime_error("Unable to render: " + text);
  }


  OpCode opcode = OpCode::unknown;
  Operand op;
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
    sall,
    orl,
    orb,
    rep,
    pushl
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
          if (o == "addl") return OpCode::addl;
          if (o == "sall") return OpCode::sall;
          if (o == "orl") return OpCode::orl;
          if (o == "andb") return OpCode::andb;
          if (o == "orb") return OpCode::orb;
          if (o == "decb") return OpCode::decb;
          if (o == "incb") return OpCode::incb;
          if (o == "rep") return OpCode::rep;
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
      if (o == "%al" || o == "%eax") {
        return Operand(Operand::Type::reg, 0);
      } else if (o == "%bl" || o == "%ebx") {
        return Operand(Operand::Type::reg, 1);
      } else if (o == "%cl" || o == "%ecx") {
        return Operand(Operand::Type::reg, 2);
      } else if (o == "%dl" || o == "%edx") {
        return Operand(Operand::Type::reg, 3);
      } else if (o == "%di") {
        return Operand(Operand::Type::reg, 6);
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
    case i386::OpCode::movb:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::ldy, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::sty, o2);
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::lda, o1);
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::ldy, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::sty, get_register(o2.reg_num));
      } else if (o1.type == Operand::Type::reg && o1.reg_num == 0 && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
      } else if (o1.type == Operand::Type::reg && o1.reg_num == 0 && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::sta, o2);
      } else if (o1.type == Operand::Type::reg && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::ldy, get_register(o1.reg_num));
        instructions.emplace_back(mos6502::OpCode::sty, o2);
      } else {
        throw std::runtime_error("Cannot translate instruction");
      }
      break;
    case i386::OpCode::movl:
      if (o1.type == Operand::Type::reg && o1.reg_num == 0 && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
      } else {
        throw std::runtime_error("Cannot translate instruction");
      }
      break;
    case i386::OpCode::orb:
    case i386::OpCode::orl:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::tay); // transfer memory through A register, pushing and popping around it
        instructions.emplace_back(mos6502::OpCode::lda, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::ORA, o2);
        instructions.emplace_back(mos6502::OpCode::sta, o2);
        instructions.emplace_back(mos6502::OpCode::tya);
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::ORA, Operand(o1.type, fixup_8bit_literal(o1.value)));
      } else {
        throw std::runtime_error("Cannot translate instruction");
      }
      break;

    case i386::OpCode::movzbl:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::lda, o1);
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::ldy, o1);
        instructions.emplace_back(mos6502::OpCode::sty, get_register(o2.reg_num));
      } else {
        throw std::runtime_error("Cannot translate instruction");
      }
      break;
    case i386::OpCode::sarl:
    case i386::OpCode::shrb:
      if (o1.type == Operand::Type::reg || o2.type == Operand::Type::reg) {
        const auto do_shift = [&instructions](const int reg_num) {
          if (reg_num == 0) {
            instructions.emplace_back(mos6502::OpCode::lsr, Operand(Operand::Type::literal, "a"));
          } else {
            instructions.emplace_back(mos6502::OpCode::lsr, get_register(reg_num));
          }
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
        throw std::runtime_error("Cannot translate instruction");
      }
      break;
    case i386::OpCode::testb:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::pha);
        instructions.emplace_back(mos6502::OpCode::AND, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::tay);
        instructions.emplace_back(mos6502::OpCode::pla);
        instructions.emplace_back(mos6502::OpCode::cpy, Operand(Operand::Type::literal, "#0"));


    //    instructions.emplace_back(mos6502::OpCode::cmp, Operand(Operand::Type::literal, "#$0"));
      } else if (o1.type == Operand::Type::reg && o1.reg_num == 0 && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        //transfer A to Y, to set the status flags appropriately
        instructions.emplace_back(mos6502::OpCode::tay);
      } else if (o1.type == Operand::Type::reg && o2.type == Operand::Type::reg && o1.reg_num == o2.reg_num) {
        instructions.emplace_back(mos6502::OpCode::ldy, get_register(o1.reg_num));
      } else {
        throw std::runtime_error("Cannot translate instruction");
      }
      break;
    case i386::OpCode::decb:
    case i386::OpCode::decl:
      if (o1.type == Operand::Type::reg && o1.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::sbc, Operand(Operand::Type::literal, "#1"));
      } else if (o1.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::dec, get_register(o1.reg_num));
      } else {
        instructions.emplace_back(mos6502::OpCode::dec, o1);
      }
      break;
    case i386::OpCode::incb:
    case i386::OpCode::incl:
      if (o1.type == Operand::Type::reg && o1.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::adc, Operand(Operand::Type::literal, "#1"));
      } else if (o1.type == Operand::Type::reg) {
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
    case i386::OpCode::xorl:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::eor, Operand(o1.type, fixup_8bit_literal(o1.value)));
      } else if (o1.type == Operand::Type::reg && o2.reg_num == 0 && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        // cheater shortcut on x86 to 0 out a register
        instructions.emplace_back(mos6502::OpCode::lda, Operand(Operand::Type::literal, "#0"));
      } else if (o1.type == Operand::Type::reg && o2.type == Operand::Type::reg && o1.reg_num == o2.reg_num) {
        // cheater shortcut on x86 to 0 out a register
        instructions.emplace_back(mos6502::OpCode::ldy, Operand(Operand::Type::literal, "#0"));
        instructions.emplace_back(mos6502::OpCode::sty, get_register(o1.reg_num));
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::tay); // transfer memory through A register, pushing and popping around it
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o2.reg_num));
        instructions.emplace_back(mos6502::OpCode::eor, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
        instructions.emplace_back(mos6502::OpCode::tya);
      } else {
        throw std::runtime_error("Cannot translate instruction");
      }
      break;
    case i386::OpCode::addl:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::adc, Operand(o1.type, fixup_8bit_literal(o1.value)));
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::tay); // transfer memory through A register, pushing and popping around it
        instructions.emplace_back(mos6502::OpCode::lda, get_register(o2.reg_num));
        instructions.emplace_back(mos6502::OpCode::adc, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::sta, get_register(o2.reg_num));
        instructions.emplace_back(mos6502::OpCode::tya);
      } else {
        throw std::runtime_error("Cannot translate instruction");
      }
      break;
    case i386::OpCode::cmpb:
      if (o1.type == Operand::Type::reg && o1.reg_num == 0 && o2.type == Operand::Type::reg) {
        instructions.emplace_back(mos6502::OpCode::cmp, get_register(o2.reg_num));
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::cmp, Operand(o1.type, fixup_8bit_literal(o1.value)));
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::literal) {
        instructions.emplace_back(mos6502::OpCode::ldy, o2);
//        if (o1.value != "$0") {
          instructions.emplace_back(mos6502::OpCode::cpy, Operand(o1.type, fixup_8bit_literal(o1.value)));
//        }
      } else {
        throw std::runtime_error("Cannot translate instruction");
      }
      break;
    case i386::OpCode::andb:
    case i386::OpCode::andl:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::AND, Operand(o1.type, fixup_8bit_literal(o1.value)));
      } else if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg)  {
        const auto reg = get_register(o2.reg_num);
        instructions.emplace_back(mos6502::OpCode::tay); // transfer memory through A register, pushing and popping around it
        instructions.emplace_back(mos6502::OpCode::lda, reg);
        instructions.emplace_back(mos6502::OpCode::AND, Operand(o1.type, fixup_8bit_literal(o1.value)));
        instructions.emplace_back(mos6502::OpCode::sta, reg);
        instructions.emplace_back(mos6502::OpCode::tya); // transfer memory through A register, pushing and popping around it
      } else {
        throw std::runtime_error("Cannot translate instruction");
      }
      break;
    case i386::OpCode::subl:
      if (o1.type == Operand::Type::literal && o2.type == Operand::Type::reg && o2.reg_num == 0) {
        instructions.emplace_back(mos6502::OpCode::sbc, Operand(o1.type, fixup_8bit_literal(o1.value)));
      } else {
        throw std::runtime_error("Cannot translate instruction");
      }
      break;
    default:
      throw std::runtime_error("Cannot translate instruction");

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
        instructions.emplace_back(i.type, i.text);
        return;
      case ASMLine::Type::Instruction:
        translate_instruction(instructions, i.opcode, i.operand1, i.operand2);
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
  }

  return false;
}


bool fix_overwritten_flags(std::vector<mos6502> &instructions)
{
  if (instructions.size() < 3) {
    return false;
  }

  for (size_t op = 0; op < instructions.size() - 2; ++op)
  {
    if (instructions[op].is_comparison
        && !instructions[op + 1].is_comparison
        && !instructions[op + 1].is_branch
        && instructions[op + 2].is_branch) {
      const auto opcode_to_duplicate = op + 1;
      const auto new_pos_1 = op + 3;
      const auto new_pos_2 = [&instructions, branch = instructions[op + 2].op.value](){
        for (size_t cur_op = 0; cur_op < instructions.size(); ++cur_op) {
          if (instructions[cur_op].type == ASMLine::Type::Label
              && instructions[cur_op].text == branch) {
            return cur_op + 1;
          }
        }
        throw std::runtime_error("Unable to find matching branch!");
      }();

      instructions.insert(std::next(std::begin(instructions), std::max(new_pos_1, new_pos_2)), instructions[opcode_to_duplicate]);
      instructions.insert(std::next(std::begin(instructions), std::min(new_pos_1, new_pos_2)), instructions[opcode_to_duplicate]);
      instructions.erase(std::next(std::begin(instructions), opcode_to_duplicate));

      return true;
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


  for (const auto i : new_instructions)
  {
    std::cout << i.to_string() << '\n';
  }

}
