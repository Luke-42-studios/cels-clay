---
phase: 03-render-bridge
plan: 02
subsystem: ui
tags: [clay, ecs, module-definition, config-api, composable-init, flecs]

# Dependency graph
requires:
  - phase: 03-render-bridge
    plan: 01
    provides: "Render bridge (clay_render.h/c) with ClayRenderable Feature, dispatch system, and wiring in clay_engine.c"
  - phase: 02-layout-system
    provides: "Layout system with ClayLayoutSystem, frame arena, text measurement"
provides:
  - "ClayEngineConfig with arena_size, initial_width, initial_height fields"
  - "Pointer-based Clay_Engine_use(const ClayEngineConfig*) API"
  - "Composable sub-module functions clay_layout_use() and clay_render_use()"
  - "Full module init order: arena -> layout init -> render init -> systems -> cleanup"
  - "Transitive clay_render.h include from clay_engine.h"
affects: [04-ncurses-renderer]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Pointer-based config: Clay_Engine_use(const ClayEngineConfig*) with NULL for defaults"
    - "Composable sub-modules: individual _use() functions for each subsystem"
    - "Config struct with zero-value defaults: {0} means all defaults"

key-files:
  created: []
  modified:
    - include/cels-clay/clay_engine.h
    - src/clay_engine.c

key-decisions:
  - "Renamed Clay_EngineConfig to ClayEngineConfig (no underscore, consistent C naming)"
  - "No error_handler in config -- internal handler sufficient, avoids Clay_ErrorData coupling in public header"
  - "Composable wrappers do NOT allocate Clay arena -- caller responsible for Clay_Initialize when using individual pieces"
  - "NULL config parameter means use all defaults (zero-initialized static config)"

patterns-established:
  - "Config pointer pattern: pass const T* config, NULL means defaults, copy to static on non-NULL"
  - "Composable module pattern: full _use() bundles everything, sub-module _use() for individual pieces"

# Metrics
duration: 2min
completed: 2026-02-08
---

# Phase 3 Plan 2: Module Definition Summary

**Pointer-based ClayEngineConfig API with initial dimensions, composable clay_layout_use/clay_render_use sub-module wrappers**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-08T19:39:49Z
- **Completed:** 2026-02-08T19:41:42Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- ClayEngineConfig expanded with initial_width and initial_height fields for Clay_Initialize dimensions
- Clay_Engine_use changed from by-value to const pointer API, with NULL meaning all defaults
- Composable sub-module functions clay_layout_use() and clay_render_use() for advanced users
- clay_render.h included transitively from clay_engine.h so consumers get full API from one include

## Task Commits

Each task was committed atomically:

1. **Task 1: Expand config struct and change to pointer-based API** - `248cdbc` (feat)
2. **Task 2: Restructure module init, add composable wrappers, update CMake** - `1321d29` (feat)

**Plan metadata:** (pending)

## Files Created/Modified
- `include/cels-clay/clay_engine.h` - Expanded config struct, pointer-based signature, composable declarations, clay_render.h include
- `src/clay_engine.c` - Pointer-based Clay_Engine_use, config dimensions in Clay_Initialize, composable wrapper functions

## Decisions Made
- **Config struct rename:** Clay_EngineConfig -> ClayEngineConfig (consistent C naming without mixed underscore)
- **No error_handler in config:** The internal error handler is sufficient for all use cases. Adding Clay_ErrorData dependency to the public header creates unnecessary coupling.
- **Composable wrappers skip arena:** clay_layout_use() and clay_render_use() only init+register their subsystem. Clay arena allocation and Clay_Initialize are the caller's responsibility when using composable pieces directly.
- **NULL config for defaults:** Clay_Engine_use(NULL) is valid and uses zero-initialized config (arena_size=0 means Clay_MinMemorySize, dimensions=0 means defer to ClaySurface).

## Deviations from Plan

None - plan executed exactly as written. The 03-01 executor had already added clay_render.h include and render bridge calls to clay_engine.c, and clay_render.c to CMakeLists.txt. This plan only needed to restructure the existing code (config expansion, pointer API, composable wrappers).

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 3 (Render Bridge + Module Definition) is COMPLETE
- Clay_Engine_use(&(ClayEngineConfig){0}) initializes the full pipeline: arena + layout + render bridge
- Phase 4 (ncurses renderer) can implement _CEL_Provides(NcursesBackend, ClayRenderable, ...) to receive render commands
- No blockers

---
*Phase: 03-render-bridge*
*Completed: 2026-02-08*
