#ifndef INC_6502_CPP_OPTIMIZER_HPP
#define INC_6502_CPP_OPTIMIZER_HPP

#include "6502.hpp"
#include "personality.hpp"
#include <vector>

bool optimize(std::vector<mos6502> &instructions, const Personality &personality)
{
  // return false;

  if (instructions.size() < 2) { return false; }

  const auto next_instruction = [&instructions](auto i) {
    do {
      ++i;
    } while (i < instructions.size() && instructions[i].type == ASMLine::Type::Directive);
    return i;
  };


  // remove unused flag-fix-up blocks
  // it might make sense in the future to only insert these if determined they are needed?
  for (size_t op = 10; op < instructions.size(); ++op) {
    if (instructions[op].opcode == mos6502::OpCode::lda || instructions[op].opcode == mos6502::OpCode::bcc
        || instructions[op].opcode == mos6502::OpCode::bcs) {
      if (instructions[op - 1].text == "; END remove if next is lda, bcc, bcs"
          || (instructions[op - 2].text == "; END remove if next is lda, bcc, bcs"
              && instructions[op - 1].type == ASMLine::Type::Directive)) {
        for (size_t inner_op = op - 1; inner_op > 1; --inner_op) {
          instructions[inner_op] =
            mos6502(ASMLine::Type::Directive, "; removed unused flag fix-up: " + instructions[inner_op].to_string());

          if (instructions[inner_op].text.find("; BEGIN") != std::string::npos) { return true; }
        }
      }
    }
  }


  // look for redundant load of lda after a tax
  for (size_t op = 0; op < instructions.size() - 3; ++op) {
    if (instructions[op].opcode == mos6502::OpCode::sta && instructions[op + 1].opcode == mos6502::OpCode::tax
        && instructions[op + 2].opcode == mos6502::OpCode::lda
        && instructions[op].op.value == instructions[op + 2].op.value) {
      instructions[op + 2] =
        mos6502(ASMLine::Type::Directive, "; removed redundant lda: " + instructions[op + 2].to_string());
      return true;
    }
  }

  // look for redundant stores to 0-page registers with sta
  for (size_t op = 0; op < instructions.size(); ++op) {
    // todo, make sure this is in the register map
    if (instructions[op].opcode == mos6502::OpCode::sta && instructions[op].op.value.size() == 3) {
      for (size_t next_op = op + 1; next_op < instructions.size(); ++next_op) {
        if (instructions[next_op].opcode != mos6502::OpCode::sta
            && instructions[next_op].op.value == instructions[op].op.value) {
          // we just found a use of ourselves back, abort the search, there's probably something else going on
          break;
        }
        if (instructions[next_op].opcode == mos6502::OpCode::lda
            && instructions[next_op].op.value != instructions[op].op.value) {
          // someone just loaded lda with a different value, so we need to abort!
          break;
        }

        // abort at label
        if (instructions[next_op].type == ASMLine::Type::Label) { break; }

        if (instructions[next_op].opcode == mos6502::OpCode::sta
            && instructions[next_op].op.value == instructions[op].op.value) {
          // looks like we found a redundant store, remove the first one
          instructions[op] =
            mos6502(ASMLine::Type::Directive, "; removed redundant sta: " + instructions[op].to_string());
          return true;
        }
      }
    }
  }

  for (size_t op = 0; op < instructions.size() - 1; ++op) {
    // look for a transfer of A -> X immediately followed by LDX
    if (instructions[op].opcode == mos6502::OpCode::sta) {
      const auto next_op = next_instruction(op);
      if (instructions[next_op].opcode == mos6502::OpCode::ldx && instructions[op].op == instructions[next_op].op) {
        auto last_comment = instructions[next_op].comment;
        instructions[next_op] = mos6502(mos6502::OpCode::tax);
        instructions[next_op].comment = last_comment;
        return true;
      }
    }
  }

  for (size_t op = 0; op < instructions.size() - 1; ++op) {
    // look for a transfer of A -> X immediately followed by LDX
    if (instructions[op].opcode == mos6502::OpCode::tax) {
      const auto next_op = next_instruction(op);
      if (instructions[next_op].opcode == mos6502::OpCode::ldx) {
        instructions[op] =
          mos6502(ASMLine::Type::Directive, "; removed redundant tax: " + instructions[op].to_string());
        return true;
      }
    }
  }

  for (size_t op = 0; op < instructions.size() - 1; ++op) {
    // look for a transfer of Y -> A immediately followed by A -> Y
    if (instructions[op].opcode == mos6502::OpCode::tya) {
      const auto next_op = next_instruction(op);
      if (instructions[next_op].opcode == mos6502::OpCode::tay) {
        instructions[op] =
          mos6502(ASMLine::Type::Directive, "; removed redundant tay: " + instructions[op].to_string());
        return true;
      }
    }
  }

  for (size_t op = 0; op < instructions.size() - 1; ++op) {
    // look for a store A -> loc immediately followed by loc -> A
    if (instructions[op].opcode == mos6502::OpCode::sta) {

      const auto next = next_instruction(op);
      if (instructions[next].opcode == mos6502::OpCode::lda && instructions[next].op == instructions[op].op) {
        instructions[next] =
          mos6502(ASMLine::Type::Directive, "; removed redundant lda: " + instructions[next].to_string());
        return true;
      }
    }
  }

  for (auto &op : instructions) {
    if (op.type == ASMLine::Type::Instruction && op.op.type == Operand::Type::literal
        && op.op.value == personality.get_register(1).value && op.opcode != mos6502::OpCode::sta) {
      // replace use of zero reg with literal 0
      op.op.value = "#0";
    }
  }

  for (size_t op = 0; op < instructions.size() - 1; ++op) {
    if (instructions[op].opcode == mos6502::OpCode::ldy && instructions[op].op.type == Operand::Type::literal) {
      auto op2 = op + 1;

      while (op2 < instructions.size() && (instructions[op2].type != ASMLine::Type::Label)) {
        // while inside this label
        if (instructions[op2].opcode == mos6502::OpCode::ldy) {

          if (instructions[op2].op.value == instructions[op].op.value) {
            instructions[op2] =
              mos6502(ASMLine::Type::Directive, "; removed redundant ldy: " + instructions[op2].to_string());
            return true;
          } else {
            // if we ldy with any other value in this block then abort
            break;
          }
        }
        ++op2;
      }
    }
  }


  for (size_t op = 0; op < instructions.size() - 1; ++op) {
    if (instructions[op].opcode == mos6502::OpCode::lda && instructions[op].op.type == Operand::Type::literal) {
      const auto operand = instructions[op].op;
      auto       op2 = op + 1;
      // look for multiple stores of the same value
      while (
        op2 < instructions.size()
        && (instructions[op2].opcode == mos6502::OpCode::sta || instructions[op2].type == ASMLine::Type::Directive)) {
        ++op2;
      }
      if (instructions[op2].opcode == mos6502::OpCode::lda && operand == instructions[op2].op) {
        instructions[op2] =
          mos6502(ASMLine::Type::Directive, "; removed redundant lda: " + instructions[op2].to_string());
        return true;
      }
    }
  }

  return false;
}

#endif// INC_6502_CPP_OPTIMIZER_HPP
