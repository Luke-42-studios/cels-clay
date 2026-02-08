---
phase: 03-render-bridge
plan: 01
subsystem: ui
tags: [clay, ecs, feature-provider, render-bridge, flecs, singleton-entity]

# Dependency graph
requires:
  - phase: 02-layout-system
    provides: "Layout system with ClayLayoutSystem at PreStore, _cel_clay_get_render_commands(), clay_engine.c module"
provides:
  - "ClayRenderable Feature at OnStore phase for backend registration"
  - "ClayRenderableData component with render commands, dimensions, frame metadata"
  - "Singleton ClayRenderTarget entity updated each frame by dispatch system"
  - "Public getter API cel_clay_get_render_commands() for advanced users"
  - "Layout dimension tracking via _cel_clay_get_layout_dimensions()"
affects: [04-ncurses-renderer, 03-02-module-definition]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "CELS Feature/Provider pattern: _CEL_DefineFeature at file scope, _CEL_Feature in init function"
    - "Singleton entity pattern: one entity with component, updated by dispatch system, queried by providers"
    - "Phase ordering: PreStore (layout) -> OnStore (dispatch) -> OnStore (provider, created lazily)"

key-files:
  created:
    - include/cels-clay/clay_render.h
    - src/clay_render.c
  modified:
    - include/cels-clay/clay_layout.h
    - src/clay_layout.c
    - src/clay_engine.c
    - CMakeLists.txt

key-decisions:
  - "Used _cel_clay_get_render_commands() internal getter (not public) for render dispatch to layout bridge"
  - "dirty flag set to (commands.length > 0) -- simple always-dirty approach per research recommendation"
  - "Render dispatch system registered in clay_engine.c BEFORE provider finalization for correct ordering within OnStore"

patterns-established:
  - "Feature/Provider: file-scope _CEL_DefineFeature, init-function _CEL_Feature, consumer _CEL_Provides"
  - "Singleton render target: one entity, dispatch system writes, provider system reads"
  - "Subsystem lifecycle: _init creates entities/registers components, _system_register creates flecs systems"

# Metrics
duration: 3min
completed: 2026-02-08
---

# Phase 3 Plan 1: Render Bridge Summary

**ClayRenderable Feature/Provider bridge with singleton dispatch at OnStore, connecting layout output to renderer backends**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-08T19:34:41Z
- **Completed:** 2026-02-08T19:37:31Z
- **Tasks:** 1
- **Files modified:** 6 (2 created, 4 modified)

## Accomplishments
- ClayRenderable Feature defined at CELS_Phase_OnStore with _CEL_DefineFeature at file scope
- ClayRenderableData component registered with CELS, containing render_commands, layout dimensions, frame_number, delta_time, and dirty flag
- Singleton ClayRenderTarget entity created during module init, updated every frame by ClayRenderDispatch system
- Layout dimensions tracked in clay_layout.c and exposed to render dispatch via _cel_clay_get_layout_dimensions()
- Public getter API cel_clay_get_render_commands() available for advanced users bypassing Feature/Provider
- Complete wiring into clay_engine.c module body: init, system registration, and CMakeLists.txt source listing

## Task Commits

Each task was committed atomically:

1. **Task 1: Layout dimension tracking + render bridge header and implementation** - `8a3f51c` (feat)

**Plan metadata:** (pending)

## Files Created/Modified
- `include/cels-clay/clay_render.h` - ClayRenderableData struct, component ID, getter API, render lifecycle declarations
- `src/clay_render.c` - Feature definition, component registration, singleton entity, dispatch system at OnStore
- `include/cels-clay/clay_layout.h` - Added _cel_clay_get_render_commands and _cel_clay_get_layout_dimensions declarations
- `src/clay_layout.c` - Added g_last_layout_dimensions storage and getter function
- `src/clay_engine.c` - Added clay_render.h include, _cel_clay_render_init and _cel_clay_render_system_register calls
- `CMakeLists.txt` - Added src/clay_render.c to INTERFACE sources

## Decisions Made
- **dirty flag strategy:** Set to `(commands.length > 0)` -- simple "always dirty when commands exist" approach. More sophisticated dirty tracking (hashing, comparing) deferred to optimization phase per research recommendation.
- **Dispatch-to-layout bridge:** Render dispatch uses internal `_cel_clay_get_render_commands()` and `_cel_clay_get_layout_dimensions()` functions, not public API. This keeps the internal cross-file bridge separate from the consumer-facing getter.
- **System registration order:** Layout system registered first (PreStore), then render dispatch (OnStore), both during _CEL_DefineModule body. Provider systems are created lazily on first Engine_Progress, guaranteeing dispatch runs before provider within OnStore.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added _cel_clay_get_render_commands declaration to clay_layout.h**
- **Found during:** Task 1 (compilation check)
- **Issue:** `_cel_clay_get_render_commands()` was defined in clay_layout.c but never declared in any header. clay_render.c includes clay_layout.h and calls this function, causing implicit function declaration error.
- **Fix:** Added `extern Clay_RenderCommandArray _cel_clay_get_render_commands(void);` to the Internal Function Declarations section of clay_layout.h.
- **Files modified:** include/cels-clay/clay_layout.h
- **Verification:** gcc -fsyntax-only passes cleanly for all source files
- **Committed in:** 8a3f51c (part of task commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Missing declaration was a pre-existing gap from Phase 2 (function only used within clay_layout.c before). Now that clay_render.c needs cross-file access, the declaration is required. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Render bridge is complete and ready for backend registration via `_CEL_Provides(Backend, ClayRenderable, ClayRenderableData, callback)`
- Phase 3 Plan 2 (module definition restructuring) can proceed
- Phase 4 (ncurses renderer) can use ClayRenderableData to implement terminal rendering
- No blockers

---
*Phase: 03-render-bridge*
*Completed: 2026-02-08*
