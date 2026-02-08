# Technology Stack

**Project:** cels-clay
**Researched:** 2026-02-07
**Overall confidence:** HIGH

## Executive Summary

The cels-clay module integrates Clay v0.14 (a single-header C99 layout library) into the CELS declarative ECS framework. The stack is deliberately minimal: Clay itself, ncurses (already available via cels-ncurses), and CMake FetchContent. The primary engineering challenges are not about technology selection but about *integration architecture* -- bridging Clay's immediate-mode `CLAY()` macro with CELS's composition-based macro system, and mapping Clay's float-pixel coordinate space to ncurses's integer-cell coordinate space.

---

## Recommended Stack

### Core Layout Library

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| Clay | v0.14 | Flexbox-style UI layout engine | Only serious single-header C layout library. Already used locally at `/home/cachy/workspaces/libs/clay`. Zero dependencies, arena-based memory, renderer-agnostic render command output. C99 compatible. |

**Confidence: HIGH** -- verified from local checkout. Clay v0.14 is the latest tagged release. The local repo is at `v0.14-52-g76ec363` (52 commits past v0.14 tag). Recommendation: pin to `v0.14` tag for stability unless a specific post-v0.14 feature is needed.

**Key Clay API surface used by cels-clay:**

```c
// Initialization
uint32_t Clay_MinMemorySize(void);
Clay_Arena Clay_CreateArenaWithCapacityAndMemory(size_t capacity, void *memory);
Clay_Context* Clay_Initialize(Clay_Arena arena, Clay_Dimensions layoutDimensions, Clay_ErrorHandler errorHandler);

// Per-frame layout
void Clay_SetLayoutDimensions(Clay_Dimensions dimensions);
void Clay_BeginLayout(void);
Clay_RenderCommandArray Clay_EndLayout(void);

// Text measurement callback (REQUIRED)
void Clay_SetMeasureTextFunction(/* callback */);

// Element declaration macro
CLAY(id, .layout = {...}, .backgroundColor = {...}) {
    CLAY_TEXT(text, config);
    // children...
}
```

### Terminal Rendering

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| ncurses (wide) | System | Terminal I/O, positioned text, color pairs, attributes | Already a dependency via cels-ncurses. No reason to add termbox2 or raw ANSI -- we reuse the existing window/color infrastructure. |

**Confidence: HIGH** -- cels-ncurses already handles ncurses initialization, color pair setup, frame loop, and `wnoutrefresh()`/`doupdate()` pattern. The Clay ncurses renderer lives *in the app layer* (wired via Feature/Provider), not in cels-clay itself.

### Build System

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| CMake | >= 3.21 | Build system | Matches CELS root CMakeLists.txt minimum version. FetchContent available since 3.11, stable since 3.14. |
| FetchContent | (CMake built-in) | Fetch Clay at configure time | Same pattern used for flecs and yyjson in CELS root. Single-header libraries work well with FetchContent. |

**Confidence: HIGH** -- verified from CELS root CMakeLists.txt.

---

## Clay Integration via CMake FetchContent

### Recommended CMakeLists.txt Pattern

Clay is a single-header library. It does NOT ship a CMake target -- its own `CMakeLists.txt` only builds examples. For FetchContent integration, we create an INTERFACE target ourselves.

```cmake
cmake_minimum_required(VERSION 3.21)

# ============================================================================
# Fetch Clay (single-header layout library)
# ============================================================================
include(FetchContent)

FetchContent_Declare(
    clay
    GIT_REPOSITORY https://github.com/nicbarker/clay.git
    GIT_TAG        v0.14
)

# Only fetch, do NOT call add_subdirectory (Clay's CMakeLists builds examples)
FetchContent_GetProperties(clay)
if(NOT clay_POPULATED)
    FetchContent_Populate(clay)
endif()

# ============================================================================
# INTERFACE library target
# ============================================================================
add_library(cels-clay INTERFACE)

target_sources(cels-clay INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_module.c
)

target_include_directories(cels-clay INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${clay_SOURCE_DIR}     # Provides clay.h
)

target_link_libraries(cels-clay INTERFACE
    cels
)
```

**Key decisions in this pattern:**

1. **`FetchContent_Populate` instead of `FetchContent_MakeAvailable`**: Clay's own CMakeLists.txt tries to build all its examples (raylib, SDL, etc.) which would fail or pull in unwanted dependencies. We only need `clay.h` from the source directory.

2. **INTERFACE library**: Same pattern as cels-ncurses. Sources compile in the consumer's context. This is required because `#define CLAY_IMPLEMENTATION` must appear in exactly one translation unit, and that unit lives in the consumer.

3. **No `target_link_libraries` to clay**: Clay is header-only. The include path is sufficient. Clay needs no linking (no `.a` or `.so`).

4. **No link to cels-ncurses**: The module architecture mandates that cels-clay and cels-ncurses are siblings with no cross-dependency. The app wires them together.

**Confidence: HIGH** -- verified Clay's CMakeLists.txt only builds examples and explicitly comments out the INTERFACE library target (lines 61-62 of Clay's CMakeLists.txt). FetchContent_Populate is the correct approach.

---

## Terminal Coordinate Mapping

### The Problem

Clay works in float pixels. Terminal works in integer character cells. A terminal cell is roughly 9x21 pixels (width x height) but this varies by font, terminal emulator, and platform.

### Recommended Approach: Cell-Width Multiplier

**Use Clay's coordinate system with a configurable `cell_pixel_size` that maps float pixels to terminal cells.** This is exactly the approach used by both of Clay's terminal renderers.

```
terminal_col = round(clay_x / cell_pixel_width)
terminal_row = round(clay_y / cell_pixel_height)
```

**Recommended cell size for terminal layout:** `1.0 x 1.0` (one Clay unit = one terminal cell).

This is a departure from Clay's terminal examples (which use 9x21 or 16x16 to simulate "real" pixel density). For a terminal-native UI, there is no benefit in pretending cells are pixel-sized. Instead:

```c
// Initialize Clay with terminal dimensions directly in cells
Clay_Initialize(arena,
    (Clay_Dimensions){ .width = (float)COLS, .height = (float)LINES },
    errorHandler);

// Text measurement: 1 char = 1 unit width, 1 line = 1 unit height
Clay_Dimensions measure_text(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    int width = 0;
    for (int i = 0; i < text.length; i++) {
        if (text.chars[i] != '\n') width++;
    }
    return (Clay_Dimensions){ .width = (float)width, .height = 1.0f };
}
```

**The render command translation then becomes trivial:**

```c
// No division needed -- Clay coordinates ARE cell coordinates
int col = (int)(renderCommand->boundingBox.x + 0.5f);
int row = (int)(renderCommand->boundingBox.y + 0.5f);
int w   = (int)(renderCommand->boundingBox.width + 0.5f);
int h   = (int)(renderCommand->boundingBox.height + 0.5f);
```

**Why this works:** Clay's layout engine is unit-agnostic. It does not care whether units represent pixels, cells, or anything else. The only requirement is that `Clay_SetMeasureTextFunction` returns dimensions in the same unit system. By using 1:1 cell units, we eliminate the float-to-int rounding error that plagues the pixel-based terminal renderers.

**Why NOT use the pixel-ratio approach:**

1. Clay's terminal example uses `columnWidth = 16` and multiplies everything -- this introduces rounding artifacts (text at x=0.5 cells rounds differently than text at x=1.5 cells)
2. The termbox2 renderer's `cell_snap_bounding_box` compensates for this by computing width as `round(x+w) - round(x)` -- a workaround for a self-imposed problem
3. A terminal UI never needs sub-cell precision. Fractional cell positions create visual jitter, not smooth rendering.

**Confidence: HIGH** -- verified from Clay's source that the layout engine is unit-agnostic (uses floats throughout, never assumes pixel semantics). Both terminal renderers (`clay_renderer_terminal_ansi.c` and `clay_renderer_termbox2.c`) confirm the division-based mapping works but is unnecessarily complex for terminal-native UI.

---

## C99 Macro Composition Techniques

### The Problem

CELS has `CLAY()` is taken by Clay. Both use the same C99 `for` loop trick for block-scoped macros. The CEL_Clay() macro must:

1. Open a CELS entity scope (via cels_begin_entity/cels_end_entity)
2. Store a component marking this entity as Clay-backed
3. Delegate to CLAY() for the layout subtree

### Recommended Approach: Wrapper Macro with Sequential For-Loop Nesting

```c
// CEL_Clay() wraps a CELS entity that contains a Clay subtree
#define CEL_Clay(clay_id, ...) \
    _CEL_CLAY_IMPL(_CEL_CAT(_cel_clay_, __COUNTER__), clay_id, __VA_ARGS__)

#define _CEL_CLAY_IMPL(uid, clay_id, ...) \
    for (int uid = (cels_begin_entity("Clay"), \
                    cels_component_add(cels_get_context(), ClayBlock_ensure(), \
                        &(ClayBlock){0}, sizeof(ClayBlock)), 0); \
         uid < 1; \
         uid++, cels_end_entity()) \
    CLAY(clay_id, __VA_ARGS__)
```

**Key technique: sequential for-loop nesting.** The CELS `for` loop runs first (opens entity), then the CLAY `for` loop runs inside it (declares layout). When CLAY's `for` loop increment fires (`Clay__CloseElement()`), and then the CELS `for` loop increment fires (`cels_end_entity()`), cleanup happens in the correct order: Clay element closes, then ECS entity closes.

**Compatibility notes:**

- Both CELS and Clay use `for (init; condition; increment)` block macros
- Both use `__COUNTER__` for unique variable names (no shadowing conflicts)
- Clay's `CLAY__ELEMENT_DEFINITION_LATCH` is a static uint8_t, not a loop variable, so it doesn't conflict with CELS's `uid` variables
- Clay requires `CLAY_IMPLEMENTATION` defined before including `clay.h` in exactly one `.c` file. In the INTERFACE library pattern, this is the consumer's responsibility -- document in the header.

**The `CLAY()` macro expanded (from clay.h v0.14):**

```c
#define CLAY(id, ...) \
    for ( \
        CLAY__ELEMENT_DEFINITION_LATCH = (Clay__OpenElementWithId(id), \
            Clay__ConfigureOpenElement(CLAY__CONFIG_WRAPPER(Clay_ElementDeclaration, __VA_ARGS__)), 0); \
        CLAY__ELEMENT_DEFINITION_LATCH < 1; \
        CLAY__ELEMENT_DEFINITION_LATCH=1, Clay__CloseElement() \
    )
```

This is safe to nest inside a CELS `for`-loop macro because:
1. `CLAY__ELEMENT_DEFINITION_LATCH` is a `static uint8_t`, not a `for`-scoped variable
2. The inner `for` loop body is the user's Clay subtree code
3. The outer CELS `for` loop's increment runs after the inner CLAY loop completes

**Alternative considered: compound statement wrapper.** Using `do { ... } while(0)` around Clay calls would prevent the block syntax (`{ children }`) from working naturally. The sequential for-loop approach preserves the block syntax for both CELS and Clay.

**Confidence: HIGH** -- verified both macro implementations from source. The for-loop nesting pattern is standard C99 and used extensively in both codebases.

### Clay ID Generation Strategy

Clay requires unique `Clay_ElementId` values for each element. In the CELS+Clay hybrid, IDs must be deterministic across frames (Clay uses them for animation state, hover detection, scroll position).

**Recommended approach:** Use CELS entity names or `__COUNTER__`-based hashes:

```c
// For static elements:
CLAY(CLAY_ID("Sidebar"), .layout = {...}) { ... }

// For dynamic/looped elements:
CLAY(CLAY_IDI("MenuItem", index), .layout = {...}) { ... }
```

This is Clay's standard approach and requires no custom ID generation.

**Confidence: HIGH** -- standard Clay pattern, verified from clay.h and examples.

---

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Layout library | Clay v0.14 | Nuklear, cimgui | Clay is C99, single-header, zero-dep, renderer-agnostic. Nuklear bundles rendering. cimgui requires C++ runtime. |
| Layout library | Clay v0.14 | Hand-rolled flexbox | Clay is 4000 lines of battle-tested layout math. Re-implementing is months of work for no benefit. |
| Terminal lib | ncurses (via cels-ncurses) | termbox2 | Already have ncurses infrastructure. termbox2 would require replacing cels-ncurses entirely. |
| Terminal lib | ncurses (via cels-ncurses) | Raw ANSI escapes | Clay's terminal ANSI renderer uses printf+ANSI. This is fragile (no window resize handling, no wide char support, no color pair management). ncurses handles all of this. |
| Coordinate mapping | 1:1 cell units | Pixel-ratio scaling | Pixel-ratio adds rounding complexity with zero visual benefit in a terminal. Clay's layout engine is unit-agnostic. |
| FetchContent | Populate only | MakeAvailable | Clay's CMakeLists.txt builds examples (raylib, SDL, etc.) -- MakeAvailable would pull in those dependencies and fail. |
| FetchContent | GitHub tag | Local path | FetchContent from GitHub ensures reproducible builds. Local path creates machine-specific dependency. However, for development, `FETCHCONTENT_SOURCE_DIR_CLAY` can override to use the local checkout at `/home/cachy/workspaces/libs/clay`. |

---

## CLAY_IMPLEMENTATION Placement

Clay requires `#define CLAY_IMPLEMENTATION` before `#include "clay.h"` in exactly one translation unit. In the INTERFACE library pattern, this is a design decision.

**Recommended approach:** The cels-clay module's `clay_module.c` defines CLAY_IMPLEMENTATION.

```c
// src/clay_module.c
#define CLAY_IMPLEMENTATION
#include <clay.h>
#include <cels-clay/clay_module.h>
// ... module implementation
```

Since cels-clay is an INTERFACE library, this source file compiles in the consumer's context. This means:
- Exactly one translation unit includes the implementation (the module source)
- The consumer includes `<cels-clay/clay_module.h>` which includes `clay.h` (without IMPLEMENTATION)
- Multiple consumers cannot both define CLAY_IMPLEMENTATION (linker error would catch this)

**Confidence: HIGH** -- same pattern used by Clay's own examples.

---

## ncurses Primitives Needed for Clay Render Commands

Clay produces 7 render command types. The ncurses renderer needs to handle each:

| Clay Command | ncurses Primitive | Notes |
|--------------|-------------------|-------|
| `RECTANGLE` | `mvaddch()` with `ACS_*` or spaces + `COLOR_PAIR` + `A_REVERSE` | Fill rectangular area with colored spaces. Use `A_REVERSE` for background color since ncurses has limited bg color support. |
| `TEXT` | `mvaddnstr()` / `mvprintw()` | Positioned text output. Clay provides `stringContents` (NOT null-terminated -- use `addnstr` not `addstr`). |
| `BORDER` | `mvaddch()` with `ACS_ULCORNER`, `ACS_HLINE`, `ACS_VLINE`, etc. | ncurses has built-in box-drawing characters. Unicode box drawing (`\u250c` etc.) also available with wide ncurses. |
| `SCISSOR_START` | Create sub-window via `subwin()` or manual clip tracking | ncurses sub-windows clip output naturally. Alternative: track scissor rect and skip out-of-bounds draws. |
| `SCISSOR_END` | Restore to stdscr | Reverse the scissor. |
| `IMAGE` | Not applicable | Terminal v1 does not support images. Render as placeholder or skip. |
| `CUSTOM` | Application-defined | Pass through to user callback. |

**Recommended rendering approach:** Direct `mvaddch()`/`mvaddnstr()` on `stdscr` with manual scissor tracking (track a clip rect, skip draws outside it). Sub-windows add complexity (coordinate translation, refresh management) without benefit for the v1 terminal renderer.

**Color mapping:** Clay uses `Clay_Color` (RGBA floats 0-255). Map to nearest ncurses color pair. The existing cels-ncurses color pair infrastructure (6 pairs defined in tui_window.c) is insufficient for arbitrary Clay colors. The renderer should:

1. For v1: Map Clay colors to the 6 existing color pairs by nearest match (simple)
2. For future: Support 256-color or truecolor if terminal supports it

**Confidence: HIGH** -- verified ncurses capabilities and Clay render command types from source.

---

## Installation / Dependencies

```bash
# No manual installation needed. FetchContent handles Clay.
# ncurses is a system package (already required by cels-ncurses).

# For development, override FetchContent to use local Clay:
cmake -DFETCHCONTENT_SOURCE_DIR_CLAY=/home/cachy/workspaces/libs/clay ..
```

### Consumer CMakeLists.txt Integration

In the CELS root CMakeLists.txt, add alongside the existing cels-ncurses block:

```cmake
# cels-clay module (Clay layout integration)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/cels-clay/CMakeLists.txt")
    message(FATAL_ERROR "cels-clay module not found at modules/cels-clay/. "
                        "Run: git submodule update --init modules/cels-clay")
endif()
add_subdirectory(modules/cels-clay)
```

Consumer executables link both:

```cmake
target_link_libraries(my_app PRIVATE cels cels-clay cels-ncurses)
```

---

## Version Compatibility Matrix

| Component | Required Version | Current | Status |
|-----------|-----------------|---------|--------|
| Clay | v0.14 | v0.14+52 (local) | Pin to v0.14 tag |
| CELS | v0.1.0+ | v0.1.0 (main) | Compatible |
| CMake | >= 3.21 | System | Compatible |
| ncurses | Wide variant | System | Already required by cels-ncurses |
| C standard | C99 | C99 | Compatible (Clay requires C99) |
| C++ standard | C++17 (internals only) | C++17 | cels-clay is pure C99 (no C++ needed) |

---

## Sources

- Clay v0.14 source: `/home/cachy/workspaces/libs/clay/clay.h` (local checkout, verified tag)
- Clay terminal ANSI renderer: `/home/cachy/workspaces/libs/clay/renderers/terminal/clay_renderer_terminal_ansi.c`
- Clay termbox2 renderer: `/home/cachy/workspaces/libs/clay/renderers/termbox2/clay_renderer_termbox2.c`
- Clay terminal example: `/home/cachy/workspaces/libs/clay/examples/terminal-example/main.c`
- Clay CMakeLists.txt: `/home/cachy/workspaces/libs/clay/CMakeLists.txt`
- CELS macro system: `/home/cachy/workspaces/libs/cels/include/cels/cels.h`
- cels-ncurses CMakeLists.txt: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/CMakeLists.txt`
- cels-ncurses renderer: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/renderer/tui_renderer.c`
- cels-ncurses window provider: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/window/tui_window.c`
- cels-ncurses components: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/include/cels-ncurses/tui_components.h`
- cels-ncurses engine module: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/tui_engine.c`
- CELS root CMakeLists.txt: `/home/cachy/workspaces/libs/cels/CMakeLists.txt`
- cels-clay PROJECT.md: `/home/cachy/workspaces/libs/cels/modules/cels-clay/.planning/PROJECT.md`
