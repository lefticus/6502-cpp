#include <cstdint>

enum class Colors : uint8_t
{
  WHITE=0x01,
  BLACK=0x00
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

  const auto border_color = [](Colors col) {
    memory_loc(0xd020) = static_cast<uint8_t>(col);
  };

  background_color(Colors::WHITE);

  bool joydown = joystick_down();

  while(true) {
    bool newjoydown = joystick_down();
    if (joydown != newjoydown) {
      increment_border_color();
      joydown = newjoydown;
    }
  }
}
