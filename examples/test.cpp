#include <cstdint>

enum Colors : uint8_t
{
  WHITE=0x01
};

volatile uint8_t &memory_loc(const uint16_t loc)
{
  return *reinterpret_cast<volatile uint8_t *>(loc);
}

void decrement_border_color()
{
  --memory_loc(0xd020);
}

void increment_border_color()
{
  ++memory_loc(0xd020);
}

bool joystick_down()
{
  uint8_t joystick_state = memory_loc(0xDC00);
  return (joystick_state & 0x2) == 0;
}

int main()
{
  const auto background_color = [](Colors col) {
    memory_loc(0xd021) = static_cast<uint8_t>(col);
  };

  background_color(Colors::WHITE);

  while(true) {
    if (joystick_down()) {
      increment_border_color();
    } else {
      decrement_border_color();
    }
  }
}
