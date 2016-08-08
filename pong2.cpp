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
constexpr auto starting_bank = 192;


namespace {

volatile uint8_t &memory(const uint16_t loc)
{
  return *reinterpret_cast<uint8_t *>(loc);
}

struct Color
{
  enum class Name : uint8_t {
    Black = 0,
    White = 1,
    Red = 2,
    Cyan = 3,
    Purple = 4,
    Green = 5,
    Blue = 6,
    Yellow = 7,
    Orange = 8,
    Brown = 9,
    LightRed = 10,
    DarkGrey = 11,
    Grey = 12,
    LightGreen = 13,
    LightBlue = 14,
    LightGrey = 15
  };
  
  constexpr Color(const Name t_name, const uint8_t t_r, const uint8_t t_g, const uint8_t t_b)
    : name(t_name), r(t_r), g(t_g), b(t_b)
    {
    }
    
  Name name;
  uint8_t r;
  uint8_t g;
  uint8_t b;
};
 
template<typename T>
constexpr auto square(T t)
{
  return t*t;
}
  
constexpr auto distance(const Color &lhs, const uint8_t t_r, const uint8_t t_g, const uint8_t t_b)
{
  // http://stackoverflow.com/questions/4754506/color-similarity-distance-in-rgba-color-space
  return square(lhs.r - t_r) + square(lhs.g - t_g) + square(lhs.b - t_b);
}
  
template<typename T>
constexpr auto distance_table(T colors, const uint8_t t_r, const uint8_t t_g, const uint8_t t_b)
{
  std::array<int, colors.size()> distances{};
  auto pos = std::begin(distances);
  
  
  for (const auto &color : colors) {
    *pos = distance(color, t_r, t_g, t_b);
    ++pos;    
  }
  
  return distances;  
}
  
template<typename T>
  constexpr Color nearest_color(T colors, 
                                const uint8_t t_r, const uint8_t t_g, const uint8_t t_b)
{
  const auto ds = distance_table(colors, t_r, t_g, t_b);
  const auto distance = std::min_element(std::begin(ds), std::end(ds)) - std::begin(ds);
  return colors[distance];
  
  /*
  return *std::min_element(std::begin(colors), std::end(colors), 
                          [&](const auto &lhs, const auto &rhs)
                          {
                            return distance(lhs, t_r, t_g, t_b) < distance(rhs, t_r, t_g, t_b);
                          });
                          */
}

void clear_bit(const uint16_t loc, const uint8_t bitnum)
{
  memory(loc) &= (0xFF ^ (1 << bitnum));
}

void set_bit(const uint16_t loc, const uint8_t bitnum)
{
  memory(loc) |= (1 << bitnum);
}

void write_multi_color_pixel(uint16_t)
{
}

template<typename D1, typename D2, typename D3, typename D4, typename ...  D >
  void write_multi_color_pixel(uint16_t loc, D1 d1, D2 d2, D3 d3, D4 d4, D ... d)
{
  memory(loc) = (d1 << 6) | (d2 << 4) | (d3 << 2) | d4;
  write_multi_color_pixel(loc + 1, d...);
}
  
constexpr bool test_bit(const uint8_t data, const uint8_t bit)
{
  return (data & (1 << bit)) != 0;
};

  
  
template<typename ... D>
  void make_sprite(uint8_t memory_loc, D ... d)
{
  write_multi_color_pixel((starting_bank + memory_loc) * 64, d...);
}
  
  
void enable_sprite(const uint8_t sprite_number, const uint8_t memory_loc,
                   const bool multicolor,
                   const bool double_width, const bool double_height)
{
  set_bit(SPRITE_ENABLE_BITS, sprite_number);
  memory(SPRITE_DATA_POINTERS + sprite_number) = starting_bank + memory_loc;
  if (double_width) {
    set_bit(SPRITE_EXPAND_HORIZONTAL, sprite_number);
  } else {
    clear_bit(SPRITE_EXPAND_HORIZONTAL, sprite_number);
  }
    
  if (double_height) {
    set_bit(SPRITE_EXPAND_VERTICAL, sprite_number);
  } else {
    clear_bit(SPRITE_EXPAND_VERTICAL, sprite_number);
  }
  
  if (multicolor) {
    set_bit(SPRITE_MULTICOLOR, sprite_number);
  } else {
    clear_bit(SPRITE_MULTICOLOR, sprite_number);
  }
  
}

void display_int(uint8_t x, uint8_t y, uint8_t val)
{
  memory(VIDEO_MEMORY + y * 40 + x) = val;
}
  
  
}



int main()
{
  constexpr std::array<Color, 16> colors {Color{Color::Name::Black, 0,0,0},
                                          Color{Color::Name::White, 255,255,255},
                                          Color{Color::Name::Red, 104, 55, 43},
                                          Color{Color::Name::Cyan, 112, 164, 178},
                                          Color{Color::Name::Purple, 111, 61, 134},
                                          Color{Color::Name::Green, 88, 141, 67},
                                          Color{Color::Name::Blue, 53, 40, 121},
                                          Color{Color::Name::Yellow, 184, 199, 111},
                                          Color{Color::Name::Orange, 111, 79, 37},
                                          Color{Color::Name::Brown, 67, 57, 0},
                                          Color{Color::Name::LightRed, 154, 103, 89},
                                          Color{Color::Name::DarkGrey, 68, 68, 68},
                                          Color{Color::Name::Grey, 108, 108, 108},
                                          Color{Color::Name::LightGreen, 154, 210, 132},
                                          Color{Color::Name::LightBlue, 108, 94, 181},
                                          Color{Color::Name::LightGrey, 149, 149, 149}};
  
  //constexpr auto nearest_black = nearest_color(colors, 0, 0, 0);
  //constexpr auto nearest_red = nearest_color(colors, 255, 255, 0);
  
  
  make_sprite(0,
              0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,2,2,2,2,0,0,0,0,
              0,0,0,2,2,2,2,2,2,0,0,0,
              0,0,2,2,2,1,1,2,2,2,0,0,
              0,0,2,2,1,1,1,1,2,2,0,0,
              0,0,2,2,1,1,1,1,2,2,0,0,
              0,0,2,2,1,1,1,1,2,2,0,0,
              0,0,2,2,1,1,1,1,2,2,0,0,
              0,0,2,2,1,1,1,1,2,2,0,0,
              0,0,2,2,1,1,1,1,2,2,0,0,
              0,0,2,2,1,1,1,1,2,2,0,0,
              0,0,2,2,2,1,1,2,2,2,0,0,
              0,0,0,2,2,2,2,2,2,0,0,0,
              0,0,0,2,2,2,2,2,2,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0,0,0,0,0,0,0
             );
  
  enable_sprite(0, 0, true, false, false);

  make_sprite(1,
              0,0,0,0,1,1,1,1,0,0,0,0,
              0,0,0,0,1,0,0,1,0,0,0,0,
              0,0,0,0,1,0,0,1,0,0,0,0,
              0,0,0,0,1,0,0,1,0,0,0,0,
              0,0,0,0,2,2,2,2,0,0,0,0,
              0,0,0,0,2,2,2,2,0,0,0,0,
              0,0,0,0,2,1,1,2,0,0,0,0,
              0,0,0,0,1,1,1,1,0,0,0,0,
              0,0,0,0,1,1,1,1,0,0,0,0,
              0,0,0,0,1,1,1,1,0,0,0,0,
              0,0,0,0,1,1,1,1,0,0,0,0,
              0,0,0,0,1,1,1,1,0,0,0,0,
              0,0,0,0,1,1,1,1,0,0,0,0,
              0,0,0,0,1,1,1,1,0,0,0,0,
              0,0,0,0,2,1,1,2,0,0,0,0,
              0,0,0,0,2,2,2,2,0,0,0,0,
              0,0,0,0,2,2,2,2,0,0,0,0,
              0,0,0,0,1,0,0,1,0,0,0,0,
              0,0,0,0,1,0,0,1,0,0,0,0,
              0,0,0,0,1,0,0,1,0,0,0,0,
              0,0,0,0,1,1,1,1,0,0,0,0
             );
  
  enable_sprite(1, 1, true, false, true);
  enable_sprite(2, 1, true, false, true);


  // sprite doubling with set_bit seems to be not working?
  // this is a hack TODO fix
//  memory(SPRITE_EXPAND_VERTICAL) = 0xFF;

  // start timer  
  memory(56590) = 1;


  const auto joy = [](const uint8_t d){
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
    return State(d);
  };

  const auto joy_port2 = [joy](){
    return joy(memory(56320));
  };
  
  const auto joy_port1 = [joy](){
    return joy(memory(56321));
  };
  
  
  const auto sprite_x = [](const uint8_t sprite_num) -> decltype(auto)
  {
    return (memory(SPRITE_POSITION_REGISTERS + sprite_num * 2));
  };

  const auto sprite_y = [](const uint8_t sprite_num) -> decltype(auto)
  {
    return (memory(SPRITE_POSITION_REGISTERS + sprite_num * 2 + 1));
  };
  
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
  uint8_t player1 = 0;
  uint8_t player2 = 0;

  const auto reset_ball = [&]{
    sprite_x(0) = 255/2;
    sprite_y(0) = 255/2;
  };

  reset_ball();

  sprite_x(1) = 20;
  sprite_y(1) = 50;

  sprite_x(2) = 255;
  sprite_y(2) = 150;

  while (true) {

    if (memory(56325) == 0) {
      // Move ball
      const auto ball_x = sprite_x(0) += std::get<0>(ball_vec);
      const auto ball_y = sprite_y(0) += std::get<1>(ball_vec);

      if (const auto collisions = sprite_collisions();
          collisions.sprite0 && (collisions.sprite1 || collisions.sprite2)) {
        // ball hit a paddle
        std::get<0>(ball_vec) *= -1;
        sprite_x(0) += std::get<0>(ball_vec);
      }

      // Update paddle positions
      if (const auto joy = joy_port1(); joy.up)
      {
        ++sprite_y(1);
      } else if (joy.down) {
        --sprite_y(1);
      }

      if (const auto joy = joy_port2(); joy.up)
      {
        ++sprite_y(2);
      } else if (joy.down) {
        --sprite_y(2);
      }



      // ball hit the top or bottom wall
      if (ball_y == 30 || ball_y == 240) {
        std::get<1>(ball_vec) *= -1;
      }

      if (ball_x == 1) {
        // ball hit left wall, player 2 scored
        ++player2;
        reset_ball();
      } else if (ball_x == 254) {
        // ball hit right wall, player 1 scored
        ++player1;
        reset_ball();
      }

      display_int(10, 3, player1);
      display_int(30, 3, player2);
    }
  }
}

