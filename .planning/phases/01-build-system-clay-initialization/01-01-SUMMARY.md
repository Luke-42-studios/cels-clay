---
phase: 01-build-system-clay-initialization
plan: 01
subsystem: build
tags: [cmake, clay, fetchcontent, interface-library, single-header]

# Dependency graph
requires: []
provides:
  - "CMake INTERFACE library target cels::clay with FetchContent for Clay v0.14"
  - "Public header clay_engine.h declaring Clay_Engine module API"
  - "Dedicated CLAY_IMPLEMENTATION translation unit (clay_impl.c)"
affects:
  - "01-02 (Clay initialization, arena management, error handler)"
  - "All future cels-clay plans (build on this CMake target)"

# Tech tracking
tech-stack:
  added: [Clay v0.14]
  patterns: [FetchContent_Populate for header-only libs with problematic CMakeLists, INTERFACE library module pattern]

key-files:
  created:
    - "CMakeLists.txt"
    - "include/cels-clay/clay_engine.h"
    - "src/clay_impl.c"
  modified: []

key-decisions:
  - "FetchContent_Populate over MakeAvailable to avoid Clay's example build dependencies"
  - "CLAY_SOURCE_DIR cache variable for local Clay checkout development workflow"
  - "clay_engine.h does NOT include clay.h -- consumers who need CLAY() macros include clay.h separately"

patterns-established:
  - "INTERFACE library with FetchContent_Populate: fetch source only, expose via custom target"
  - "Single CLAY_IMPLEMENTATION TU: dedicated .c file prevents ODR violations"

# Metrics
duration: 2 min
completed: 2026-02-08
---

# Phase 1 Plan 1: CMake FetchContent + INTERFACE Library Summary

**CMake INTERFACE library cels::clay with FetchContent_Populate for Clay v0.14, public module header, and dedicated CLAY_IMPLEMENTATION translation unit**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-08T07:43:10Z
- **Completed:** 2026-02-08T07:44:49Z
- **Tasks:** 2
- **Files created:** 3

## Accomplishments
- CMakeLists.txt fetches Clay v0.14 via FetchContent_Populate (avoids pulling raylib/SDL example deps)
- INTERFACE library target cels-clay with cels::clay alias, linking cels, exposing Clay + module headers
- CLAY_SOURCE_DIR cache variable enables local Clay checkout for development
- clay_engine.h declares Clay_EngineConfig struct and Clay_Engine module externals (init, use)
- clay_impl.c is the sole CLAY_IMPLEMENTATION translation unit preventing duplicate symbol errors

## Task Commits

Each task was committed atomically:

1. **Task 1: CMakeLists.txt with FetchContent Populate + INTERFACE library** - `efed725` (feat)
2. **Task 2: Public header (clay_engine.h) + CLAY_IMPLEMENTATION TU (clay_impl.c)** - `6c1a05b` (feat)

## Files Created/Modified
- `CMakeLists.txt` - INTERFACE library target with FetchContent_Populate for Clay v0.14
- `include/cels-clay/clay_engine.h` - Public module header declaring Clay_EngineConfig and Clay_Engine externals
- `src/clay_impl.c` - Dedicated CLAY_IMPLEMENTATION translation unit (single TU for Clay symbols)

## Decisions Made
- Used FetchContent_Populate instead of MakeAvailable because Clay's CMakeLists.txt builds example executables requiring raylib/SDL/etc.
- CLAY_SOURCE_DIR cache variable mirrors the local development workflow (user has Clay at ~/workspaces/libs/clay)
- clay_engine.h deliberately does NOT include clay.h -- consumers who need CLAY() macros include it separately (it's on the include path from CMakeLists.txt)
- Deferred the `if(NOT TARGET cels)` standalone guard -- consumer currently provides cels via add_subdirectory

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- CMake target ready for Plan 01-02 (Clay initialization, arena management, error handler)
- Plan 01-02 will create `src/clay_engine.c` (listed in CMake target_sources but intentionally not created yet)
- Build will not compile until `clay_engine.c` exists -- this is expected and documented in the plan

---
*Phase: 01-build-system-clay-initialization*
*Completed: 2026-02-08*
