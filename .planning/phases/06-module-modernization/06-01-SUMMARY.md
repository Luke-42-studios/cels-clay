---
phase: 06-module-modernization
plan: 01
subsystem: core
tags: [cel-module, cel-lifecycle, cel-state, cels-v04, module-modernization]

# Dependency graph
requires:
  - phase: 03-render-bridge
    provides: ClayRenderableData component, render dispatch system, Feature/Provider pattern (now removed)
  - phase: 05-demo-app-integration
    provides: Demo app consuming Clay_Engine_use (now updated)
provides:
  - CEL_Module(Clay_Engine, init) with cels_register batch registration
  - CEL_Lifecycle(ClayEngineLC) with on_destroy for arena cleanup
  - CEL_Define_State(ClayEngineState) with cross-TU pointer registry
  - Clay_Engine_configure() API for pre-init configuration
  - Clean render bridge with no Feature/Provider remnants
  - ClaySurfaceProps with cels_lifecycle_def_t* type
affects:
  - 07-entity-primitives
  - 08-layout-rewrite
  - 09-ncurses-migration
  - 10-sdl3-renderer
  - 11-demo-app

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "CEL_Module(Name, init) { cels_register(...); } for module definition"
    - "CEL_Lifecycle(LC) + CEL_Observe(LC, on_destroy) for resource cleanup"
    - "CEL_Define_State(State) in header + CEL_State(State) in source + cels_state_bind"
    - "Clay_Engine_configure() before cels_register(Clay_Engine) for pre-init config"

key-files:
  created: []
  modified:
    - include/cels-clay/clay_engine.h
    - src/clay_engine.c
    - include/cels-clay/clay_render.h
    - src/clay_render.c
    - include/cels-clay/clay_layout.h
    - include/cels-clay/clay_ncurses_renderer.h
    - src/clay_ncurses_renderer.c
    - examples/demo/main.c

key-decisions:
  - "Arena init in CEL_Module body (not lifecycle observer) -- simpler than SDL3 pattern since Clay has only one instance"
  - "Clay_Engine_configure() replaces Clay_Engine_use() -- separates config storage from module registration"
  - "Lifecycle on_destroy for cleanup replaces atexit() -- proper CELS v0.4 resource management"
  - "Retired clay_layout_use() and clay_render_use() sub-module API -- not used by any consumer"

patterns-established:
  - "CEL_Module(Clay_Engine, init) as the canonical cels-clay module registration pattern"
  - "cels_register(Clay_Engine) in CEL_Build blocks replaces Clay_Engine_use()"
  - "ClayEngineState singleton accessible cross-TU via cel_read(ClayEngineState)"

# Metrics
duration: 11min
completed: 2026-03-16
---

# Phase 6 Plan 1: Module Modernization Summary

**CEL_Module(Clay_Engine) with CEL_Lifecycle cleanup, CEL_Define_State cross-TU singleton, and complete Feature/Provider removal**

## Performance

- **Duration:** 11 min
- **Started:** 2026-03-16T00:12:28Z
- **Completed:** 2026-03-16T00:23:34Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- Modernized Clay_Engine from deprecated cel_module to CEL_Module(Clay_Engine, init) with proper cels_register batch registration
- Replaced atexit() cleanup with CEL_Lifecycle(ClayEngineLC) on_destroy observer for proper resource management
- Added CEL_Define_State(ClayEngineState) with cross-TU pointer registry via cels_state_bind
- Removed all Feature/Provider dead code (ClayRenderable_feature_id, _clay_feature_counter, ClayRenderable_register)
- Fixed ClaySurfaceProps lifecycle type from LifecycleDef_C* to cels_lifecycle_def_t*

## Task Commits

Each task was committed atomically:

1. **Task 1: Modernize clay_engine.h/c** - `f483a68` (feat)
2. **Task 2: Remove Feature/Provider, fix ClaySurface lifecycle type** - `56397eb` (refactor)

## Files Created/Modified
- `include/cels-clay/clay_engine.h` - CEL_Module(Clay_Engine) declaration, CEL_Define_State(ClayEngineState), Clay_Engine_configure API
- `src/clay_engine.c` - CEL_Module(Clay_Engine, init) body with cels_register, CEL_Lifecycle(ClayEngineLC) with on_destroy, CEL_State + cels_state_bind
- `include/cels-clay/clay_render.h` - Cleaned header: no Feature/Provider, updated comments
- `src/clay_render.c` - Removed ClayRenderable_feature_id, _clay_feature_counter, ClayRenderable_register function and call
- `include/cels-clay/clay_layout.h` - ClaySurfaceProps uses cels_lifecycle_def_t* instead of LifecycleDef_C*
- `include/cels-clay/clay_ncurses_renderer.h` - Updated comments: no _CEL_Provides references, cels_register(Clay_Engine) usage
- `src/clay_ncurses_renderer.c` - Updated header comment: no _CEL_Provides reference
- `examples/demo/main.c` - Updated from Clay_Engine_use(NULL) to cels_register(Clay_Engine)

## Decisions Made
- **Arena init in module body, not lifecycle observer:** Clay has exactly one instance globally, so the CEL_Module body handles initialization directly. The SDL3 pattern (lifecycle observer triggers init) adds complexity without benefit here. The lifecycle on_destroy handles cleanup only.
- **Clay_Engine_configure() replaces Clay_Engine_use():** Separates configuration storage from module registration, aligning with the v0.4 pattern where cels_register(Name) is the standard module activation verb.
- **Retired sub-module API:** clay_layout_use() and clay_render_use() were never used by consumers (the demo app and all examples use Clay_Engine_use/cels_register). Removing reduces API surface.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Updated consumer code and comments across all affected files**
- **Found during:** Task 1 (Modernize clay_engine.h/c)
- **Issue:** Plan focused on clay_engine.h/c changes but didn't explicitly cover updating consumer files (demo main.c, ncurses renderer header/source comments) that reference the old Clay_Engine_use API and _CEL_Provides pattern
- **Fix:** Updated examples/demo/main.c to use cels_register(Clay_Engine), updated clay_ncurses_renderer.h and clay_ncurses_renderer.c comments to remove _CEL_Provides references and use new API names
- **Files modified:** examples/demo/main.c, include/cels-clay/clay_ncurses_renderer.h, src/clay_ncurses_renderer.c
- **Verification:** grep confirms no Clay_Engine_use or _CEL_Provides in include/ or src/
- **Committed in:** f483a68 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Consumer code update was necessary for the API change to be complete. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Module system fully modernized with CELS v0.4+ patterns
- Clean foundation for Phase 7 (Entity Primitives): all components registered via cels_register in module body
- ClayEngineState singleton available cross-TU for future state queries
- No deprecated patterns remain in include/ or src/

---
*Phase: 06-module-modernization*
*Completed: 2026-03-16*
