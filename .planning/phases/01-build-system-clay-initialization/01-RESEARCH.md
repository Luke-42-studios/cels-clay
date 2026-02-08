# Phase 1: Build System + Clay Initialization - Research

**Researched:** 2026-02-07
**Domain:** CMake FetchContent integration, single-header C library compilation, arena allocation, error handler wiring
**Confidence:** HIGH

## Summary

Phase 1 is pure build infrastructure and initialization plumbing. The task is to fetch Clay v0.14 via CMake FetchContent (using the Populate pattern to avoid pulling in Clay's example dependencies), expose it as a CMake INTERFACE library called `cels::clay`, define `CLAY_IMPLEMENTATION` in exactly one translation unit, allocate and initialize Clay's arena, and wire an error handler that prints to stderr.

The standard approach is well-established: cels-ncurses already demonstrates the INTERFACE library pattern, FetchContent Populate is the documented solution for header-only libraries with problematic CMakeLists.txt files, and Clay's initialization API is a straightforward 3-call sequence (MinMemorySize, CreateArenaWithCapacityAndMemory, Initialize). The error handler is a simple callback that receives `Clay_ErrorData` with an error type enum and human-readable text.

**Primary recommendation:** Follow the cels-ncurses CMakeLists.txt pattern exactly, substituting ncurses-specific lines with Clay FetchContent + include path lines. Put `CLAY_IMPLEMENTATION` in one `.c` source file. Initialize Clay in the `_CEL_DefineModule` body. Register `atexit` cleanup for the arena.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Clay | v0.14 | Single-header C99 flexbox layout engine | Only dependency this phase introduces. Pinned to v0.14 release tag. GitHub: `https://github.com/nicbarker/clay.git` |
| CMake | >= 3.21 | Build system with FetchContent | Matches CELS root CMakeLists.txt minimum version. FetchContent stable since 3.14. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| CELS | v0.1.0+ | ECS framework (linked via INTERFACE) | Always -- cels-clay depends on cels for `_CEL_DefineModule`, `cels_entity_t`, module registration |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| FetchContent_Populate | FetchContent_MakeAvailable | MakeAvailable calls `add_subdirectory` on Clay's CMakeLists.txt which builds examples (raylib, SDL, etc.), pulling unwanted dependencies and likely failing. Populate only fetches the source. |
| INTERFACE library | STATIC library | INTERFACE matches cels-ncurses pattern. STATIC would require CELS to link Clay as a separate object, but Clay is header-only -- there is nothing to compile independently. CLAY_IMPLEMENTATION produces all symbols in the consumer's TU. |
| atexit cleanup | ECS on-destroy observer | CELS v0.1 has no module-level cleanup hook. cels-ncurses uses `atexit(cleanup_endwin)` for terminal cleanup. Same pattern works for Clay arena free. |

**Installation:**
```bash
# No manual installation needed. FetchContent handles Clay at configure time.
# For development, override FetchContent to use local Clay checkout:
cmake -DCLAY_SOURCE_DIR=/home/cachy/workspaces/libs/clay ..
```

## Architecture Patterns

### Recommended Project Structure
```
modules/cels-clay/
  CMakeLists.txt          # INTERFACE library, FetchContent, target definitions
  include/
    cels-clay/
      clay_engine.h       # Module public header (Clay_Engine_use, config struct)
  src/
    clay_impl.c           # CLAY_IMPLEMENTATION defined here (one TU only)
    clay_engine.c         # _CEL_DefineModule(Clay_Engine), arena init, error handler
```

### Pattern 1: FetchContent Populate (not MakeAvailable)
**What:** Fetch Clay source without calling `add_subdirectory` on it, because Clay's own CMakeLists.txt only builds examples and has no INTERFACE target.
**When to use:** Always for this project. Clay's CMakeLists.txt lines 61-62 show the INTERFACE target is commented out. Lines 1-57 build example executables requiring raylib, SDL, etc.
**Example:**
```cmake
# Source: /home/cachy/workspaces/libs/clay/CMakeLists.txt (verified)
include(FetchContent)

# Option for local development override
set(CLAY_SOURCE_DIR "" CACHE PATH "Path to local Clay source (overrides FetchContent)")

if(CLAY_SOURCE_DIR)
    # Use local clone for development
    set(clay_SOURCE_DIR "${CLAY_SOURCE_DIR}")
else()
    # Fetch from GitHub
    FetchContent_Declare(
        clay
        GIT_REPOSITORY https://github.com/nicbarker/clay.git
        GIT_TAG        v0.14
    )
    FetchContent_GetProperties(clay)
    if(NOT clay_POPULATED)
        FetchContent_Populate(clay)
    endif()
endif()
```

### Pattern 2: INTERFACE Library (matches cels-ncurses)
**What:** `add_library(cels-clay INTERFACE)` with sources, includes, and link dependencies all INTERFACE.
**When to use:** This is the established cels module pattern. Sources compile in the consumer's translation unit.
**Example:**
```cmake
# Source: /home/cachy/workspaces/libs/cels/modules/cels-ncurses/CMakeLists.txt (verified pattern)
add_library(cels-clay INTERFACE)
add_library(cels::clay ALIAS cels-clay)

target_sources(cels-clay INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_engine.c
)

target_include_directories(cels-clay INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${clay_SOURCE_DIR}          # Provides clay.h
)

target_link_libraries(cels-clay INTERFACE
    cels
)
```

### Pattern 3: CLAY_IMPLEMENTATION in Dedicated TU
**What:** A single `.c` file defines `CLAY_IMPLEMENTATION` before including `clay.h`. All other files include `clay.h` without the define (header-only declarations).
**When to use:** Required. Clay mandates this pattern (clay.h lines 3-11).
**Example:**
```c
// src/clay_impl.c -- THE ONLY FILE that defines CLAY_IMPLEMENTATION
#define CLAY_IMPLEMENTATION
#include "clay.h"
// Nothing else needed. This file exists solely to instantiate Clay's implementation.
```

### Pattern 4: Module Init with _CEL_DefineModule
**What:** Clay initialization (arena alloc, Clay_Initialize, error handler) runs inside the `_CEL_DefineModule` body, triggered by `Clay_Engine_use()`.
**When to use:** Module startup. Follows the cels-ncurses `TUI_Engine` pattern exactly.
**Example:**
```c
// Source: cels-ncurses/src/tui_engine.c (verified pattern)
static Clay_EngineConfig g_clay_config = {0};

_CEL_DefineModule(Clay_Engine) {
    // Arena allocation
    uint32_t min_memory = Clay_MinMemorySize();
    uint32_t arena_size = g_clay_config.arena_size > 0
        ? g_clay_config.arena_size
        : min_memory;
    void* memory = malloc(arena_size);
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(arena_size, memory);

    // Initialize Clay
    Clay_Initialize(arena, (Clay_Dimensions){0, 0}, (Clay_ErrorHandler){
        .errorHandlerFunction = clay_error_handler
    });

    // Register cleanup
    atexit(clay_cleanup);
}

void Clay_Engine_use(Clay_EngineConfig config) {
    g_clay_config = config;
    Clay_Engine_init();  // idempotent
}
```

### Pattern 5: Error Handler Callback
**What:** Clay calls an error handler function with `Clay_ErrorData` containing error type enum + human-readable text. The handler prints to stderr and continues (never aborts).
**When to use:** Registered during `Clay_Initialize`.
**Example:**
```c
// Source: Clay_ErrorData struct verified from clay.h lines 800-824
static void clay_error_handler(Clay_ErrorData error) {
    const char* type_str = "unknown";
    switch (error.errorType) {
        case CLAY_ERROR_TYPE_TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED:
            type_str = "text measurement function not provided"; break;
        case CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED:
            type_str = "arena capacity exceeded"; break;
        case CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED:
            type_str = "elements capacity exceeded"; break;
        case CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED:
            type_str = "text measurement capacity exceeded"; break;
        case CLAY_ERROR_TYPE_DUPLICATE_ID:
            type_str = "duplicate ID"; break;
        case CLAY_ERROR_TYPE_FLOATING_CONTAINER_PARENT_NOT_FOUND:
            type_str = "floating container parent not found"; break;
        case CLAY_ERROR_TYPE_PERCENTAGE_OVER_1:
            type_str = "percentage over 1"; break;
        case CLAY_ERROR_TYPE_INTERNAL_ERROR:
            type_str = "internal error"; break;
        case CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE:
            type_str = "unbalanced open/close"; break;
    }
    fprintf(stderr, "[cels-clay] %s: %.*s\n",
            type_str, error.errorText.length, error.errorText.chars);
}
```

### Anti-Patterns to Avoid
- **Using FetchContent_MakeAvailable for Clay:** This calls `add_subdirectory` on Clay's CMakeLists.txt, which attempts to build all example executables (raylib, SDL, termbox). These will fail or pull unwanted dependencies. Use `FetchContent_Populate` only.
- **Defining CLAY_IMPLEMENTATION in a header:** This will cause duplicate symbol errors when multiple TUs include the header. It must be in exactly one `.c` file.
- **Calling Clay_Initialize in a header or at global scope:** Module init happens in the `_CEL_DefineModule` body, which runs when `Clay_Engine_use()` is called from the consumer's `CEL_Build` block. Never at file scope.
- **Forgetting to store the arena memory pointer:** The `void*` passed to `Clay_CreateArenaWithCapacityAndMemory` must be retained for `free()` at cleanup. Clay does not free it.
- **Using Clay_MinMemorySize as a maximum:** `Clay_MinMemorySize()` returns the minimum. For production use, allocate more (e.g., 2x or a configurable value) to avoid `CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED` errors at runtime.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Arena sizing | Custom size calculation | `Clay_MinMemorySize()` | Clay knows its own internal data structure sizes. The function accounts for `maxElementCount` and `maxMeasureTextCacheWordCount` settings. Manual calculation would be wrong. |
| Error type strings | Manual string table | `Clay_ErrorData.errorText` field | Clay already provides human-readable error descriptions in every error callback. The error type enum is for programmatic dispatch, not string formatting. |
| FetchContent guard | Manual `if(TARGET clay)` checks | `FetchContent_GetProperties` + `if(NOT clay_POPULATED)` | Standard CMake FetchContent idiom. Prevents double-fetching. |
| CMake alias target | No alias, just `cels-clay` | `add_library(cels::clay ALIAS cels-clay)` | Namespaced alias (`cels::clay`) follows modern CMake conventions. Matches `cels::cels` alias in CELS root. |

**Key insight:** Phase 1 has zero novel engineering. Every pattern has a working reference: cels-ncurses for the INTERFACE library + module pattern, Clay's own examples for initialization, and CELS root CMakeLists.txt for FetchContent. The risk is not complexity but deviation from established patterns.

## Common Pitfalls

### Pitfall 1: CLAY_IMPLEMENTATION in Multiple Translation Units (ODR Violation)
**What goes wrong:** If `CLAY_IMPLEMENTATION` is defined in more than one `.c` file (or in a header included by multiple files), the linker sees duplicate definitions for every Clay function. Build fails with "multiple definition of Clay_Initialize" etc.
**Why it happens:** Clay is a single-header library that uses the `#define IMPLEMENTATION` / `#include` pattern. The implementation section (clay.h lines 947-4428) is guarded by `#ifdef CLAY_IMPLEMENTATION` and `#undef`s the define after expansion. But if two TUs both define it before including, both get the implementation.
**How to avoid:** Create a dedicated `clay_impl.c` that ONLY defines `CLAY_IMPLEMENTATION` and includes `clay.h`. All other `.c` files include `clay.h` without the define.
**Warning signs:** Linker errors mentioning Clay function names with "multiple definition" or "already defined in."

### Pitfall 2: Arena Too Small (Clay_MinMemorySize Not Called)
**What goes wrong:** If the arena is allocated with an arbitrary fixed size (e.g., 1MB) without consulting `Clay_MinMemorySize()`, Clay may fail during `Clay_Initialize` or later during layout when internal arrays need more space. The error handler fires with `CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED`.
**Why it happens:** `Clay_MinMemorySize()` calculates the exact minimum based on `maxElementCount` (default 8192) and `maxMeasureTextCacheWordCount` (default 16384). These defaults produce a minimum around ~2.5MB. A naive "1MB should be enough" allocation is too small.
**How to avoid:** Always call `Clay_MinMemorySize()` to get the baseline. Allocate at least that much, preferably with headroom. If the consumer passes a custom size, validate it against `Clay_MinMemorySize()`.
**Warning signs:** Error handler fires immediately on startup or after first `Clay_BeginLayout`.

### Pitfall 3: Forgetting to Free Arena Memory
**What goes wrong:** The memory passed to `Clay_CreateArenaWithCapacityAndMemory` is allocated via `malloc`. Clay does not own this memory and will not free it. If the module does not track the pointer and free it on shutdown, the arena leaks.
**Why it happens:** `Clay_CreateArenaWithCapacityAndMemory` takes a `void*` pointer and wraps it in a `Clay_Arena` struct, but the struct only stores `char* memory` which is offset by cacheline alignment (up to 64 bytes). The original `malloc` pointer is different from `arena.memory`. You must keep the original pointer for `free()`.
**How to avoid:** Store the `malloc` result in a static variable before passing it to Clay. Free it in an `atexit` handler (same pattern as cels-ncurses `cleanup_endwin`).
**Warning signs:** Valgrind/ASan reports a leak of the arena-sized allocation.

### Pitfall 4: Clay_Initialize Called Before CLAY_IMPLEMENTATION TU is Linked
**What goes wrong:** In an INTERFACE library, sources compile in the consumer's context. If the consumer's build system processes files in an order where `Clay_Initialize` is called from code that hasn't seen `CLAY_IMPLEMENTATION`, the linker finds no implementation. This manifests as undefined reference errors.
**Why it happens:** INTERFACE library sources are listed in `target_sources` but compiled as part of the consumer target. The linker resolves all symbols at once, so as long as any one `.c` file in the consumer target defines `CLAY_IMPLEMENTATION`, all Clay functions are available.
**How to avoid:** List `clay_impl.c` (the file with `CLAY_IMPLEMENTATION`) in `target_sources`. This ensures it is always compiled as part of the consumer target. This is the correct behavior -- not a bug but a point of confusion.
**Warning signs:** "undefined reference to Clay_Initialize" at link time.

### Pitfall 5: FetchContent_MakeAvailable Pulling Clay's Example Dependencies
**What goes wrong:** Using `FetchContent_MakeAvailable(clay)` calls `add_subdirectory` on Clay's fetched source. Clay's CMakeLists.txt (lines 8-57) has `CLAY_INCLUDE_ALL_EXAMPLES` defaulting to `ON`, which tries to build raylib, SDL2, SDL3, and sokol examples. These require external libraries that are not installed, causing CMake configure errors.
**Why it happens:** Clay's CMakeLists.txt is designed for Clay's own development, not for consumers. The INTERFACE library target (lines 61-62) is commented out with `#`.
**How to avoid:** Use `FetchContent_Populate` (which only fetches the source, does not call `add_subdirectory`). Then create your own INTERFACE target that points to `${clay_SOURCE_DIR}` for includes.
**Warning signs:** CMake configure fails with "Could NOT find raylib" or "SDL2 not found" when building a project that uses cels-clay.

### Pitfall 6: Cacheline Alignment Offset in Clay Arena
**What goes wrong:** `Clay_Initialize` aligns the arena memory pointer to a 64-byte boundary (clay.h lines 4042-4044). This means `arena.memory` after initialization is NOT the same pointer as the `malloc` result. If you try to `free(arena.memory)`, you free a misaligned pointer, causing undefined behavior.
**How to avoid:** Store the original `malloc` pointer separately. Free that pointer, not `arena.memory`.
**Warning signs:** Double-free or heap corruption errors in ASan/Valgrind.

## Code Examples

Verified patterns from source analysis:

### Clay Arena Allocation + Initialize (Full Sequence)
```c
// Source: clay.h lines 830-844, 3920-3948, 4040-4068

// Step 1: Get minimum memory size
uint32_t min_memory = Clay_MinMemorySize();

// Step 2: Allocate (keep original pointer for free)
static void* g_clay_arena_memory = NULL;
uint32_t arena_capacity = min_memory;  // or larger
g_clay_arena_memory = malloc(arena_capacity);

// Step 3: Create arena struct (wraps pointer + capacity)
Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(arena_capacity, g_clay_arena_memory);

// Step 4: Initialize Clay with arena, initial dimensions, and error handler
Clay_Context* ctx = Clay_Initialize(arena,
    (Clay_Dimensions){ .width = 0.0f, .height = 0.0f },  // Updated later when window state is known
    (Clay_ErrorHandler){ .errorHandlerFunction = clay_error_handler }
);
// ctx is non-NULL on success
```

### Clay Error Type Enum (All Values)
```c
// Source: clay.h lines 778-798
typedef CLAY_PACKED_ENUM {
    CLAY_ERROR_TYPE_TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED,  // No Clay_SetMeasureTextFunction call
    CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED,                 // Arena too small for Clay's internals
    CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED,              // Too many elements (increase via Clay_SetMaxElementCount)
    CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED,      // Text cache full (increase via Clay_SetMaxMeasureTextCacheWordCount)
    CLAY_ERROR_TYPE_DUPLICATE_ID,                            // Two elements share the same ID
    CLAY_ERROR_TYPE_FLOATING_CONTAINER_PARENT_NOT_FOUND,     // CLAY_ATTACH_TO_ELEMENT_WITH_ID with invalid parentId
    CLAY_ERROR_TYPE_PERCENTAGE_OVER_1,                       // CLAY_SIZING_PERCENT value > 1.0
    CLAY_ERROR_TYPE_INTERNAL_ERROR,                          // Bug in Clay itself
    CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE,                   // More OpenElement than CloseElement calls
} Clay_ErrorType;
```

### Clay_ErrorData Struct
```c
// Source: clay.h lines 800-816
typedef struct Clay_ErrorData {
    Clay_ErrorType errorType;   // Enum identifying the error
    Clay_String errorText;      // Human-readable description (NOT null-terminated)
    void *userData;             // Transparent pointer from Clay_ErrorHandler registration
} Clay_ErrorData;
```

### Clay_ErrorHandler Struct
```c
// Source: clay.h lines 818-824
typedef struct {
    void (*errorHandlerFunction)(Clay_ErrorData errorText);  // Callback
    void *userData;                                           // Passed through to callback
} Clay_ErrorHandler;
```

### CELS Module Pattern (Reference: cels-ncurses)
```c
// Source: /home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/tui_engine.c

// Header declares:
//   extern cels_entity_t Clay_Engine;
//   extern void Clay_Engine_init(void);
//   extern void Clay_Engine_use(Clay_EngineConfig config);

static Clay_EngineConfig g_clay_config = {0};

_CEL_DefineModule(Clay_Engine) {
    // Module init body -- runs once (idempotent guard in macro)
    // ... Clay initialization goes here ...
}

void Clay_Engine_use(Clay_EngineConfig config) {
    g_clay_config = config;
    Clay_Engine_init();  // triggers _CEL_DefineModule body
}
```

### atexit Cleanup Pattern (Reference: cels-ncurses)
```c
// Source: /home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/window/tui_window.c lines 58-63

static void* g_clay_arena_memory = NULL;

static void clay_cleanup(void) {
    if (g_clay_arena_memory != NULL) {
        free(g_clay_arena_memory);
        g_clay_arena_memory = NULL;
    }
}

// Registered during module init:
atexit(clay_cleanup);
```

### CELS FetchContent for Self-Resolution
```c
// CMake pattern: cels-clay fetches CELS if not already available
# (in CMakeLists.txt)
if(NOT TARGET cels)
    include(FetchContent)
    FetchContent_Declare(
        cels
        GIT_REPOSITORY https://github.com/[user]/cels.git
        GIT_TAG        v0.1.0
    )
    FetchContent_MakeAvailable(cels)
endif()
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `FetchContent_Populate` + manual `add_subdirectory` | `FetchContent_MakeAvailable` (one call) | CMake 3.14 (2019) | MakeAvailable is preferred for libraries with proper CMakeLists.txt. But Clay's CMakeLists.txt builds examples, so Populate is correct HERE. |
| Fixed arena size (hardcoded bytes) | `Clay_MinMemorySize()` + configurable override | Clay v0.12+ | Dynamic sizing avoids capacity errors. Always call MinMemorySize. |
| Global `Clay__currentContext` only | Multi-context via `Clay_GetCurrentContext` / `Clay_SetCurrentContext` | Clay v0.14 | Enables multiple Clay instances. Phase 1 uses single context, but store the pointer for future multi-context support. |

**Deprecated/outdated:**
- Clay's commented-out INTERFACE target (CMakeLists.txt lines 61-62): Not usable. Create your own.
- `FETCHCONTENT_SOURCE_DIR_CLAY` (generic CMake override): Works but is a global CMake variable. A project-specific `CLAY_SOURCE_DIR` option is clearer.

## Open Questions

Things that couldn't be fully resolved:

1. **Exact default arena size recommendation**
   - What we know: `Clay_MinMemorySize()` with defaults (8192 elements, 16384 text cache words) returns approximately 2.5MB. This is the minimum.
   - What's unclear: Whether the planner should recommend a configurable multiplier (e.g., 2x minimum) or just use the minimum. For terminal UI with limited element count, the minimum is likely sufficient.
   - Recommendation: Use `Clay_MinMemorySize()` directly as the default. Let the consumer override via `Clay_EngineConfig.arena_size`. If a custom size is smaller than MinMemorySize, clamp to MinMemorySize and print a warning.

2. **Whether to store Clay_Context pointer**
   - What we know: `Clay_Initialize` returns a `Clay_Context*`. The global `Clay__currentContext` tracks it. For single-context use, you can ignore the return value.
   - What's unclear: Whether storing it is needed for Phase 1 or is premature.
   - Recommendation: Store it in a static variable for future multi-context support, but don't use it in Phase 1.

3. **CELS self-resolution via FetchContent**
   - What we know: The CONTEXT.md says "cels-clay fetches CELS via FetchContent if not already available."
   - What's unclear: The CELS repo URL for FetchContent (it's a local workspace, not on GitHub yet). Also, the `cels` target is STATIC, not INTERFACE, so `FetchContent_MakeAvailable` should work for it.
   - Recommendation: Add a `if(NOT TARGET cels)` guard with a placeholder FetchContent block. For now, the consumer project (CELS root) already has `cels` available via `add_subdirectory`.

## Sources

### Primary (HIGH confidence)
- Clay v0.14 header: `/home/cachy/workspaces/libs/clay/clay.h` -- verified initialization API (lines 830-844, 3920-3948, 4040-4068), error handler types (lines 777-824), arena struct (lines 216-220), CLAY_IMPLEMENTATION guard (lines 947-4428)
- Clay CMakeLists.txt: `/home/cachy/workspaces/libs/clay/CMakeLists.txt` -- verified example-only build, commented-out INTERFACE target (lines 61-62)
- cels-ncurses CMakeLists.txt: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/CMakeLists.txt` -- INTERFACE library reference pattern
- cels-ncurses tui_engine.c: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/tui_engine.c` -- `_CEL_DefineModule` pattern, `Clay_Engine_use` pattern
- cels-ncurses tui_window.c: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/window/tui_window.c` -- `atexit` cleanup pattern (lines 58-63, 74)
- CELS cels.h: `/home/cachy/workspaces/libs/cels/include/cels/cels.h` -- `_CEL_DefineModule` macro (lines 1301-1309), `cels_entity_t` (line 61), `cels_module_register` (line 1281)
- CELS CMakeLists.txt: `/home/cachy/workspaces/libs/cels/CMakeLists.txt` -- FetchContent pattern for flecs/yyjson, module add_subdirectory pattern (lines 132-137)
- cels-clay CONTEXT.md: `/home/cachy/workspaces/libs/cels/modules/cels-clay/.planning/phases/01-build-system-clay-initialization/01-CONTEXT.md` -- locked decisions
- cels-clay STACK.md: `/home/cachy/workspaces/libs/cels/modules/cels-clay/.planning/research/STACK.md` -- project-level research on Clay+CMake integration

### Secondary (HIGH confidence -- same-project research)
- cels-clay SUMMARY.md: `/home/cachy/workspaces/libs/cels/modules/cels-clay/.planning/research/SUMMARY.md`
- cels-clay PITFALLS.md: `/home/cachy/workspaces/libs/cels/modules/cels-clay/.planning/research/PITFALLS.md`
- cels-clay ARCHITECTURE.md: `/home/cachy/workspaces/libs/cels/modules/cels-clay/.planning/research/ARCHITECTURE.md`

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- Clay v0.14 verified from local checkout, CMake FetchContent is well-documented, all patterns verified from existing cels modules
- Architecture: HIGH -- Direct mirror of cels-ncurses patterns, no novel design decisions
- Pitfalls: HIGH -- All 6 pitfalls verified by reading Clay source code and cels-ncurses implementation
- Code examples: HIGH -- All code derived from verified source files with line numbers

**Research date:** 2026-02-07
**Valid until:** 2026-03-07 (30 days -- stable domain, Clay v0.14 is a tagged release)
