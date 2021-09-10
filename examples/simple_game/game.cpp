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

using Square_Passable = bool (*)(const std::uint8_t type) noexcept;

struct Map_Action
{
  geometry::rect box;

  using Action_Func = void (*)(GameState &);
  Action_Func action = nullptr;

  constexpr void execute_if_collision(geometry::rect object, GameState &game) const
  {
    if (action == nullptr) { return; }

    if (box.intersects(object)) { action(game); }
  }
};


template<geometry::size Size, std::size_t Scale> struct Map
{
  std::string_view name;
  petscii::Graphic<Size> layout;
  Square_Passable passable = nullptr;
  std::uint8_t step_scale;
  std::span<const Map_Action> actions;

  [[nodiscard]] constexpr std::uint8_t location_value(geometry::point loc) const noexcept
  {
    const auto descaled_location =
      geometry::point{ static_cast<std::uint8_t>(loc.x / Scale), static_cast<std::uint8_t>(loc.y / Scale) };
    return layout[descaled_location];
  }

  [[nodiscard]] constexpr bool location_passable(geometry::point loc, geometry::size obj_size) const noexcept
  {
    if (passable != nullptr) {
      for (const auto &p : obj_size) {
        if (!passable(location_value(p + loc))) { return false; }
      }
      return true;
    }
    return false;
  }
};

struct GameState
{
  enum struct State { Walking, SystemMenu, AboutBox, Exit, AlmostDead };
  State state = State::Walking;

  std::uint8_t endurance{ 8 };
  std::uint8_t stamina{ max_stamina() };
  std::uint16_t cash{ 100 };
  std::uint8_t step_counter{ 0 };
  std::uint8_t stamina_counter{ 0 };
  geometry::point location{ 20, 12 };

  bool redraw = true;
  bool redraw_stats = true;

  c64::Clock game_clock{};

  Map<geometry::size{ 10, 5 }, 4> const *current_map = nullptr;
  Map<geometry::size{ 10, 5 }, 4> const *last_map = nullptr;

  constexpr void goto_last_map(geometry::point new_location)
  {
    std::swap(current_map, last_map);
    location = new_location;
    redraw = true;
  }

  constexpr void set_current_map(const Map<geometry::size{ 10, 5 }, 4> &new_map)
  {
    last_map = std::exchange(current_map, &new_map);
    redraw = true;
  }

  constexpr void execute_actions(geometry::point new_location, const auto &character) noexcept
  {
    if (new_location.x + character.size().width > 40) { new_location.x = location.x; }
    if (new_location.y + character.size().height > 20) { new_location.y = location.y; }


    if (current_map && current_map->location_passable(new_location, character.size())) {
      step_counter += current_map->step_scale;

      while (step_counter >= endurance) {
        redraw_stats = true;
        step_counter -= endurance;

        if (stamina == 1) {
          state = State::AlmostDead;
        } else {
          stamina -= 1;
        }

        stamina_counter += 1;

        if (stamina_counter == endurance * 3) {
          endurance += 1;
          stamina_counter = 0;
        }
      }

      location = new_location;

      for (const auto &action : current_map->actions) {
        action.execute_if_collision({ location, character.size() }, *this);
      }
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



template<geometry::size Size> static constexpr auto from_pixels_to_petscii(const petscii::Graphic<Size> &pixels)
{
  petscii::Graphic<geometry::size{ Size.width / 8, Size.height / 8 }> result{};

  constexpr auto charset = petscii::load_charset(petscii::uppercase);

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


struct TextBox
{
  consteval TextBox(std::span<const std::string_view> t_lines)
    : lines{ t_lines }, box{ geometry::rect{ { 0, 0 },
                          { static_cast<std::uint8_t>(
                              std::max_element(begin(lines),
                                end(lines),
                                [](std::string_view lhs, std::string_view rhs) { return lhs.size() < rhs.size(); })
                                ->size()
                              + 1),
                            static_cast<std::uint8_t>(lines.size() + 1) } }
                               .centered() }
  {}

  void hide(GameState &game)
  {
    displayed = false;
    game.redraw = true;
  }

  bool show([[maybe_unused]] GameState &game)
  {
    if (!displayed) {
      displayed = true;

      clear(box, vicii::Colors::grey);
      draw_box(box, vicii::Colors::white);

      for (auto pos = box.top_left() + geometry::point{ 1, 1 }; const auto &str : lines) {
        puts(pos, str, vicii::Colors::grey);
        pos = pos + geometry::point{ 0, 1 };
      }
    }

    if (selected) {
      selected = false;
      return true;
    } else {
      return false;
    }
  }

  bool process_event(const GameState::Event &e)
  {
    if (not displayed) { return false; }

    if (const auto *ptr = std::get_if<GameState::JoyStick2StateChanged>(&e); ptr) {
      if (ptr->state.fire()) { selected = true; }
      return true;
    }

    return false;
  }

  std::span<const std::string_view> lines;
  geometry::rect box;
  bool selected{ false };
  bool displayed{ false };
};

struct Menu
{
  consteval Menu(std::span<const std::string_view> t_options)
    : options{ t_options }, box{ geometry::rect{ { 0, 0 },
                              { static_cast<std::uint8_t>(
                                  std::max_element(begin(options),
                                    end(options),
                                    [](std::string_view lhs, std::string_view rhs) { return lhs.size() < rhs.size(); })
                                    ->size()
                                  + 1),
                                static_cast<std::uint8_t>(options.size() + 1) } }
                                   .centered() }

  {}

  void highlight(std::uint8_t selection)
  {
    const auto cur_y = static_cast<std::uint8_t>(selection + 1 + box.top_left().y);
    for (std::uint8_t cur_x = 1; cur_x < box.width(); ++cur_x) {
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

      clear(box, vicii::Colors::grey);
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
      // wrap around up and down during selection
      if (ptr->state.up()) {
        if (current_selection == 0) {
          next_selection = static_cast<std::uint8_t>(options.size() - 1);
        } else {
          next_selection = current_selection - 1;
        }
      }

      if (ptr->state.down()) {
        if (current_selection == options.size() - 1) {
          next_selection = 0;
        } else {
          next_selection = current_selection + 1;
        }
      }

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

  static constexpr auto mountain = petscii::ColoredGraphic<geometry::size{4, 4}>{
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

  static constexpr auto ore_town_actions = std::array {
    Map_Action { {{0,19},{40,1}}, [](GameState &g) { g.goto_last_map({0, 4}); } },
  };

  static constexpr auto ore_town = Map<geometry::size{10,5}, 4>{
    "ore town",
    {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      0, 4, 0, 0, 0, 0, 6, 0, 0, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      0, 0, 5, 0, 0, 0, 0, 0, 0, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    },
    [](const std::uint8_t type) noexcept { return type != 1; },
    1,
    ore_town_actions
  };

  static constexpr auto wool_town_actions = std::array {
    Map_Action { {{0,0},{1,20}}, [](GameState &g) { g.goto_last_map({6, 12}); } },
    Map_Action { {{39,0},{1,20}}, [](GameState &g) { g.goto_last_map({12, 12}); } },
  };

  static constexpr auto wool_town = Map<geometry::size{10,5}, 4>{
    "wool town",
    {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      0, 4, 0, 0, 0, 0, 6, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    },
    [](const std::uint8_t type) noexcept { return type != 1; },
    1,
    wool_town_actions
  };

  static constexpr auto wheat_town_actions = std::array {
    Map_Action { {{0,0},{1,20}}, [](GameState &g) { g.goto_last_map({22, 16}); } },
    Map_Action { {{39,0},{1,20}}, [](GameState &g) { g.goto_last_map({28, 16}); } },
    Map_Action { {{0,0},{40,1}}, [](GameState &g) { g.goto_last_map({22, 13}); } },
  };


  static constexpr auto wheat_town = Map<geometry::size{10,5}, 4>{
    "wheat town",
    {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 4, 0, 0, 0, 0, 6, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    [](const std::uint8_t type) noexcept { return type != 1; },
    1,
    wheat_town_actions
  };

  static constexpr auto brick_town_actions = std::array {
    Map_Action { {{0,0},{1,20}}, [](GameState &g) { g.goto_last_map({30, 4}); } },
    Map_Action { {{39,0},{1,20}}, [](GameState &g) { g.goto_last_map({36, 4}); } },
    Map_Action { {{0,0},{40,1}}, [](GameState &g) { g.goto_last_map({32, 0}); } },
    Map_Action { {{0,19},{40,1}}, [](GameState &g) { g.goto_last_map({32, 8}); } },
  };

  static constexpr auto brick_town = Map<geometry::size{10,5}, 4>{
    "brick town",
    {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 4, 0, 0, 0, 0, 6, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    [](const std::uint8_t type) noexcept { return type != 1; },
    1,
    brick_town_actions
  };

  static constexpr auto wood_town_actions = std::array {
    Map_Action { {{0,0},{1,20}}, [](GameState &g) { g.goto_last_map({14, 0}); } },
    Map_Action { {{39,0},{1,20}}, [](GameState &g) { g.goto_last_map({20, 0}); } },
    Map_Action { {{0,19},{40,1}}, [](GameState &g) { g.goto_last_map({16, 4}); } },
  };

  static constexpr auto wood_town = Map<geometry::size{10,5}, 4>{
    "wood town",
    {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 4, 0, 0, 0, 0, 6, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    [](const std::uint8_t type) noexcept { return type != 1; },
    1,
    wood_town_actions
  };


  static constexpr auto overview_actions = std::array {
    Map_Action { {{0,0},{4,4}}, [](GameState &g) { g.set_current_map(ore_town); } },
    Map_Action { {{8,12},{4,4}}, [](GameState &g) { g.set_current_map(wool_town); } },
    Map_Action { {{24,16},{4,4}}, [](GameState &g) { g.set_current_map(wheat_town); } },
    Map_Action { {{32,4},{4,4}}, [](GameState &g) { g.set_current_map(brick_town); } },
    Map_Action { {{16,0},{4,4}}, [](GameState &g) { g.set_current_map(wood_town); } }
};

  static constexpr auto overview_map = Map<geometry::size{10, 5}, 4>{
    "the world",
    {
      3, 1, 1, 0, 3, 0, 0, 0, 0, 0,
      0, 0, 1, 1, 0, 0, 0, 0, 3, 0,
      0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 3, 0, 0, 0, 0, 0, 0, 0,
      0, 1, 1, 1, 0, 0, 3, 0, 0, 0,
    },
    [](const std::uint8_t type) noexcept { return type != 1; },
    10,
    overview_actions
  };
  // clang-format on


  static constexpr std::array<void (*)(geometry::point), 7> tile_types{
    [](geometry::point) {},
    [](geometry::point p) { vicii::put_graphic(p, mountain); },
    [](geometry::point) {},
    [](geometry::point p) { vicii::put_graphic(p, town); },
    [](geometry::point p) { vicii::put_graphic(p, inn); },
    [](geometry::point p) { vicii::put_graphic(p, gym); },
    [](geometry::point p) { vicii::put_graphic(p, trading_post); },
  };


  const auto draw_map = [](const auto &map) {
    for (std::size_t tile = 0; tile < tile_types.size(); ++tile) {
      for (const auto &pos : map.size()) {
        if (map[pos] == tile) {
          tile_types[tile]({ static_cast<std::uint8_t>(pos.x * 4), static_cast<std::uint8_t>(pos.y * 4) });
        }
      }
    }
  };

  GameState game;
  game.state = GameState::State::Walking;
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

  static constexpr auto menu_options = std::array{ std::string_view{ "about game" }, std::string_view{ "exit menu" } };

  Menu m(menu_options);

  static constexpr auto url = petscii::PETSCII("HTTPS://GITHUB.COM/LEFTICUS/6502-CPP");

  static constexpr auto about_text = std::array{ std::string_view{ "created in c++20 by jason turner" },
    std::string_view{ "using an automated conversion of" },
    std::string_view{ "gcc generated avr code to 6502" },
    std::string_view{ "assembly." },
    std::string_view{ url.data(), url.size() } };

  TextBox about_box(about_text);

  static constexpr auto almost_dead_text = std::array{ std::string_view{ "you became so exhausted that you" },
    std::string_view{ "passed out and passers by stole" },
    std::string_view{ "some of your cash and items." },
    std::string_view{ "" },
    std::string_view{ "a kind soul has dropped you off at a" },
    std::string_view{ "nearby inn." } };

  TextBox almost_dead(almost_dead_text);

  auto eventHandler = overloaded{ [&](const GameState::JoyStick2StateChanged &e) {
                                   auto new_loc = game.location;
                                   put_hex({ 36, 1 }, e.state.state, vicii::Colors::dark_grey);

                                   if (e.state.fire()) {
                                     game.state = GameState::State::SystemMenu;
                                     return;
                                   }

                                   if (e.state.up()) { --new_loc.y; }
                                   if (e.state.down()) { ++new_loc.y; }
                                   if (e.state.left()) { --new_loc.x; }
                                   if (e.state.right()) { ++new_loc.x; }

                                   if (new_loc != game.location) {
                                     game.execute_actions(new_loc, character.graphic);
                                     screen.show(game.location, character);
                                   }
                                 },
    [](const GameState::TimeElapsed &e) {
      vicii::put_hex({ 36, 0 }, e.us.count(), vicii::Colors::dark_grey);
    } };

  while (game.state != GameState::State::Exit) {
    const auto next_event = game.next_event();

    if (not m.process_event(next_event) && not about_box.process_event(next_event)
        && not almost_dead.process_event(next_event)) {
      // if no gui elements needed the event, then we handle it
      std::visit(eventHandler, next_event);
    }

    if (game.redraw) {
      screen.hide(character);
      vicii::cls();

      mos6502::poke(53280, 0);
      mos6502::poke(53281, 0);

      game.redraw = false;
      game.redraw_stats = true;
      draw_map(game.current_map->layout);
      draw_box(geometry::rect{ { 0, 20 }, { 39, 4 } }, vicii::Colors::dark_grey);

      puts(geometry::point{ 10, 20 }, game.current_map->name, vicii::Colors::white);

      screen.show(game.location, character);
    }

    if (game.redraw_stats) {
      show_stats(game);
      game.redraw_stats = false;
    }

    if (std::uint8_t result = 0; game.state == GameState::State::SystemMenu && m.show(game, result)) {
      // we had a menu item selected
      m.hide(game);
      if (result == 0) {
        game.state = GameState::State::AboutBox;
      } else {
        game.state = GameState::State::Walking;
      }
    } else if (game.state == GameState::State::AboutBox && about_box.show(game)) {
      about_box.hide(game);
      game.state = GameState::State::Walking;
    } else if (game.state == GameState::State::AlmostDead && almost_dead.show(game)) {
      almost_dead.hide(game);
      game.set_current_map(wheat_town);
      game.cash /= 2;
      game.stamina = game.max_stamina();
      game.state = GameState::State::Walking;
    }
  }
}
