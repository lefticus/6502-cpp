#ifndef INC_6502_CPP_OPTIMIZER_HPP
#define INC_6502_CPP_OPTIMIZER_HPP

#include "6502.hpp"
#include "personality.hpp"
#include <span>
#include <vector>


constexpr bool consume_directives(auto &begin, const auto &end)
{
  if (begin != end && begin->type == ASMLine::Type::Directive) {
    ++begin;
    return true;
  }
  return false;
}


constexpr bool consume_labels(auto &begin, const auto &end)
{
  if (begin != end && begin->type == ASMLine::Type::Label) {
    ++begin;
    return true;
  }
  return false;
}

constexpr bool is_opcode(const mos6502 &op, const auto... opcodes) { return ((op.opcode == opcodes) || ...); }

constexpr bool is_end_of_block(const auto &begin)
{
  if (begin->text.ends_with("__optimizable") || begin->op.value.ends_with("__optimizable")) {
        return false;
  }

  if (begin->type == ASMLine::Type::Label) { return true; }

  return is_opcode(*begin,
    mos6502::OpCode::jsr,
    mos6502::OpCode::jmp,
    mos6502::OpCode::bcc,
    mos6502::OpCode::bcs,
    mos6502::OpCode::beq,
    mos6502::OpCode::bne,
    mos6502::OpCode::bpl);
}

constexpr bool consume_end_of_block(auto &begin, const auto &end)
{
  if (begin != end && is_end_of_block(begin)) {
    ++begin;
    return true;
  }
  return false;
}

static std::vector<std::span<mos6502>> get_optimizable_blocks(std::vector<mos6502> &statements)
{
  std::vector<std::span<mos6502>> blocks;

  auto begin = std::begin(statements);
  auto end = std::end(statements);

  const auto find_end_of_block = [](auto &find_begin, const auto &find_end) {
    while (find_begin != find_end) {
      if (is_end_of_block(find_begin)) { return; }
      ++find_begin;
    }
  };

  while (begin != end) {
    while (consume_end_of_block(begin, end) || consume_directives(begin, end) || consume_labels(begin, end)) {}

    const auto block_start = begin;
    find_end_of_block(begin, end);

    blocks.emplace_back(block_start, begin);
  }

  return blocks;
}

static bool is_virtual_register_op(const mos6502 &op, const Personality &personality)
{
  for (int i = 0; i < 32; ++i) {
    if (personality.get_register(i).value == op.op.value) { return true; }
  }

  return false;
}

static bool optimize_dead_tax(std::span<mos6502> &block)
{
  for (auto itr = block.begin(); itr != block.end(); ++itr) {
    if (is_opcode(*itr, mos6502::OpCode::tax, mos6502::OpCode::tsx, mos6502::OpCode::ldx)) {
      for (auto inner = std::next(itr); inner != block.end(); ++inner) {
        if (is_opcode(*inner,
              mos6502::OpCode::txa,
              mos6502::OpCode::txs,
              mos6502::OpCode::stx,
              mos6502::OpCode::inx,
              mos6502::OpCode::dex,
              mos6502::OpCode::cpx)) {
          break;
        }
        if (is_opcode(*inner, mos6502::OpCode::tax, mos6502::OpCode::tsx, mos6502::OpCode::ldx)) {
          // redundant store found
          *itr = mos6502(ASMLine::Type::Directive, "; removed dead load of X: " + itr->to_string());
          return true;
        }
      }
    }
  }

  return false;
}

bool optimize_dead_sta(std::span<mos6502> &block, const Personality &personality)
{
  for (auto itr = block.begin(); itr != block.end(); ++itr) {
    if (itr->opcode == mos6502::OpCode::sta && is_virtual_register_op(*itr, personality)) {
      for (auto inner = std::next(itr); inner != block.end(); ++inner) {
        if (not inner->op.value.starts_with("#<(") && inner->op.value.find('(') != std::string::npos) {
          // this is an indexed operation, which is risky to optimize a sta around on the virtual registers,
          // so we'll skip this block
          break;
        }
        if (inner->op.value == itr->op.value) {
          if (is_opcode(*inner, mos6502::OpCode::sta)) {
            // redundant store found
            *itr = mos6502(ASMLine::Type::Directive, "; removed dead store of a: " + itr->to_string());
            return true;
          } else {
            // someone else is operating on us, time to abort
            break;
          }
        }
      }
    }
  }

  return false;
}

bool optimize_redundant_ldy(std::span<mos6502> &block)
{
  for (auto itr = block.begin(); itr != block.end(); ++itr) {
    if (itr->opcode == mos6502::OpCode::ldy && itr->op.value.starts_with('#')) {
      for (auto inner = std::next(itr); inner != block.end(); ++inner) {
        if (is_opcode(*inner,
              mos6502::OpCode::cpy,
              mos6502::OpCode::tya,
              mos6502::OpCode::tay,
              mos6502::OpCode::sty,
              mos6502::OpCode::iny,
              mos6502::OpCode::dey)) {
          break;// break, these all operate on Y
        }
        // we found a matching ldy
        if (is_opcode(*inner, mos6502::OpCode::ldy)) {
          // with the same value
          // note: this operation is only safe because we know that our system only uses Y
          // for index operations and we don't rely (or even necessarily *want* the changes to N,Z)
          if (inner->op.value == itr->op.value) {
            *inner = mos6502(ASMLine::Type::Directive, "; removed redundant ldy: " + inner->to_string());
            return true;
          } else {
            break;
          }
        }
      }
    }
  }

  return false;
}

bool optimize_redundant_lda(std::span<mos6502> &block, const Personality &personality)
{
  // look for a literal or virtual register load into A
  // that is redundant later
  for (auto itr = block.begin(); itr != block.end(); ++itr) {
    if (itr->opcode == mos6502::OpCode::lda
        && (itr->op.value.starts_with('#') || is_virtual_register_op(*itr, personality))) {
      for (auto inner = std::next(itr); inner != block.end(); ++inner) {
        if (is_opcode(*inner,
              mos6502::OpCode::tay,
              mos6502::OpCode::tax,
              mos6502::OpCode::sta,
              mos6502::OpCode::pha,
              mos6502::OpCode::nop)) {
          continue;// OK to skip instructions that don't modify A or change flags
        }
        if (inner->type == ASMLine::Type::Directive) {
          continue;// OK to skip directives
        }
        if (is_opcode(*inner, mos6502::OpCode::lda)) {
          if (inner->op == itr->op) {
            // we found a matching lda, after an sta, we can remove it
            *inner = mos6502(ASMLine::Type::Directive, "; removed redundant lda: " + inner->to_string());
            return true;
          } else {
            break;
          }
        }

        break;// we can only optimize around tax and comments right now
      }
    }
  }

  return false;
}

bool optimize_redundant_lda_after_sta(std::span<mos6502> &block)
{
  for (auto itr = block.begin(); itr != block.end(); ++itr) {
    if (itr->opcode == mos6502::OpCode::sta) {
      for (auto inner = std::next(itr); inner != block.end(); ++inner) {
        if (is_opcode(*inner,
              mos6502::OpCode::tax,
              mos6502::OpCode::tay,
              mos6502::OpCode::clc,
              mos6502::OpCode::sec,
              mos6502::OpCode::sta,
              mos6502::OpCode::pha,
              mos6502::OpCode::txs,
              mos6502::OpCode::php,
              mos6502::OpCode::sty,
              mos6502::OpCode::nop)) {
          continue;// OK to skip instructions that don't modify A or change flags
        }
        if (inner->type == ASMLine::Type::Directive) {
          continue;// OK to skip directives
        }
        if (is_opcode(*inner, mos6502::OpCode::lda)) {
          if (inner->op == itr->op) {
            // we found a matching lda, after a sta, we can remove it
            *inner = mos6502(ASMLine::Type::Directive, "; removed redundant lda: " + inner->to_string());
            return true;
          } else {
            break;
          }
        }

        break;// we can only optimize around tax and comments right now
      }
    }
  }

  return false;
}

bool optimize(std::vector<mos6502> &instructions, [[maybe_unused]] const Personality &personality)
{


  // remove unused flag-fix-up blocks
  // it might make sense in the future to only insert these if determined they are needed?
  for (size_t op = 10; op < instructions.size(); ++op) {
    if (instructions[op].opcode == mos6502::OpCode::lda || instructions[op].opcode == mos6502::OpCode::bcc
        || instructions[op].opcode == mos6502::OpCode::bcs || instructions[op].opcode == mos6502::OpCode::ldy
        || instructions[op].opcode == mos6502::OpCode::inc || instructions[op].opcode == mos6502::OpCode::clc
        || instructions[op].opcode == mos6502::OpCode::sec || instructions[op].text.starts_with("; Handle N / S")) {
      if (instructions[op - 1].text == "; END remove if next is lda, bcc, bcs, ldy, inc, clc, sec"
          || (instructions[op - 2].text == "; END remove if next is lda, bcc, bcs, ldy, inc, clc, sec"
              && instructions[op - 1].type == ASMLine::Type::Directive)) {
        for (size_t inner_op = op - 1; inner_op > 1; --inner_op) {
          instructions[inner_op] =
            mos6502(ASMLine::Type::Directive, "; removed unused flag fix-up: " + instructions[inner_op].to_string());

          if (instructions[inner_op].text.find("; BEGIN") != std::string::npos) { return true; }
        }
      }
    }
  }

  // replace use of __zero_reg__ with literal 0
  for (auto &op : instructions) {
    if (op.type == ASMLine::Type::Instruction && op.op.type == Operand::Type::literal
        && op.op.value == personality.get_register(1).value && op.opcode != mos6502::OpCode::sta) {
      // replace use of zero reg with literal 0
      const auto old_string = op.to_string();
      op.op.value = "#0";
      op.comment = "replaced use of register 1 with a literal 0, because of AVR GCC __zero_reg__  ; " + old_string;
    }
  }

  bool optimizer_run = false;
  for (auto &block : get_optimizable_blocks(instructions)) {
    const bool block_optimized = optimize_redundant_lda_after_sta(block)
                      || optimize_dead_sta(block, personality) || optimize_dead_tax(block)
                      || optimize_redundant_ldy(block) || optimize_redundant_lda(block, personality);

    optimizer_run = optimizer_run || block_optimized;
  }
  return optimizer_run;
}

#endif// INC_6502_CPP_OPTIMIZER_HPP
