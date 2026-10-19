#pragma once

#include "bettercpp.hpp" // NOLINT(misc-header-include-cycle)

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <fstream>
#include <mutex>
#include <ostream>
#include <print>
#include <ranges>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <io.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace diag {

enum class level : u8 { trace, debug, info, warn, error, off };

constexpr str_view to_string(level lvl) {
  switch (lvl) {
    case level::trace: return "TRACE";
    case level::debug: return "DEBUG";
    case level::info:  return "INFO";
    case level::warn:  return "WARN";
    case level::error: return "ERROR";
    case level::off:   return "OFF";
  }
  return "?";
}

constexpr opt<level> parse_level(str_view text) {
  if (text == "trace")
    return level::trace;
  if (text == "debug")
    return level::debug;
  if (text == "info")
    return level::info;
  if (text == "warn")
    return level::warn;
  if (text == "error")
    return level::error;
  if (text == "off")
    return level::off;
  return nullopt;
}

// The views only live for the sink call; copy what you keep.
struct record {
  sys_instant wall_time;
  secs_f64    elapsed; // since program start
  level       lvl;
  str_view    category;
  str_view    thread;
  str_view    message;
};

using sink = fn<void(const record&)>;

class category;
class alloc_tracker;

namespace detail {

struct live_thread {
  str     name;
  instant start;
};

struct sink_entry {
  u64   id;
  level min;
  sink  callback;
};

struct state {
  mutex output_mutex; // guards sinks, next_sink_id and stderr_enabled
  mutex registry_mutex;

  list<sink_entry> sinks;
  u64              next_sink_id   = 1;
  bool             stderr_enabled = true;

  list<category*>                   categories;
  list<pair<str, level>>            rules;
  list<alloc_tracker*>              trackers;
  map<std::thread::id, live_thread> threads;

  instant          start = clk::now();
  std::atomic<u32> next_thread_index{ 1 };
};

// Leaked on purpose: logs and leak reports from static destructors must never touch a dead mutex.
inline state& global() {
  static auto* instance = new state;
  return *instance;
}

inline const std::thread::id main_thread_id = std::this_thread::get_id();
inline thread_local str      t_thread_name;
inline thread_local bool     t_in_sink = false;

inline bool color_enabled() {
#ifdef _WIN32
  static const bool enabled = _isatty(_fileno(stderr)) != 0 && std::getenv("NO_COLOR") == nullptr;
#else
  static const bool enabled = isatty(fileno(stderr)) != 0 && std::getenv("NO_COLOR") == nullptr;
#endif
  return enabled;
}

constexpr str_view color(level lvl) {
  switch (lvl) {
    case level::trace: return "\x1b[90m";
    case level::debug: return "\x1b[36m";
    case level::info:  return "\x1b[32m";
    case level::warn:  return "\x1b[33m";
    case level::error: return "\x1b[31m";
    case level::off:   return "";
  }
  return "";
}

constexpr str_view trim(str_view text) {
  const auto first = text.find_first_not_of(" \t");
  if (first == str_view::npos)
    return {};
  const auto last = text.find_last_not_of(" \t");
  return text.substr(first, last - first + 1);
}

void write(const category& cat, level lvl, str_view message);

}

inline str_view thread_name() {
  if (detail::t_thread_name.empty()) {
    detail::t_thread_name =
      std::this_thread::get_id() == detail::main_thread_id
        ? "main"s
        : std::format("thread-{}", detail::global().next_thread_index.fetch_add(1));
  }
  return detail::t_thread_name;
}

// The name must outlive the category.
class category {
  str_view           m_name;
  std::atomic<level> m_threshold;

public:
  explicit category(str_view name, level threshold = level::info)
  : m_name(name), m_threshold(threshold) {
    auto&       g = detail::global();
    scoped_lock lock(g.registry_mutex);
    for (const auto& [rule_name, rule_level] : g.rules)
      if (rule_name.empty() || rule_name == m_name)
        m_threshold.store(rule_level);
    g.categories.push_back(this);
  }

  ~category() {
    auto&       g = detail::global();
    scoped_lock lock(g.registry_mutex);
    std::erase(g.categories, this);
  }

  category(const category&)            = delete;
  category(category&&)                 = delete;
  category& operator=(const category&) = delete;
  category& operator=(category&&)      = delete;

  str_view name() const {
    return m_name;
  }
  level threshold() const {
    return m_threshold.load(std::memory_order_relaxed);
  }
  void set_threshold(level lvl) {
    m_threshold.store(lvl, std::memory_order_relaxed);
  }
  bool enabled(level lvl) const {
    return lvl >= threshold() && lvl != level::off;
  }

  template <typename... Args>
  void log(level lvl, std::format_string<Args...> fmt, Args&&... args) const {
    if (!enabled(lvl))
      return;
    detail::write(*this, lvl, std::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void trace(std::format_string<Args...> fmt, Args&&... args) const {
    log(level::trace, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args&&... args) const {
    log(level::debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void info(std::format_string<Args...> fmt, Args&&... args) const {
    log(level::info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void warn(std::format_string<Args...> fmt, Args&&... args) const {
    log(level::warn, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args&&... args) const {
    log(level::error, fmt, std::forward<Args>(args)...);
  }
};

inline str format_line(const record& rec, bool colored = false) {
  return std::format(
    "{:9.3f} {:<10} {:<6} {}{:<5}{} {}", rec.elapsed.count(), rec.thread, rec.category,
    colored ? detail::color(rec.lvl) : "", to_string(rec.lvl), colored ? "\x1b[0m" : "", rec.message
  );
}

inline void stderr_sink(const record& rec) {
  std::println(stderr, "{}", format_line(rec, detail::color_enabled()));
}

// Once reset() (or the destructor) returns, the sink is not running on any thread, so whatever it
// captured is safe to destroy.
class [[nodiscard]] sink_handle {
  u64 m_id = 0;

public:
  sink_handle() = default;
  explicit sink_handle(u64 id)
  : m_id(id) {}

  ~sink_handle() {
    reset();
  }

  sink_handle(sink_handle&& other) noexcept
  : m_id(std::exchange(other.m_id, 0)) {}

  sink_handle& operator=(sink_handle&& other) noexcept {
    if (this != &other) {
      reset();
      m_id = std::exchange(other.m_id, 0);
    }
    return *this;
  }

  sink_handle(const sink_handle&)            = delete;
  sink_handle& operator=(const sink_handle&) = delete;

  bool active() const {
    return m_id != 0;
  }

  void reset() {
    if (m_id == 0)
      return;
    auto&       g = detail::global();
    scoped_lock lock(g.output_mutex);
    std::erase_if(g.sinks, [id = m_id](const detail::sink_entry& entry) { return entry.id == id; });
    m_id = 0;
  }

  void release() {
    m_id = 0;
  }
};

// Sinks run on the logging thread, one at a time; state they share with other threads still needs
// its own lock. Adding or removing sinks from inside a sink deadlocks.
inline sink_handle add_sink(sink callback, level min = level::trace) {
  auto&       g = detail::global();
  scoped_lock lock(g.output_mutex);
  const u64   id = g.next_sink_id++;
  g.sinks.push_back({ id, min, std::move(callback) });
  return sink_handle(id);
}

inline void set_stderr_output(bool enabled) {
  auto&       g = detail::global();
  scoped_lock lock(g.output_mutex);
  g.stderr_enabled = enabled;
}

inline sink file_sink(const fs::path& path, bool append = false) {
  auto out = make_shared<std::ofstream>(path, append ? std::ios::app : std::ios::trunc);
  if (!*out)
    throw std::runtime_error(std::format("diag: can't open log file '{}'", path.string()));

  return [out](const record& rec) {
    std::println(*out, "{}", format_line(rec));
    out->flush();
  };
}

inline void detail::write(const category& cat, level lvl, str_view message) {
  auto&        g = global();
  const record rec{ sys_clk::now(), clk::now() - g.start, lvl, cat.name(), thread_name(), message };

  // Logging from inside a sink: this thread already holds output_mutex, so print directly.
  if (t_in_sink) {
    stderr_sink(rec);
    return;
  }

  scoped_lock lock(g.output_mutex);
  if (g.stderr_enabled)
    stderr_sink(rec);

  t_in_sink = true;
  for (const sink_entry& entry : g.sinks) {
    if (lvl < entry.min)
      continue;
    try {
      entry.callback(rec);
    } catch (const std::exception& e) {
      std::println(stderr, "diag: sink threw: {}", e.what());
    } catch (...) {
      std::println(stderr, "diag: sink threw an unknown exception");
    }
  }
  t_in_sink = false;
}

inline category app{ "app" };        // NOLINT(bugprone-throwing-static-initialization)
inline category threads{ "thread" }; // NOLINT(bugprone-throwing-static-initialization)
inline category memory{ "memory" };  // NOLINT(bugprone-throwing-static-initialization)

template <typename... Args>
void log(level lvl, std::format_string<Args...> fmt, Args&&... args) {
  app.log(lvl, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void trace(std::format_string<Args...> fmt, Args&&... args) {
  app.trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
  app.debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
  app.info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
  app.warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
  app.error(fmt, std::forward<Args>(args)...);
}

inline void configure(str_view spec) {
  auto&       g = detail::global();
  scoped_lock lock(g.registry_mutex);

  for (const auto part : std::views::split(spec, ',')) {
    const str_view item = detail::trim(str_view(part.begin(), part.end()));
    if (item.empty())
      continue;

    const auto     separator = item.find('=');
    const str_view name =
      separator == str_view::npos ? ""sv : detail::trim(item.substr(0, separator));
    const str_view value =
      separator == str_view::npos ? item : detail::trim(item.substr(separator + 1));

    const auto lvl = parse_level(value);
    if (!lvl) {
      app.warn("Unknown log level '{}'", value);
      continue;
    }

    bool matched = false;
    for (category* cat : g.categories) {
      if (!name.empty() && cat->name() != name)
        continue;
      cat->set_threshold(*lvl);
      matched = true;
    }
    if (!matched)
      app.warn("No log category named '{}' (yet); keeping the rule", name);

    g.rules.emplace_back(str(name), *lvl);
  }
}

inline void configure_from_env(const char* variable = "BETTERCPP_LOG") {
  if (const char* spec = std::getenv(variable); spec != nullptr && *spec != '\0')
    configure(spec);
}

inline void set_thread_name(str_view name) {
  detail::t_thread_name = name;
#ifdef __APPLE__
  pthread_setname_np(detail::t_thread_name.substr(0, 63).c_str());
#elifdef __linux__
  pthread_setname_np(pthread_self(), detail::t_thread_name.substr(0, 15).c_str());
#endif

  auto&       g = detail::global();
  scoped_lock lock(g.registry_mutex);
  if (auto itr = g.threads.find(std::this_thread::get_id()); itr != g.threads.end())
    itr->second.name = detail::t_thread_name;
}

class thread_scope {
public:
  explicit thread_scope(str_view name) {
    set_thread_name(name);
    {
      auto&       g = detail::global();
      scoped_lock lock(g.registry_mutex);
      g.threads.insert_or_assign(
        std::this_thread::get_id(), detail::live_thread{ str(name), clk::now() }
      );
    }
    threads.info("Thread started");
  }

  ~thread_scope() { // NOLINT(bugprone-exception-escape)
    instant start;
    {
      auto&       g = detail::global();
      scoped_lock lock(g.registry_mutex);
      auto        node = g.threads.extract(std::this_thread::get_id());
      start            = node.empty() ? clk::now() : node.mapped().start;
    }
    threads.info("Thread stopped after {:.2f}s", secs_f64(clk::now() - start).count());
  }

  thread_scope(const thread_scope&)            = delete;
  thread_scope(thread_scope&&)                 = delete;
  thread_scope& operator=(const thread_scope&) = delete;
  thread_scope& operator=(thread_scope&&)      = delete;
};

struct thread_info {
  str             name;
  std::thread::id id;
  secs_f64        uptime;
};

inline list<thread_info> live_threads() {
  auto&         g   = detail::global();
  const instant now = clk::now();
  scoped_lock   lock(g.registry_mutex);

  list<thread_info> result;
  result.reserve(g.threads.size());
  for (const auto& [id, thread] : g.threads)
    result.push_back({ thread.name, id, now - thread.start });
  std::ranges::sort(result, std::greater{}, &thread_info::uptime);
  return result;
}

inline void log_threads() {
  const list<thread_info> live = live_threads();
  threads.info("{} live thread(s)", live.size());
  for (const thread_info& thread : live)
    threads.info("  {:<16} up {:.2f}s", thread.name, thread.uptime.count());
}

class scope_timer {
  str             m_label;
  const category& m_category;
  level           m_level;
  instant         m_start = clk::now();

public:
  explicit scope_timer(str label, const category& cat = app, level lvl = level::info)
  : m_label(std::move(label)), m_category(cat), m_level(lvl) {}

  ~scope_timer() { // NOLINT(bugprone-exception-escape)
    m_category.log(
      m_level, "{} took {:.3f}ms", m_label, duration<f64, std::milli>(elapsed()).count()
    );
  }

  scope_timer(const scope_timer&)            = delete;
  scope_timer(scope_timer&&)                 = delete;
  scope_timer& operator=(const scope_timer&) = delete;
  scope_timer& operator=(scope_timer&&)      = delete;

  secs_f64 elapsed() const {
    return clk::now() - m_start;
  }
};

struct alloc_stats {
  usize live_count  = 0;
  usize live_bytes  = 0;
  usize peak_bytes  = 0;
  usize total_count = 0;
};

class alloc_tracker {
  str_view           m_name;
  std::atomic<usize> m_live_count{ 0 };
  std::atomic<usize> m_live_bytes{ 0 };
  std::atomic<usize> m_peak_bytes{ 0 };
  std::atomic<usize> m_total_count{ 0 };

public:
  explicit alloc_tracker(str_view name)
  : m_name(name) {
    auto&       g = detail::global();
    scoped_lock lock(g.registry_mutex);
    g.trackers.push_back(this);
  }

  ~alloc_tracker() {
    auto&       g = detail::global();
    scoped_lock lock(g.registry_mutex);
    std::erase(g.trackers, this);
  }

  alloc_tracker(const alloc_tracker&)            = delete;
  alloc_tracker(alloc_tracker&&)                 = delete;
  alloc_tracker& operator=(const alloc_tracker&) = delete;
  alloc_tracker& operator=(alloc_tracker&&)      = delete;

  str_view name() const {
    return m_name;
  }

  void on_alloc(usize bytes) {
    m_live_count.fetch_add(1, std::memory_order_relaxed);
    m_total_count.fetch_add(1, std::memory_order_relaxed);
    const usize live = m_live_bytes.fetch_add(bytes, std::memory_order_relaxed) + bytes;

    usize peak = m_peak_bytes.load(std::memory_order_relaxed);
    while (live > peak &&
           !m_peak_bytes.compare_exchange_weak(peak, live, std::memory_order_relaxed)) {}
  }

  void on_free(usize bytes) {
    m_live_count.fetch_sub(1, std::memory_order_relaxed);
    m_live_bytes.fetch_sub(bytes, std::memory_order_relaxed);
  }

  alloc_stats stats() const {
    return {
      m_live_count.load(std::memory_order_relaxed),
      m_live_bytes.load(std::memory_order_relaxed),
      m_peak_bytes.load(std::memory_order_relaxed),
      m_total_count.load(std::memory_order_relaxed),
    };
  }

  // Destroy with the same type you created, since the byte count comes from sizeof(T).
  template <typename T, typename... Args>
  T* create(Args&&... args) {
    T* ptr = new T(std::forward<Args>(args)...);
    on_alloc(sizeof(T));
    return ptr;
  }

  template <typename T>
  void destroy(T* ptr) {
    if (ptr == nullptr)
      return;
    delete ptr;
    on_free(sizeof(T));
  }
};

template <typename T, alloc_tracker& Tracker>
struct tracked_allocator {
  using value_type = T;

  // Needed explicitly: allocator_traits can only rebind templates whose parameters are all types.
  template <typename U>
  struct rebind {
    using other = tracked_allocator<U, Tracker>;
  };

  tracked_allocator() noexcept = default;

  // Implicit on purpose: containers convert between rebound allocators.
  template <typename U>
  // NOLINTNEXTLINE(cppcoreguidelines-explicit-constructor,misc-explicit-constructor)
  tracked_allocator(const tracked_allocator<U, Tracker>& /*other*/) noexcept {}

  T* allocate(usize count) {
    T* ptr = std::allocator<T>{}.allocate(count);
    Tracker.on_alloc(count * sizeof(T));
    return ptr;
  }

  void deallocate(T* ptr, usize count) noexcept {
    Tracker.on_free(count * sizeof(T));
    std::allocator<T>{}.deallocate(ptr, count);
  }

  template <typename U>
  bool operator==(const tracked_allocator<U, Tracker>& /*other*/) const noexcept {
    return true;
  }
};

inline void log_allocations() {
  auto&       g = detail::global();
  scoped_lock lock(g.registry_mutex);
  for (const alloc_tracker* tracker : g.trackers) {
    const alloc_stats s = tracker->stats();
    memory.info(
      "{}: {} live ({} bytes), peak {} bytes, {} total allocations", tracker->name(), s.live_count,
      s.live_bytes, s.peak_bytes, s.total_count
    );
  }
}

inline bool report_leaks() {
  auto&       g = detail::global();
  scoped_lock lock(g.registry_mutex);

  bool leaked = false;
  for (const alloc_tracker* tracker : g.trackers) {
    const alloc_stats s = tracker->stats();
    if (s.live_count == 0)
      continue;
    leaked = true;
    memory.error(
      "{}: {} allocation(s) leaked ({} bytes)", tracker->name(), s.live_count, s.live_bytes
    );
  }
  if (!leaked)
    memory.info("No leaks across {} tracker(s)", g.trackers.size());
  return leaked;
}

// Globals are still alive when this runs, so their allocations count as live.
class leak_check {
public:
  leak_check() = default;
  ~leak_check() { // NOLINT(bugprone-exception-escape)
    report_leaks();
  }

  leak_check(const leak_check&)            = delete;
  leak_check(leak_check&&)                 = delete;
  leak_check& operator=(const leak_check&) = delete;
  leak_check& operator=(leak_check&&)      = delete;
};

}
