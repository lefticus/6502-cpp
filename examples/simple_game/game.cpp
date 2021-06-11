#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <string_view>
#include <variant>

#include "chargen.hpp"
#include "commodore64.hpp"

struct GameState;

struct Map_Action
{
  geometry::rect box;

  using Action_Func = void (*)(GameState &);
  Action_Func action = nullptr;

  constexpr void execute_if_collision(geometry::rect object,
    GameState &game) const
  {
    if (action == nullptr) { return; }

    if (box.intersects(object)) { action(game); }

  }
};


template<geometry::size Size> struct Map
{
  std::string_view name;
  petscii::Graphic<Size> layout;

  std::span<const Map_Action> actions;
};

struct GameState
{

  std::uint8_t endurance{ 10 };
  std::uint8_t stamina{ max_stamina() };
  std::uint16_t cash{ 100 };
  geometry::point location{ 20, 12 };

  bool redraw = true;

  c64::Clock game_clock{};

  Map<geometry::size{ 10, 5 }> const *current_map = nullptr;

  constexpr void set_current_map(const Map<geometry::size{ 10, 5 }> &new_map)
  {
    current_map = &new_map;
    redraw = true;
  }

  constexpr void execute_actions(geometry::point new_location, const auto &character) noexcept
  {
    if (new_location.x + character.size().width > 40) { new_location.x = location.x; }

    if (new_location.y + character.size().height > 20) { new_location.y = location.y; }

    location = new_location;

    if (current_map) {
      for (auto &action : current_map->actions) { action.execute_if_collision({location, character.size()}, *this); }
    }
  }

  [[nodiscard]] constexpr std::uint8_t max_stamina() const noexcept { return endurance * 5; }

  struct JoyStick1StateChanged
  {
    c64::Joystick state;
  };


  struct JoyStick2StateChanged
  {
    c64::Joystick state;
  };

  struct TimeElapsed
  {
    c64::Clock::milliseconds us;
  };

  using Event = std::variant<JoyStick2StateChanged, TimeElapsed>;

  std::uint8_t last_joystick_2_state = mos6502::peek(0xDC00);

  Event next_event() noexcept
  {
    const auto new_joystick_2_state = mos6502::peek(0xDC00);

    if (new_joystick_2_state != last_joystick_2_state) {
      last_joystick_2_state = new_joystick_2_state;
      return JoyStick2StateChanged{ c64::Joystick{ new_joystick_2_state } };
    }

    return TimeElapsed{ game_clock.restart() };
  }
};

static void puts(const geometry::point loc, const auto &range, const vicii::Colors color = vicii::Colors::white)
{
  const auto offset = static_cast<std::uint16_t>(loc.y * 40 + loc.x);

  const std::uint16_t start = 0x400 + offset;
  std::copy(begin(range), end(range), &mos6502::memory_loc(start));

  for (std::uint16_t color_loc = 0; color_loc < range.size(); ++color_loc) {
    mos6502::poke(static_cast<std::uint16_t>(0xD800 + color_loc + offset), static_cast<std::uint8_t>(color));
  }
}

static constexpr auto load_charset(const std::span<const std::uint8_t, 256 * 8> &bits)
{
  std::array<petscii::Graphic<geometry::size{ 8, 8 }>, 256> results{};

  for (std::size_t idx = 0; idx < 256; ++idx) {
    petscii::Graphic<geometry::size{ 8, 8 }> glyph{};

    for (std::uint8_t row = 0; row < 8; ++row) {
      const auto input_row = bits[idx * 8 + row];
      glyph[geometry::point{ 0, row }] = (0b1000'0000 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 0, row }] = (0b0100'0000 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 0, row }] = (0b0010'0000 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 0, row }] = (0b0001'0000 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 0, row }] = (0b0000'1000 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 0, row }] = (0b0000'0100 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 0, row }] = (0b0000'0010 & input_row) == 0 ? 0 : 1;
      glyph[geometry::point{ 0, row }] = (0b0000'0001 & input_row) == 0 ? 0 : 1;
    }

    results[idx] = glyph;
  }

  return results;
}

template<geometry::size Size> static constexpr auto from_pixels_to_petscii(const petscii::Graphic<Size> &pixels)
{
  petscii::Graphic<geometry::size{ Size.width / 8, Size.height / 8 }> result{};

  constexpr auto charset = load_charset(uppercase);

  for (uint8_t x = 0; x < pixels.width(); x += 8) {
    for (uint8_t y = 0; y < pixels.height(); y += 8) {
      std::uint8_t best_match = 32;
      std::size_t match_count = 0;

      std::uint8_t cur_char = 0;
      for (const auto &glyph : charset) {

        const auto count = pixels.match_count(glyph, x, y);
        if (count > match_count) {
          best_match = cur_char;
          match_count = count;
        }

        ++cur_char;
      }

      result(x / 8, y / 8) = best_match;
    }
  }

  return result;
}


template<geometry::size Size> static constexpr auto from_pixels_to_2x2(const petscii::Graphic<Size> &pixels)
{
  petscii::Graphic<geometry::size{ Size.width / 2, Size.height / 2 }> result{};

  using GlyphType = std::pair<petscii::Graphic<geometry::size{ 2, 2 }>, std::uint8_t>;
  constexpr std::array<GlyphType, 17> lookup_map{ GlyphType{ { 0, 0, 0, 0 }, 32 },
    GlyphType{ { 1, 0, 0, 0 }, 126 },
    GlyphType{ { 0, 1, 0, 0 }, 124 },
    GlyphType{ { 1, 1, 0, 0 }, 226 },
    GlyphType{ { 0, 0, 1, 0 }, 123 },
    GlyphType{ { 1, 0, 1, 0 }, 97 },
    GlyphType{ { 0, 1, 1, 0 }, 255 },
    GlyphType{ { 1, 1, 1, 0 }, 236 },
    GlyphType{ { 0, 0, 0, 1 }, 108 },
    GlyphType{ { 1, 0, 0, 1 }, 127 },
    GlyphType{ { 0, 1, 0, 1 }, 225 },
    GlyphType{ { 1, 1, 0, 1 }, 251 },
    GlyphType{ { 0, 0, 1, 1 }, 98 },
    GlyphType{ { 1, 0, 1, 1 }, 252 },
    GlyphType{ { 0, 1, 1, 1 }, 254 },
    GlyphType{ { 1, 1, 1, 1 }, 160 } };


  for (uint8_t x = 0; x < pixels.width(); x += 2) {
    for (uint8_t y = 0; y < pixels.height(); y += 2) {
      for (const auto &glyph : lookup_map) {
        if (pixels.match(glyph.first, x, y)) {
          result(x / 2, y / 2) = glyph.second;
          break;// go to next Y, we found our match
        }
      }
    }
  }

  return result;
}

template<geometry::size Size> struct SimpleSprite
{
  geometry::point location{};
  bool is_shown = false;
  petscii::Graphic<Size> graphic;
  petscii::Graphic<Size> saved_background{};

  constexpr SimpleSprite(std::initializer_list<uint8_t> data_) noexcept : graphic(data_) {}
};


// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts...
{
  using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;


struct Menu
{
  consteval Menu(std::span<const std::string_view> t_options)
    : options{ t_options }, box{ geometry::rect{ { 0, 0 },
                              { static_cast<std::uint8_t>(
                                  std::max_element(begin(options),
                                    end(options),
                                    [](std::string_view lhs, std::string_view rhs) { return lhs.size() < rhs.size(); })
                                    ->size()
                                  + 2),
                                static_cast<std::uint8_t>(options.size() + 2) } }
                                   .centered() }

  {}

  void highlight(std::uint8_t selection)
  {
    const auto cur_y = static_cast<std::uint8_t>(selection + 1 + box.top_left().y);
    for (std::uint8_t cur_x = 1; cur_x < box.width() - 1; ++cur_x) {
      vicii::invertc(geometry::point{ static_cast<std::uint8_t>(box.top_left().x + cur_x), cur_y });
    }
  }

  void unhighlight(std::uint8_t selection) { highlight(selection); }

  void hide(GameState &game)
  {
    displayed = false;
    game.redraw = true;
  }

  bool show([[maybe_unused]] GameState &game, std::uint8_t &selection)
  {
    if (!displayed) {
      displayed = true;

      draw_box(box, vicii::Colors::white);

      for (auto pos = box.top_left() + geometry::point{ 1, 1 }; const auto &str : options) {
        puts(pos, str, vicii::Colors::grey);
        pos = pos + geometry::point{ 0, 1 };
      }

      highlight(current_selection);
    }

    if (current_selection != next_selection) {
      unhighlight(current_selection);
      highlight(next_selection);
      current_selection = next_selection;
    }

    if (selected) {
      selected = false;
      selection = current_selection;
      return true;
    } else {
      return false;
    }
  }

  bool process_event(const GameState::Event &e)
  {
    if (not displayed) { return false; }

    if (const auto *ptr = std::get_if<GameState::JoyStick2StateChanged>(&e); ptr) {
      if (ptr->state.up()) { next_selection = current_selection - 1; }

      if (ptr->state.down()) { next_selection = current_selection + 1; }

      if (next_selection > options.size() - 1) { next_selection = static_cast<std::uint8_t>(options.size() - 1); }

      if (ptr->state.fire()) { selected = true; }

      return true;
    }

    return false;
  }

  std::span<const std::string_view> options;
  geometry::rect box;
  std::uint8_t current_selection{ 0 };
  std::uint8_t next_selection{ 0 };
  bool selected{ false };
  bool displayed{ false };
};


enum struct State { Walking, SystemMenu, AboutBox };

int main()
{
  // static constexpr auto charset = load_charset(uppercase);

  // clang-format off
  static constexpr auto inn = petscii::Graphic<geometry::size{6,5}> {
    32,233,160,160,223,32,
    233,160,160,160,160,223,
    160,137,142,142,160,160,
    160,160,160,160,79,160,
    160,160,160,160,76,160,
  };

  static constexpr auto gym = petscii::Graphic<geometry::size{6,5}> {
    32,233,160,160,223,32,
    233,160,160,160,160,223,
    160,135,153,141,160,160,
    160,160,160,160,79,160,
    160,160,160,160,76,160,
  };

  static constexpr auto trading_post = petscii::Graphic<geometry::size{6,5}> {
    32,233,160,160,223,32,
    233,160,160,160,160,223,
    148,146,129,132,133,160,
    160,160,160,160,79,160,
    160,160,160,160,76,160,
  };

/*
  static constexpr auto town = Graphic<4, 4>{ 
    85, 67, 67, 73,
    93, 233, 223, 93,
    93, 160, 160, 93,
    74, 67, 67, 75 };
*/

  static constexpr auto town = petscii::ColoredGraphic<geometry::size{4, 4}>{
    { 
      32, 32,32, 32,
      233,223,233,223,
      224,224,224,224,
      104,104,104,104
    },
    {
      2,2,10,10,
      4,4,7,7,
      4,4,7,7,
      11,11,11,11
    }
  };

/*
  static constexpr auto mountain = petscii::Graphic<geometry::size{4, 4}>{
    32, 78, 77, 32,
    32, 32, 78, 77,
    78, 77, 32, 32,
    32, 78, 77, 32 };
  */

  static constexpr auto colored_mountain = petscii::ColoredGraphic<geometry::size{4, 4}>{
    {
    32, 78, 77, 32,
    32, 32, 233, 223,
    233, 223, 32, 32,
    32, 78, 77, 32 },
    {
      1, 9, 9, 1,
      1, 1, 8, 8,
      9, 9, 1, 1,
      1, 8, 8, 1

    }
  };


  auto character = SimpleSprite<geometry::size{2,3}>{
    32, 87,
    78, 79,
    78, 77 };

  static constexpr auto city_map = Map<geometry::size{10,5}>{
    "wood town",
    {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 4, 0, 0, 0, 0, 6, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    std::span<const Map_Action>{}
  };


  static constexpr auto overview_actions = std::array {
    Map_Action { {{16,0},{4,4}}, [](GameState &g) { g.set_current_map(city_map); } }
  };

  static constexpr auto overview_map = Map<geometry::size{10, 5}>{
    "the world",
    {
      3, 1, 1, 0, 3, 0, 0, 0, 0, 0,
      0, 0, 1, 1, 0, 0, 0, 0, 3, 0,
      0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 3, 0, 0, 0, 0, 0, 0, 0,
      0, 1, 1, 1, 0, 0, 3, 0, 0, 0,
    },
    std::span<const Map_Action>(begin(overview_actions), end(overview_actions))
  };
  // clang-format on


  static constexpr std::array<void (*)(geometry::point), 7> tile_types{
    [](geometry::point) {},
    [](geometry::point p) { vicii::put_graphic(p, colored_mountain); },
    [](geometry::point) {},
    [](geometry::point p) { vicii::put_graphic(p, town); },
    [](geometry::point p) { vicii::put_graphic(p, inn); },
    [](geometry::point p) { vicii::put_graphic(p, gym); },
    [](geometry::point p) { vicii::put_graphic(p, trading_post); },
  };


  const auto draw_map = [](const auto &map) {
    for (std::size_t tile = 0; tile < tile_types.size(); ++tile) {
      for (const auto &pos : map.size()) {
        if (map[pos] == tile) { tile_types[tile]({static_cast<std::uint8_t>(pos.x * 4), static_cast<std::uint8_t>(pos.y * 4)}); }
      }
    }
  };

  GameState game;
  State state = State::Walking;
  game.current_map = &overview_map;

  constexpr auto show_stats = [](const auto &cur_game) {
    puts(geometry::point{ 1, 21 }, petscii::PETSCII("STAMINA:"), vicii::Colors::light_grey);
    put_hex(geometry::point{ 12, 21 }, cur_game.stamina, vicii::Colors::white);
    put_hex(geometry::point{ 15, 21 }, cur_game.max_stamina(), vicii::Colors::white);
    puts(geometry::point{ 14, 21 }, petscii::PETSCII("/"), vicii::Colors::light_grey);
    puts(geometry::point{ 1, 22 }, petscii::PETSCII("ENDURANCE:"), vicii::Colors::light_grey);
    put_hex(geometry::point{ 12, 22 }, cur_game.endurance, vicii::Colors::white);
    puts(geometry::point{ 1, 23 }, petscii::PETSCII("CASH:"), vicii::Colors::light_grey);
    put_hex(geometry::point{ 12, 23 }, cur_game.cash, vicii::Colors::white);
  };

  vicii::Screen screen;

  static constexpr auto menu_options = std::array{ std::string_view{ "info" },
    std::string_view{ "test2" },
    std::string_view{ "test3" },
    std::string_view{ "an even longer thing" } };

  Menu m(menu_options);


  auto eventHandler = overloaded{ [&](const GameState::JoyStick2StateChanged &e) {
                                   auto new_loc = game.location;

                                   if (e.state.fire()) {
                                     state = State::SystemMenu;
                                     return;
                                   }

                                   if (e.state.up()) { --new_loc.y; }
                                   if (e.state.down()) { ++new_loc.y; }
                                   if (e.state.left()) { --new_loc.x; }
                                   if (e.state.right()) { ++new_loc.x; }


                                   game.execute_actions(new_loc, character.graphic);

                                   screen.show(game.location, character);

                                   put_hex({36, 1}, e.state.state, vicii::Colors::dark_grey);
                                 },
    [](const GameState::TimeElapsed &e) { vicii::put_hex({36, 0}, e.us.count(), vicii::Colors::dark_grey); } };

  while (true) {
    const auto next_event = game.next_event();

    if (not m.process_event(next_event)) {
      // if no gui elements needed the event, then we handle it
      std::visit(eventHandler, next_event);
    }

    if (game.redraw) {
      screen.hide(character);
      vicii::cls();

      mos6502::poke(53280, 0);
      mos6502::poke(53281, 0);

      game.redraw = false;
      draw_map(game.current_map->layout);
      draw_box(geometry::rect{ { 0, 20 }, { 40, 5 } }, vicii::Colors::dark_grey);

      puts(geometry::point{ 10, 20 }, game.current_map->name, vicii::Colors::white);
      show_stats(game);
      screen.show(game.location, character);
    }

    if (std::uint8_t result = 0; state == State::SystemMenu && m.show(game, result)) {
      // we had a menu item selected
      m.hide(game);
      if (result == 0) {}


      vicii::increment_border_color();
    }

    /*
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
    */
  }
}