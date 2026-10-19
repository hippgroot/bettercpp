# Better C++

A header-only C++20 library of aliases for the standard library, plus
optional extensions for [GLM](https://github.com/g-truc/glm) and lightweight diagnostics
(logging, thread tracking, scope timers and allocation tracking), meant to make the C++ development experience a bit more ergonomic.

```cpp
#include <bettercpp/bettercpp.hpp>

struct mesh {
  list<f32>           vertices;
  map<str, u32>       lookup;
  opt<str>            name;
  fn<void(secs_f32)>  on_update;
};

instant start = clk::now();
// ...
secs_f64 elapsed = clk::now() - start;
```

## Requirements

- CMake 3.21+
- A C++20 compiler (C++23 for the diagnostics extension)

## Installation

bettercpp is an `INTERFACE` library, so there is nothing to build. Pull it into your project with
`FetchContent` or `add_subdirectory` and link the `bettercpp::bettercpp` target.

```cmake
include(FetchContent)
FetchContent_Declare(bettercpp
  GIT_REPOSITORY https://github.com/<user>/bettercpp.git
  GIT_TAG main
)
FetchContent_MakeAvailable(bettercpp)

target_link_libraries(my_app PRIVATE bettercpp::bettercpp)
```

Or, with a vendored copy:

```cmake
add_subdirectory(external/bettercpp)
target_link_libraries(my_app PRIVATE bettercpp::bettercpp)
```

### CMake options

Set these before the `FetchContent_MakeAvailable` / `add_subdirectory` call.

| Option                         | Default | Effect                                                                 |
| ------------------------------ | ------- | ---------------------------------------------------------------------- |
| `BETTERCPP_WITH_GLM`           | `OFF`   | Enables `bettercpp-glm.hpp`. Uses an existing `glm::glm` target or `find_package(glm)`, and falls back to fetching GLM 1.0.1. |
| `BETTERCPP_WITH_DIAG`          | `OFF`   | Enables `bettercpp-diag.hpp`. Raises the target to C++23 and links `Threads::Threads`. |
| `BETTERCPP_COPY_CLANG_FORMAT`  | `OFF`   | Copies bettercpp's `.clang-format` into your project root.             |
| `BETTERCPP_COPY_CLANG_TIDY`    | `OFF`   | Copies bettercpp's `.clang-tidy` into your project root.               |
| `BETTERCPP_COPY_CMAKE_PRESETS` | `OFF`   | Copies bettercpp's `CMakePresets.json` into your project root.         |
| `BETTERCPP_COPY_GITIGNORE`     | `OFF`   | Copies bettercpp's `.gitignore` into your project root.                |

The copy options only run when bettercpp is not the top-level project, and never overwrite an
existing file: if yours differs, bettercpp leaves it alone and prints a status message.

```cmake
set(BETTERCPP_WITH_DIAG ON)
set(BETTERCPP_COPY_CLANG_FORMAT ON)
FetchContent_MakeAvailable(bettercpp)
```

Enabled extensions are included automatically by `bettercpp.hpp`, so it's the only header you
need.

## Core aliases

Everything in `bettercpp.hpp` lives in the global namespace.

> **Note:** `bettercpp.hpp` also brings `std::chrono_literals`, `std::string_literals` and
> `std::string_view_literals` into the global namespace, and defines `fs` as an alias for
> `std::filesystem`.

### Numbers

| Alias                         | Type                              |
| ----------------------------- | --------------------------------- |
| `u8` `u16` `u32` `u64`        | `std::uint8_t` … `std::uint64_t`  |
| `i8` `i16` `i32` `i64`        | `std::int8_t` … `std::int64_t`    |
| `usize` / `isize`             | `std::size_t` / `std::ptrdiff_t`  |
| `f32` / `f64`                 | `float` / `double`                |
| `byte`                        | `std::byte`                       |

### Strings

| Alias      | Type               |
| ---------- | ------------------ |
| `str`      | `std::string`      |
| `str_view` | `std::string_view` |

### Containers and utilities

| Alias                | Type                       |
| -------------------- | -------------------------- |
| `array<T, N>`        | `std::array<T, N>`         |
| `span<T>`            | `std::span<T>`             |
| `list<T>`            | `std::vector<T>`           |
| `map<K, V>`          | `std::unordered_map<K, V>` |
| `ordered_map<K, V>`  | `std::map<K, V>`           |
| `set<K>`             | `std::unordered_set<K>`    |
| `ordered_set<K>`     | `std::set<K>`              |
| `pair<A, B>`         | `std::pair<A, B>`          |
| `tuple<Ts...>`       | `std::tuple<Ts...>`        |
| `opt<T>`             | `std::optional<T>`         |
| `fn<Sig>`            | `std::function<Sig>`       |

Note that `map` and `set` are the **hash-based** containers; use `ordered_map` / `ordered_set`
when you need sorted iteration. Every alias forwards the standard template parameters, so custom
hashers, comparators and allocators still work.

Smart pointers are re-exported as-is: `unique_ptr`, `shared_ptr`, `weak_ptr`, `make_unique`,
`make_shared`, along with `nullopt`.

### Threads

| Alias         | Type               |
| ------------- | ------------------ |
| `thread`      | `std::jthread`     |
| `stop_token`  | `std::stop_token`  |
| `mutex`       | `std::mutex`       |
| `scoped_lock` | `std::scoped_lock` |
| `unique_lock` | `std::unique_lock` |

`thread` joins automatically when destroyed, and passes a `stop_token` to its function if the
function takes one as its first parameter.

### Time

| Alias                              | Type                                       |
| ---------------------------------- | ------------------------------------------ |
| `clk` / `sys_clk`                  | `steady_clock` / `system_clock`            |
| `instant` / `sys_instant`          | their `time_point`s                        |
| `nanos` `micros` `millis`          | `nanoseconds` `microseconds` `milliseconds`|
| `secs` `mins` `hours`              | `seconds` `minutes` `hours`                |
| `secs_f32` / `secs_f64`            | floating-point seconds                     |
| `duration<Rep, Period>`            | `std::chrono::duration`                    |
| `time_point<Clock, Dur>`           | `std::chrono::time_point`                  |

`duration_cast` and `time_point_cast` are re-exported too.

## GLM extension

With `BETTERCPP_WITH_GLM`, the common GLM types are available unqualified:
`vec2`–`vec4`, `ivec2`–`ivec4`, `uvec2`–`uvec4`, `mat2`–`mat4` and `quat`.

## Diagnostics extension

With `BETTERCPP_WITH_DIAG` (C++23), the `diag` namespace provides logging and runtime
introspection.

### Logging

```cpp
inline diag::category net{ "net" };

int main() {
  diag::configure_from_env(); // reads BETTERCPP_LOG, e.g. "debug,net=trace,render=off"

  diag::info("Starting up");
  net.debug("Connecting to {}:{}", host, port);
}
```

- **Categories** are named channels with their own threshold. `app`, `threads` and `memory` are
  built in, and the free functions (`diag::info`, `diag::warn`, …) log to `app`.
- **Levels:** `trace`, `debug`, `info`, `warn`, `error`, `off`.
- **`configure(spec)`** applies rules like `"debug,net=trace"`. A bare level sets every category,
  and rules are remembered so categories registered later pick them up.
- Messages use `std::format` syntax and are checked at compile time.

Output goes to stderr (colored when it's a terminal and `NO_COLOR` is unset). Add your own sinks
for files, in-game consoles and so on:

```cpp
auto file = diag::add_sink(diag::file_sink("app.log"), diag::level::info);
diag::set_stderr_output(false); // optional
```

`add_sink` returns a `sink_handle` that unregisters the sink when destroyed; call `release()` to
keep it for the rest of the program.

### Threads

```cpp
thread worker([](stop_token st) {
  diag::thread_scope scope{ "worker" }; // names the thread, logs start/stop
  while (!st.stop_requested()) {
    // ...
  }
});

diag::log_threads(); // lists every live thread_scope with its uptime
```

Thread names show up in log lines and, on macOS and Linux, in debuggers and profilers.

### Timing

```cpp
{
  diag::scope_timer timer{ "load_assets" };
  // ...
} // logs "load_assets took 12.345ms"
```

### Allocation tracking

```cpp
inline diag::alloc_tracker graph_allocs{ "graph" };

list<node, diag::tracked_allocator<node, graph_allocs>> nodes;
node* raw = graph_allocs.create<node>(args...);
graph_allocs.destroy(raw);

int main() {
  diag::leak_check check; // reports any tracker with live allocations when main returns
  // ...
}
```

`tracked_allocator` works with any allocator-aware container and with `std::allocate_shared`,
which makes `shared_ptr` cycles show up as leaks. `log_allocations()` prints live, peak and total
counts for every tracker.

## Tooling

The repository ships an opinionated `.clang-format`, `.clang-tidy`, `CMakePresets.json`
(Ninja-based debug / release-with-debug-info / release presets for Windows, macOS and Linux)
and `.gitignore` (build output, CMake and tooling caches, editor and OS files).
Downstream projects can adopt them with the `BETTERCPP_COPY_*` options above.

## License

bettercpp is released under the [Boost Software License 1.0](LICENSE).
