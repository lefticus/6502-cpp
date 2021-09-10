#include <algorithm>
#include <concepts>
#include <cstdint>
#include <utility>

template<std::integral Int> struct np_int
{
  Int val;

  // intentionally implicit conversions
  // from the underlying type allowed
  [[gnu::always_inline]] constexpr np_int(Int i) noexcept : val{ i } {}

  [[gnu::always_inline]] constexpr explicit operator Int() noexcept { return val; }

  [[gnu::always_inline]] constexpr auto operator<=>(const np_int &) const noexcept = default;

  template<typename Other>
  [[gnu::always_inline]] constexpr auto operator<=>(const np_int<Other> &other) const noexcept
    requires(std::is_signed_v<Int> == std::is_signed_v<Other>)
  {
    return val == other.val  ? std::strong_ordering::equal
           : val < other.val ? std::strong_ordering::less
                             : std::strong_ordering::greater;
  }
};

template<typename T> constexpr bool np_or_integral_v = std::is_integral_v<T>;

template<typename T> constexpr bool np_or_integral_v<np_int<T>> = true;

template<typename T> concept np_or_integral = np_or_integral_v<T>;

template<std::integral Int> [[nodiscard, gnu::always_inline, gnu::const]] constexpr auto val(Int i) noexcept
{
  return i;
}

template<typename T> [[nodiscard, gnu::always_inline, gnu::const]] constexpr auto val(np_int<T> i) noexcept
{
  return i.val;
}

template<typename LHS, typename RHS> consteval auto calculate_common_int()
{
  constexpr auto size = std::max(sizeof(LHS), sizeof(RHS));
  if constexpr (std::is_unsigned_v<LHS> && std::is_unsigned_v<RHS>) {
    if constexpr (size == 1) {
      return std::uint8_t{};
    } else if constexpr (size == 2) {
      return std::uint16_t{};
    } else if constexpr (size == 4) {
      return std::uint32_t{};
    } else if constexpr (size == 8) {
      return std::uint64_t{};
    }
  } else {
    if constexpr (size == 1) {
      return std::int8_t{};
    } else if constexpr (size == 2) {
      return std::int16_t{};
    } else if constexpr (size == 4) {
      return std::int32_t{};
    } else if constexpr (size == 8) {
      return std::int64_t{};
    }
  }
}

template<std::integral LHS, std::integral RHS> using common_int_t = decltype(calculate_common_int<LHS, RHS>());

template<std::integral Int>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto operator>>(np_int<Int> lhs,
  np_or_integral auto rhs) noexcept
{
  return np_int<Int>{ static_cast<Int>(lhs.val >> static_cast<Int>(rhs)) };
}

template<std::integral Int>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto operator<<(np_int<Int> lhs,
  np_or_integral auto rhs) noexcept
{
  return np_int<Int>{ static_cast<Int>(lhs.val << static_cast<Int>(rhs)) };
}

template<std::integral LHS, std::integral RHS>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto operator+(np_int<LHS> lhs, np_int<RHS> rhs) noexcept
{
  using Type = common_int_t<LHS, RHS>;
  return np_int{ static_cast<Type>(lhs.val + rhs.val) };
}

template<std::integral LHS, std::integral RHS>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto operator-(np_int<LHS> lhs, np_int<RHS> rhs) noexcept
{
  using Type = common_int_t<LHS, RHS>;
  return np_int{ static_cast<Type>(lhs.val - rhs.val) };
}

template<std::integral LHS, std::integral RHS>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto operator*(np_int<LHS> lhs, np_int<RHS> rhs) noexcept
{
  using Type = common_int_t<LHS, RHS>;
  return np_int{ static_cast<Type>(lhs.val * rhs.val) };
}

template<std::integral LHS, std::integral RHS>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto operator/(np_int<LHS> lhs, np_int<RHS> rhs) noexcept
{
  using Type = common_int_t<LHS, RHS>;
  return np_int{ static_cast<Type>(lhs.val / rhs.val) };
}

template<std::integral LHS>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto operator|(np_int<LHS> lhs, np_int<LHS> rhs) noexcept
{
  return np_int{ static_cast<LHS>(lhs.val | rhs.val) };
}

template<std::integral LHS>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto operator&(np_int<LHS> lhs, np_int<LHS> rhs) noexcept
{
  return np_int{ static_cast<LHS>(lhs.val & rhs.val) };
}

template<std::integral LHS, std::integral RHS>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto operator%(np_int<LHS> lhs, np_int<RHS> rhs) noexcept
{
  using Type = common_int_t<LHS, RHS>;
  return np_int{ static_cast<Type>(lhs.val % rhs.val) };
}

template<std::integral LHS, std::integral RHS>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto operator^(np_int<LHS> lhs, np_int<LHS> rhs) noexcept
{
  return np_int{ static_cast<LHS>(lhs.val ^ rhs.val) };
}

template<std::integral LHS>
[[gnu::always_inline]] constexpr auto &operator+=(np_int<LHS> &lhs, np_or_integral auto rhs) noexcept
{
  lhs.val += static_cast<LHS>(rhs);
  return lhs;
}

template<std::integral LHS>
[[gnu::always_inline]] constexpr auto &operator-=(np_int<LHS> &lhs, np_or_integral auto rhs) noexcept
{
  lhs.val -= static_cast<LHS>(rhs);
  return lhs;
}

template<std::integral LHS>
[[gnu::always_inline]] constexpr auto &operator/=(np_int<LHS> &lhs, np_or_integral auto rhs) noexcept
{
  lhs.val /= static_cast<LHS>(rhs);
  return lhs;
}

template<std::integral LHS>
[[gnu::always_inline]] constexpr auto &operator*=(np_int<LHS> &lhs, np_or_integral auto rhs) noexcept
{
  lhs.val /= static_cast<LHS>(rhs);
  return lhs;
}

template<std::integral LHS>
[[gnu::always_inline]] constexpr auto &operator%=(np_int<LHS> &lhs, np_or_integral auto rhs) noexcept
{
  lhs.val %= static_cast<LHS>(rhs);
  return lhs;
}

template<std::integral LHS>
[[gnu::always_inline]] constexpr auto &operator&=(np_int<LHS> &lhs, np_or_integral auto rhs) noexcept
{
  lhs.val &= static_cast<LHS>(rhs);
  return lhs;
}

template<std::integral LHS>
[[gnu::always_inline]] constexpr auto &operator|=(np_int<LHS> &lhs, np_or_integral auto rhs) noexcept
{
  lhs.val |= static_cast<LHS>(rhs);
  return lhs;
}

template<std::integral LHS>
[[gnu::always_inline]] constexpr auto &operator^=(np_int<LHS> &lhs, np_or_integral auto rhs) noexcept
{
  lhs.val ^= static_cast<LHS>(rhs);
  return lhs;
}

template<std::integral LHS>
[[gnu::always_inline]] constexpr auto &operator<<=(np_int<LHS> &lhs, np_or_integral auto rhs) noexcept
{
  lhs.val <<= static_cast<LHS>(rhs);
  return lhs;
}

template<std::integral LHS>
[[gnu::always_inline]] constexpr auto &operator>>=(np_int<LHS> &lhs, np_or_integral auto rhs) noexcept
{
  lhs.val >>= static_cast<LHS>(rhs);
  return lhs;
}

using uint_np8_t = np_int<std::uint8_t>;
using uint_np16_t = np_int<std::uint16_t>;
using uint_np32_t = np_int<std::uint32_t>;
using uint_np64_t = np_int<std::uint64_t>;

using int_np8_t = np_int<std::int8_t>;
using int_np16_t = np_int<std::int16_t>;
using int_np32_t = np_int<std::int32_t>;
using int_np64_t = np_int<std::int64_t>;

template<typename T>
constexpr bool np_meets_requirements =
  sizeof(T) == sizeof(np_int<T>)
  && std::is_trivially_destructible_v<np_int<T>> &&std::is_trivially_move_constructible_v<np_int<T>>
    &&std::is_trivially_copy_constructible_v<np_int<T>> &&std::is_trivially_copy_assignable_v<np_int<T>>
      &&std::is_trivially_move_assignable_v<np_int<T>>;

static_assert(np_meets_requirements<std::uint8_t>);
static_assert(np_meets_requirements<std::uint16_t>);
static_assert(np_meets_requirements<std::uint32_t>);
static_assert(np_meets_requirements<std::uint64_t>);

static_assert(np_meets_requirements<std::int8_t>);
static_assert(np_meets_requirements<std::int16_t>);
static_assert(np_meets_requirements<std::int32_t>);
static_assert(np_meets_requirements<std::int64_t>);

// ensures that First paired with any of Rest, in any order
// results in the same type as First again
template<std::integral First, std::integral... Rest>
constexpr bool is_same_combinations_v = (std::is_same_v<First, common_int_t<First, Rest>> && ...)
                                        && (std::is_same_v<First, common_int_t<Rest, First>> && ...);

static_assert(is_same_combinations_v<std::uint8_t, std::uint8_t>);
static_assert(is_same_combinations_v<std::uint16_t, std::uint8_t, std::uint16_t>);
static_assert(is_same_combinations_v<std::uint32_t, std::uint8_t, std::uint16_t, std::uint32_t>);
static_assert(is_same_combinations_v<std::uint64_t, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t>);

static_assert(is_same_combinations_v<std::int8_t, std::uint8_t>);
static_assert(is_same_combinations_v<std::int16_t, std::uint8_t, std::uint16_t>);
static_assert(is_same_combinations_v<std::int32_t, std::uint8_t, std::uint16_t, std::uint32_t>);
static_assert(is_same_combinations_v<std::int64_t, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t>);

static_assert(is_same_combinations_v<std::int8_t, std::int8_t>);
static_assert(is_same_combinations_v<std::int16_t, std::int8_t, std::int16_t>);
static_assert(is_same_combinations_v<std::int32_t, std::int8_t, std::int16_t, std::int32_t>);
static_assert(is_same_combinations_v<std::int64_t, std::int8_t, std::int16_t, std::int32_t, std::int64_t>);

auto left_shift(uint_np8_t value, int val) { return value << val; }

auto left_shift(std::uint8_t value, int val) { return static_cast<std::uint8_t>(value << val); }

std::uint8_t unsigned_value();
std::int8_t signed_value();


int main(int argc, const char **)
{
  uint_np8_t a{ 15 };
  auto value = a + uint_np16_t{ 10 };

  value += uint_np16_t{ 16 };

  [[maybe_unused]] const auto comparison = value < uint_np8_t{ 3 };
  // return static_cast<std::uint16_t>(value);

  int i = 1;
  [[maybe_unused]] int v = (i << argc);


  return std::cmp_less(unsigned_value(), signed_value());
}
