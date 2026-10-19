#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem; // NOLINT(misc-unused-alias-decls)
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

using u8    = std::uint8_t;
using u16   = std::uint16_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;
using i8    = std::int8_t;
using i16   = std::int16_t;
using i32   = std::int32_t;
using i64   = std::int64_t;
using usize = std::size_t;
using isize = std::ptrdiff_t;

using f32 = float;
using f64 = double;

using str      = std::string;
using str_view = std::string_view;

using clk     = std::chrono::steady_clock;
using sys_clk = std::chrono::system_clock;

using instant     = clk::time_point;
using sys_instant = sys_clk::time_point;

using nanos  = std::chrono::nanoseconds;
using micros = std::chrono::microseconds;
using millis = std::chrono::milliseconds;
using secs   = std::chrono::seconds;
using mins   = std::chrono::minutes;
using std::chrono::hours;

using secs_f32 = std::chrono::duration<f32>;
using secs_f64 = std::chrono::duration<f64>;

using std::chrono::duration_cast;
using std::chrono::time_point_cast;

using std::byte;

using std::make_shared;
using std::make_unique;
using std::nullopt;
using std::shared_ptr;
using std::unique_ptr;
using std::weak_ptr;

using thread = std::jthread;
using std::stop_token;
using condvar     = std::condition_variable;
using condvar_any = std::condition_variable_any;

using std::mutex;
using std::scoped_lock;
using std::unique_lock;

template <typename T, usize Size>
using array = std::array<T, Size>;

template <typename T, usize Extent = std::dynamic_extent>
using span = std::span<T, Extent>;

template <typename T, typename Alloc = std::allocator<T>>
using list = std::vector<T, Alloc>;

template <typename T, typename Alloc = std::allocator<T>>
using queue = std::deque<T, Alloc>;

template <
  typename Key, typename Value, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>,
  typename Alloc = std::allocator<std::pair<const Key, Value>>>
using map = std::unordered_map<Key, Value, Hash, Equal, Alloc>;

template <
  typename Key, typename Value, typename Compare = std::less<Key>,
  typename Alloc = std::allocator<std::pair<const Key, Value>>>
using ordered_map = std::map<Key, Value, Compare, Alloc>;

template <
  typename Key, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>,
  typename Alloc = std::allocator<Key>>
using set = std::unordered_set<Key, Hash, Equal, Alloc>;

template <typename Key, typename Compare = std::less<Key>, typename Alloc = std::allocator<Key>>
using ordered_set = std::set<Key, Compare, Alloc>;

template <typename First, typename Second>
using pair = std::pair<First, Second>;

template <typename... Types>
using tuple = std::tuple<Types...>;

template <typename T>
using opt = std::optional<T>;
using std::nullopt;

template <typename Signature>
using fn = std::function<Signature>;

template <typename Count, typename Period = std::ratio<1>>
using duration = std::chrono::duration<Count, Period>;

template <typename Clock, typename Duration = Clock::duration>
using time_point = std::chrono::time_point<Clock, Duration>;

#ifdef BETTERCPP_WITH_GLM
#include "./bettercpp-glm.hpp" // NOLINT(misc-header-include-cycle)
#endif

#ifdef BETTERCPP_WITH_DIAG
#include "./bettercpp-diag.hpp" // NOLINT(misc-header-include-cycle)
#endif
