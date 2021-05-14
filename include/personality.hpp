#ifndef INC_6502_CPP_PERSONALITY_HPP
#define INC_6502_CPP_PERSONALITY_HPP

#include <vector>
#include "6502.hpp"

class Personality
{
public:
  virtual void                  insert_autostart_sequence(std::vector<mos6502> &new_instructions) const = 0;
  [[nodiscard]] virtual Operand get_register(const int reg_num) const                                   = 0;

  virtual ~Personality()           = default;
  Personality(const Personality &) = delete;
  Personality(Personality &&)      = delete;
  Personality &operator=(const Personality &) = delete;
  Personality &operator=(Personality &&) = delete;

protected:
  Personality() = default;
};

#endif//INC_6502_CPP_PERSONALITY_HPP
