#include <cstdint>

enum class Colors : uint8_t
{
  WHITE=0x01,
  BLACK=0x00
};

inline volatile uint8_t &memory_loc(const uint16_t loc)
{
  return *reinterpret_cast<volatile uint8_t *>(loc);
}

inline void decrement_border_color()
{
  --memory_loc(0xd020);
}

inline void increment_border_color()
{
  ++memory_loc(0xd020);
}

inline bool joystick_down()
{
  uint8_t joystick_state = memory_loc(0xDC00);
  return (joystick_state & 0x2) == 0;
}

int main()
{
  const auto background_color = [](Colors col) {
    memory_loc(0xd021) = static_cast<uint8_t>(col);
  };

  const auto border_color = [](Colors col) {
    memory_loc(0xd020) = static_cast<uint8_t>(col);
  };

  background_color(Colors::WHITE);

  while(true) {
    if (joystick_down()) {
      border_color(Colors::WHITE);
    } else {
      border_color(Colors::BLACK);
    }
  }
}
