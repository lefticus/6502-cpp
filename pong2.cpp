#include <cstdint>
#include <array>
#include <utility>
#include <algorithm>

constexpr uint16_t SPRITE_DATA_POINTERS = 2040;
constexpr uint16_t VIDEO_REGISTERS = 53248;
constexpr uint16_t SPRITE_ENABLE_BITS = VIDEO_REGISTERS + 21;
constexpr uint16_t SPRITE_EXPAND_HORIZONTAL = VIDEO_REGISTERS + 29;
constexpr uint16_t SPRITE_EXPAND_VERTICAL = VIDEO_REGISTERS + 23;
constexpr uint16_t SPRITE_POSITION_REGISTERS = VIDEO_REGISTERS;
constexpr uint16_t SPRITE_COLLISIONS = VIDEO_REGISTERS + 30;
constexpr uint16_t SPRITE_MULTICOLOR = VIDEO_REGISTERS + 28;
constexpr uint16_t VIDEO_MEMORY = 1024;
constexpr uint8_t  SPRITE_STARTING_BANK = 192;


namespace {

volatile uint8_t &memory(const uint16_t loc)
{
  return *reinterpret_cast<uint8_t *>(loc);
}

constexpr bool test_bit(const uint8_t data, const uint8_t bit)
{
  return (data & (1 << bit)) != 0;
};

void set_bit(const uint16_t loc, const uint8_t bitnum, bool val)
{
  if (val) {
    memory(loc) |= (1 << bitnum);
  } else {
    memory(loc) &= (0xFF ^ (1 << bitnum));
  }
}

void write_multi_color_pixel(uint16_t)
{
}

void write_pixel(uint16_t)
{
}


template<typename D1, typename D2, typename D3, typename D4, typename ...  D >
  void write_multi_color_pixel(uint16_t loc, D1 d1, D2 d2, D3 d3, D4 d4, D ... d)
{
  memory(loc) = (d1 << 6) | (d2 << 4) | (d3 << 2) | d4;
  write_multi_color_pixel(loc + 1, d...);
}

template<typename D1, typename D2, typename D3, typename D4, typename D5, typename D6, typename D7, typename D8, typename ...  D >
  void write_pixel(uint16_t loc, D1 d1, D2 d2, D3 d3, D4 d4, D5 d5, D6 d6, D7 d7, D8 d8, D ... d)
{
  memory(loc) = (d1 << 7) | (d2 << 6) | (d3 << 5) | (d4 << 4) | (d5 << 3) | (d6 << 2) | (d7 << 1) | d8;
  write_pixel(loc + 1, d...);
}


template<typename ... D>
  void make_sprite(uint8_t memory_loc, D ... d)
{
  if constexpr (sizeof...(d) == 12 * 21) {
    write_multi_color_pixel((SPRITE_STARTING_BANK + memory_loc) * 64, d...);
  } else {
    write_pixel((SPRITE_STARTING_BANK + memory_loc) * 64, d...);
  }
}

void enable_sprite(const uint8_t sprite_number, const uint8_t memory_loc,
                   const bool multicolor,
                   const bool double_width, const bool double_height)
{
  set_bit(SPRITE_ENABLE_BITS, sprite_number, true);
  memory(SPRITE_DATA_POINTERS + sprite_number) = SPRITE_STARTING_BANK + memory_loc;
  set_bit(SPRITE_EXPAND_HORIZONTAL, sprite_number, double_width);
  set_bit(SPRITE_EXPAND_VERTICAL, sprite_number, double_height);
  set_bit(SPRITE_MULTICOLOR, sprite_number, multicolor);
}

void display(uint8_t x, uint8_t y, uint8_t val)
{
  memory(VIDEO_MEMORY + y * 40 + x) = val;
}

auto joystick(const uint8_t port) {
  struct State{
    State(const uint8_t portdata) 
      : up(!test_bit(portdata,0)),
        down(!test_bit(portdata,1)),
        left(!test_bit(portdata,2)),
        right(!test_bit(portdata,3)),
        fire(!test_bit(portdata,4))
    {
    }

    bool up;
    bool down;
    bool left;
    bool right;
    bool fire;
  };

  if (port == 2) {
    return State(memory(56320));
  } else {
    return State(memory(56321));
  }
};


volatile uint8_t &sprite_x(const uint8_t sprite_num)
{
  return memory(SPRITE_POSITION_REGISTERS + sprite_num * 2);
};

volatile uint8_t &sprite_y(const uint8_t sprite_num)
{
  return memory(SPRITE_POSITION_REGISTERS + sprite_num * 2 + 1);
};

}

int main()
{
  make_sprite(0,
              0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
              0,0,0,0,0,0,1,1,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
              0,0,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
              0,0,0,0,0,1,1,1,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
              0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
              0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
              0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
              0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
              0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,
              0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
              0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
              0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
              0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
              0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
             );

  enable_sprite(0, 0, false, false, false);

  make_sprite(1,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,2,2,0,0,0,0,0,
              0,0,0,0,0,3,3,0,0,0,0,0,
              0,0,0,0,0,1,1,0,0,0,0,0,
              0,0,0,0,0,3,3,0,0,0,0,0,
              0,0,0,0,0,1,1,0,0,0,0,0,
              0,0,0,0,0,3,3,0,0,0,0,0
             );

  enable_sprite(1, 1, true, false, true);
  enable_sprite(2, 1, true, false, true);


  const auto sprite_collisions = []() {
    const auto collisions = memory(SPRITE_COLLISIONS);
    memory(SPRITE_COLLISIONS) = 0;
    struct Col_Data {
      bool sprite0;
      bool sprite1;
      bool sprite2;
      bool sprite3;
      bool sprite4;
      bool sprite5;
      bool sprite6;
      bool sprite7;
    };

    return Col_Data{test_bit(collisions, 0),test_bit(collisions, 1),test_bit(collisions, 2),test_bit(collisions, 3),
                    test_bit(collisions, 4),test_bit(collisions, 5),test_bit(collisions, 6),test_bit(collisions, 7)};
  };

  std::pair<int8_t, int8_t> ball_vec{1,1};

  const auto reset_ball = [&]{
    sprite_x(0) = 255/2;
    sprite_y(0) = 255/2;
  };

  reset_ball();

  sprite_x(1) = 15;
  sprite_y(1) = 255/2;

  sprite_x(2) = 255;
  sprite_y(2) = 255/2;

  memory(53280) = 12;
  memory(53281) = 0;


  struct Player
  {
    void update_position()
    {
      if (const auto joy = joystick(player_num);
          joy.up)
      {
        sprite_y(player_num) += 3;
      } else if (joy.down) {
        sprite_y(player_num) -= 3;
      }
    };

    void scored() {
      ++score;
    }

    const uint8_t player_num;
    uint8_t score = '0';
  };

  Player p1{1};
  Player p2{2};

  const auto raster_off_screen = [](){
    return memory(53266) == 250 && !test_bit(memory(53265),7);
  };

  while (true) {
    if (raster_off_screen())
    {
      if (const auto collisions = sprite_collisions();
          collisions.sprite0 && (collisions.sprite1 || collisions.sprite2))
      {
        std::get<0>(ball_vec) *= -1; //invert ball x velocity
        // "bounce" ball out of collision area
        sprite_x(0) += std::get<0>(ball_vec);
      }

      // Update paddle positions
      p1.update_position();
      p2.update_position();

      // ball hit the top or bottom wall
      if (const auto ball_y = sprite_y(0) += std::get<1>(ball_vec);
          ball_y == 45 || ball_y == 235) 
      {
        std::get<1>(ball_vec) *= -1;
      }

      if (const auto ball_x = sprite_x(0) += std::get<0>(ball_vec); 
          ball_x == 1) {
        // ball hit left wall, player 2 scored
        p2.scored();
        reset_ball();
      } else if (ball_x == 255) {
        // ball hit right wall, player 1 scored
        p1.scored();
        reset_ball();
      }

      display(10, 12, p1.score);
      display(30, 12, p2.score);
    }
  }
}

