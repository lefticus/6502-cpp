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
constexpr uint16_t SPRITE_PRIORITY = VIDEO_REGISTERS + 27;
constexpr uint16_t VIDEO_MEMORY = 1024;
constexpr uint8_t  SPRITE_STARTING_BANK = 192;
constexpr uint16_t BORDER_COLOR = 53280;
constexpr uint16_t BACKGROUND_COLOR = 53281;
constexpr uint16_t SPRITE_0_COLOR = VIDEO_REGISTERS + 39;
constexpr uint16_t SPRITE_1_COLOR = SPRITE_0_COLOR + 1;
constexpr uint16_t SPRITE_2_COLOR = SPRITE_1_COLOR + 1;
constexpr uint16_t SPRITE_3_COLOR = SPRITE_2_COLOR + 1;
constexpr uint16_t SPRITE_4_COLOR = SPRITE_3_COLOR + 1;
constexpr uint16_t SPRITE_5_COLOR = SPRITE_4_COLOR + 1;
constexpr uint16_t SPRITE_6_COLOR = SPRITE_5_COLOR + 1;
constexpr uint16_t SPRITE_7_COLOR = SPRITE_6_COLOR + 1;



namespace {

struct Color
{
  uint8_t num;
  uint8_t r;
  uint8_t g;
  uint8_t b;
};
  
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
                   const bool multicolor, const bool low_priority,
                   const bool double_width, const bool double_height)
{
  memory(SPRITE_DATA_POINTERS + sprite_number) = SPRITE_STARTING_BANK + memory_loc;
  set_bit(SPRITE_ENABLE_BITS, sprite_number, true);
  set_bit(SPRITE_EXPAND_HORIZONTAL, sprite_number, double_width);
  set_bit(SPRITE_EXPAND_VERTICAL, sprite_number, double_height);
  set_bit(SPRITE_MULTICOLOR, sprite_number, multicolor);
  set_bit(SPRITE_PRIORITY, sprite_number, low_priority);
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
    
    auto direction_vector() const 
    {
      return std::make_pair(left?-1:(right?1:0), up?-1:(down?1:0));
    }
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

  template<typename T>
    auto square(T t) {
      return t * t;
  }
  
template<uint8_t r, uint8_t g, uint8_t b>
auto color_distance(const Color &lhs, const Color &rhs)
{
  return (square(lhs.r - r) + square(lhs.g - g) + square(lhs.b - b))
    < (square(rhs.r - r) + square(rhs.g - g) + square(rhs.b - b));
}


template<uint8_t r, uint8_t g, uint8_t b, typename Colors>
auto nearest_color(const Colors &colors)
{
  return *std::min_element(std::begin(colors), std::end(colors), color_distance<r,g,b>);
}

  
}

int main()
{
  // http://www.pepto.de/projects/colorvic/
  constexpr std::array<Color, 16> colors {{
    Color{0,  0x00, 0x00, 0x00},
    Color{1,  0xFF, 0xFF, 0xFF},
    Color{2,  0x88, 0x39, 0x32},
    Color{3,  0x67, 0xB6, 0xBD},
    Color{4,  0x8B, 0x3F, 0x96},
    Color{5,  0x55, 0xA0, 0x49},
    Color{6,  0x40, 0x31, 0x8D},
    Color{7,  0xBF, 0xCE, 0x72},
    Color{8,  0x8B, 0x54, 0x29},
    Color{9,  0x57, 0x42, 0x00},
    Color{10, 0xB8, 0x69, 0x62},
    Color{11, 0x50, 0x50, 0x50},
    Color{12, 0x78, 0x78, 0x78},
    Color{13, 0x94, 0xE0, 0x89},
    Color{14, 0x78, 0x69, 0xC4},
    Color{15, 0x9F, 0x9F, 0x9F}
  }};

  
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



  enable_sprite(0, 0, false, true, false, false);

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



  enable_sprite(1, 1, true, false, false, true);
  enable_sprite(2, 1, true, false, false, true);

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

  std::pair<int8_t, int8_t> ball_velocity{1,1};

  const auto reset_ball = [&]{
    sprite_x(0) = 255/2;
    sprite_y(0) = 255/2;
  };

  reset_ball();
  
  memory(BORDER_COLOR) = nearest_color<128,128,128>(colors).num;
  memory(BACKGROUND_COLOR) = nearest_color<0,0,0>(colors).num;
  memory(SPRITE_1_COLOR) = nearest_color<255,0,0>(colors).num;
  memory(SPRITE_2_COLOR) = nearest_color<0,255,0>(colors).num;
  

  struct Player
  {
    Player(const uint8_t num, const uint8_t startx, const uint8_t starty)
      : player_num(num),
        x(sprite_x(player_num)),
        y(sprite_y(player_num))
    {
      x = startx;
      y = starty;
    }
    
    void update_position()
    {
      y += joystick(player_num).direction_vector().second * 3;
    };

    void scored() {
      ++score;
    }

    const uint8_t player_num;
    volatile uint8_t &x;
    volatile uint8_t &y;
    uint8_t score = '0';
  };

  Player p1(1, 15,  255/2);
  Player p2(2, 255, 255/2);

  struct Frame
  {
    Frame(Player &p1, Player &p2) 
      : player1(p1), player2(p2)
    {
      while (!(memory(53266) == 250 && !test_bit(memory(53265),7))) {}
    }
    
    ~Frame() {
      display(10, 12, player1.score);
      display(20, 12, player2.score);
    }
    
    Player &player1;
    Player &player2;
  };
  
  
  while (true) {    
    Frame rt(p1, p2);
    
    if (auto [s0, s1, s2, s3, s4, s5, s6, s7] = sprite_collisions();
        s0 && (s1 || s2))
    {
      std::get<0>(ball_velocity) *= -1; //invert ball x velocity
      // "bounce" ball out of collision area
      sprite_x(0) += std::get<0>(ball_velocity);
    }

    // Update paddle positions
    p1.update_position();
    p2.update_position();

   

    // ball hit the top or bottom wall
    if (const auto ball_y = sprite_y(0) += std::get<1>(ball_velocity);
        ball_y == 45 || ball_y == 235) 
    {
      std::get<1>(ball_velocity) *= -1;
    }

    const auto score = [reset_ball](auto &player){
      player.scored();
      reset_ball();
    };
    
    if (const auto ball_x = sprite_x(0) += std::get<0>(ball_velocity); 
        ball_x == 1) {
      // ball hit left wall, player 2 scored
      score(p2);
    } else if (ball_x == 255) {
      // ball hit right wall, player 1 scored
      score(p1);
    }
  }  
}


