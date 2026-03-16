---
phase: 11-dual-renderer-example
plan: 01
subsystem: ui
tags: [clay, ncurses, sdl3, dual-renderer, example, cmake]

# Dependency graph
requires:
  - phase: 10-sdl3-renderer
    provides: "CEL_Module(Clay_SDL3) with Clay_SDL3_configure(config)"
  - phase: 09-ncurses-renderer
    provides: "CEL_Module(Clay_NCurses) with Clay_NCurses_configure(theme)"
  - phase: 07-entity-primitives
    provides: "ClayRow, ClayColumn, ClayBox, ClayText, ClaySpacer compositions"
  - phase: 08-layout-dispatch
    provides: "Layout walker dispatches by component presence"
  - phase: 06-clay-engine-module
    provides: "CEL_Module(Clay_Engine) with CEL_Lifecycle and CEL_State"
provides:
  - "Shared ui.h entity tree using primitive compositions (no raw CLAY macros)"
  - "NCurses example: terminal rendering with module registration pattern"
  - "SDL3 example: graphical rendering with module registration pattern"
  - "CMake CELS_CLAY_BUILD_EXAMPLES option with conditional backend targets"
  - "Canonical dual-renderer bootstrap pattern for future apps"
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Shared UI header pattern: static inline build_ui() in .h included by both backends"
    - "cels_main() + cels_register(modules) + cels_session(Root) bootstrap pattern"
    - "CEL_Compose(Root) wraps backend window + ClaySurface + shared build_ui()"

key-files:
  created:
    - "examples/minimal/ui.h"
    - "examples/minimal/main_ncurses.c"
    - "examples/minimal/main_sdl3.c"
  modified:
    - "CMakeLists.txt"

key-decisions:
  - "Used primitive compositions directly (ClayRow/ClayColumn/etc) instead of custom CEL_Compose wrappers in ui.h -- avoids static linkage issues in INTERFACE library header"
  - "ClaySurface dimensions: 40x24 for NCurses (80/2.0 aspect ratio), 1280x720 for SDL3 (pixel-accurate)"
  - "OnUpdate phase shorthand for QuitInput system (current v0.4 API, not deprecated CELS_Phase_OnUpdate)"

patterns-established:
  - "Dual-renderer example: shared ui.h + backend-specific main_*.c with identical cels_session loop"
  - "CMake example guard: CELS_CLAY_BUILD_EXAMPLES option with per-backend TARGET checks"

# Metrics
duration: 9min
completed: 2026-03-15
---

# Phase 11 Plan 01: Dual-Renderer Example Summary

**Sidebar + content layout rendered identically on NCurses terminal and SDL3 graphical window via shared ui.h entity tree**

## Performance

- **Duration:** 9 min
- **Started:** 2026-03-16T04:31:16Z
- **Completed:** 2026-03-16T04:40:30Z
- **Tasks:** 4
- **Files modified:** 4

## Accomplishments
- Shared UI entity tree (ui.h) using ClayColumn, ClayRow, ClayBox, ClayText, ClaySpacer with high-contrast dark theme and relative sizing
- NCurses example bootstraps terminal rendering with aspect-ratio-compensated ClaySurface dimensions
- SDL3 example bootstraps graphical rendering with pixel-accurate ClaySurface dimensions
- CMake build option controls example compilation with per-backend availability guards

## Task Commits

Each task was committed atomically:

1. **Task 1: Create examples/minimal/ui.h** - `397dc1c` (feat)
2. **Task 2: Create examples/minimal/main_ncurses.c** - `7412821` (feat)
3. **Task 3: Create examples/minimal/main_sdl3.c** - `25bbbfc` (feat)
4. **Task 4: CMake targets** - `93165b0` (feat)

## Files Created/Modified
- `examples/minimal/ui.h` - Shared UI entity tree with sidebar + content layout, high-contrast colors, relative sizing
- `examples/minimal/main_ncurses.c` - NCurses backend entry point: module registration, root composition, QuitInput system, frame loop
- `examples/minimal/main_sdl3.c` - SDL3 backend entry point: renderer config, module registration, root composition, frame loop
- `CMakeLists.txt` - CELS_CLAY_BUILD_EXAMPLES option, conditional cels-clay-example-ncurses and cels-clay-example-sdl3 targets

## Decisions Made
- **Primitive compositions directly instead of custom wrappers:** The plan suggested custom `CEL_Compose(SidebarItem)` etc. in ui.h, but `CEL_Compose` generates non-static functions which would cause multiple-definition errors when the header is included by both TUs. Used ClayRow/ClayColumn primitives directly instead -- simpler and avoids the INTERFACE library static linkage issue.
- **OnUpdate shorthand:** Used `OnUpdate` phase name (current API) instead of deprecated `CELS_Phase_OnUpdate` enum. The cels-ncurses module code uses the shorthand form.
- **cel_run block for QuitInput:** Used `cel_run { ... }` pattern for systems that read state without entity query, matching the current cels-ncurses example convention.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed CEL_Compose static linkage issue in ui.h**
- **Found during:** Task 1 (UI entity tree design)
- **Issue:** Plan suggested `CEL_Compose(SidebarItem)`, `CEL_Compose(Sidebar)` etc. in the shared header, but CEL_Compose generates non-static `Name_factory` and `Name_impl` functions. Including ui.h from both main_ncurses.c and main_sdl3.c would cause multiple-definition linker errors in the INTERFACE library pattern.
- **Fix:** Used the existing primitive compositions (ClayRow, ClayColumn, ClayBox, ClayText, ClaySpacer) directly with nested structure instead of custom wrappers. These primitives already use static linkage in clay_primitives.h.
- **Files modified:** examples/minimal/ui.h
- **Verification:** No multiple-definition risk; header only defines `static inline build_ui()`
- **Committed in:** 397dc1c

**2. [Rule 1 - Bug] Fixed incorrect API usage in plan pseudocode**
- **Found during:** Task 2 (NCurses entry point)
- **Issue:** Plan suggested `cels_main { cels_scope { cels_register(...) } cels_session { ... } }` block-nesting pattern which doesn't match the actual CELS v0.4 API. The real API is `cels_main() { ... }` (generates main()), `cels_register(...)` (bare call), `cels_session(Root) { ... }` (takes composition name).
- **Fix:** Used the correct API pattern matching cels-ncurses minimal.c: `cels_main() { cels_register(NCurses, Clay_Engine, Clay_NCurses); cels_session(NCursesApp) { while(cels_running()) cels_step(0); } }`
- **Files modified:** examples/minimal/main_ncurses.c, examples/minimal/main_sdl3.c
- **Verification:** Code matches the canonical pattern from cels-ncurses examples/minimal.c
- **Committed in:** 7412821, 25bbbfc

---

**Total deviations:** 2 auto-fixed (2 bugs -- API mismatch with actual codebase)
**Impact on plan:** Both fixes necessary for correct compilation. No scope creep. The entity tree design and overall architecture match the plan exactly.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- v0.6 milestone complete: all 6 phases (6-11) delivered
- Entity primitives, layout system, NCurses renderer, SDL3 renderer, and dual-renderer example are all in place
- The example establishes the canonical bootstrap pattern for future cels-clay applications

---
*Phase: 11-dual-renderer-example*
*Completed: 2026-03-15*
