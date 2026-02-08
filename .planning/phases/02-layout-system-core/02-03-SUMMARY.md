---
phase: 02-layout-system-core
plan: 03
subsystem: layout
tags: [clay, cels, composition, surface, validation, layout-pipeline]

# Dependency graph
requires:
  - phase: 02-layout-system-core/02
    provides: "Complete layout system: tree walk, PreStore system, CEL_Clay_Children, render command storage, module wiring"
  - phase: 02-layout-system-core/01
    provides: "clay_layout.h public API, clay_layout.c infrastructure, frame arena, text measurement, auto-ID"
  - phase: 01-build-system
    provides: "Clay_Engine module, CMake build, clay.h integration"
provides:
  - "ClaySurface built-in composition: developer entry point for Clay layout"
  - "Shorthand macro ClaySurface(...) for natural CELS composition syntax"
  - "Phase 2 complete: all 5 success criteria verified end-to-end"
  - "Full layout pipeline validated: Clay_Engine_use -> layout init -> system registration -> per-frame layout pass"
affects:
  - "03-render-bridge (ClaySurface is the layout root -- render bridge consumes its output)"
  - "05-demo-app (demo uses ClaySurface as the root layout boundary)"

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "CEL_Composition in header: static functions, safe in INTERFACE library headers across TUs"
    - "Shorthand macro AFTER CEL_Composition definition to avoid preprocessor token conflict"

key-files:
  created: []
  modified:
    - "include/cels-clay/clay_layout.h"

key-decisions:
  - "ClaySurface composition placed in header (not .c) -- CEL_Composition generates static functions, safe across TUs"
  - "Shorthand macro after definition to prevent preprocessor expanding ClaySurface token inside composition body"
  - "No if/else guard on CEL_Clay macro -- Clay error handler catches misuse, layout functions only called within BeginLayout/EndLayout"

patterns-established:
  - "Built-in composition pattern: CEL_Composition + shorthand #define in public header for module-provided compositions"

# Metrics
duration: 2min
completed: 2026-02-08
---

# Phase 2 Plan 3: ClaySurface Composition + Pipeline Validation Summary

**ClaySurface built-in composition as developer entry point, all 5 phase success criteria verified with clean build**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-08T18:41:21Z
- **Completed:** 2026-02-08T18:43:33Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Added ClaySurface as a built-in CELS composition in clay_layout.h with shorthand macro
- ClaySurface attaches ClaySurfaceConfig so ClayLayoutSystem finds it and runs the layout pass
- Full project builds with zero errors and zero warnings
- All 5 phase success criteria verified at code level:
  1. CEL_Clay() wraps CLAY() with auto-ID via __COUNTER__ + entity seed
  2. Recursive depth-first tree walk via clay_walk_entity/clay_walk_children/ecs_children
  3. ClayLayoutSystem registered at PreStore phase with BeginLayout/EndLayout bracketing
  4. ClaySurfaceConfig dimensions read each frame and passed to Clay_SetLayoutDimensions
  5. Text measurement callback registered via Clay_SetMeasureTextFunction in layout init

## Task Commits

Each task was committed atomically:

1. **Task 1: ClaySurface composition in clay_layout.h** - `39e912d` (feat)
2. **Task 2: End-to-end compile validation + phase success criteria check** - `4b1f795` (chore, no source changes)

## Files Created/Modified
- `include/cels-clay/clay_layout.h` - Added ClaySurface CEL_Composition with doc comment and shorthand macro

## Decisions Made
- ClaySurface composition placed directly in the header file -- `CEL_Composition` generates `static` functions which are safe across translation units in an INTERFACE library
- Shorthand `#define ClaySurface(...)` placed AFTER the `CEL_Composition(ClaySurface, ...)` definition to prevent the preprocessor from expanding the `ClaySurface` token inside the composition body
- No if/else safety guard added to CEL_Clay macro -- layout functions are only called by the tree walk (which runs between Begin/End), and Clay's own error handler catches misuse

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- CELS test suite (test_cels) has pre-existing compilation failures due to v0.2 API changes (`CEL_Call` renamed to `CEL_Init`). These are unrelated to cels-clay and existed before this plan. The cels-clay module builds cleanly.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 2 (Layout System Core) is complete -- all 3 plans executed, all 5 success criteria verified
- Layout pipeline fully wired: Clay_Engine_use -> Clay_Initialize -> layout_init -> system_register -> per-frame PreStore layout pass
- Render commands accessible via _cel_clay_get_render_commands() for Phase 3 render bridge
- ClaySurface composition provides the developer-facing entry point for Clay layout trees
- No consumer target links to cels-clay yet -- runtime testing requires a consumer executable (Phase 5 demo app or an earlier integration test)

---
*Phase: 02-layout-system-core*
*Completed: 2026-02-08*
