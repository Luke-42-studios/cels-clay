---
phase: 02-layout-system-core
plan: 02
subsystem: layout
tags: [clay, flecs, ecs, tree-walk, layout-system, prestore]

# Dependency graph
requires:
  - phase: 02-layout-system-core/01
    provides: "clay_layout.h public API, clay_layout.c infrastructure stubs, component registration, frame arena, text measurement"
  - phase: 01-build-system
    provides: "Clay_Engine module, CMake build, clay.h integration"
provides:
  - "Complete layout system: depth-first entity tree walk with transparent pass-through"
  - "CEL_Clay_Children child emission at caller-chosen position in CLAY tree"
  - "PreStore layout system: ClaySurface query -> SetDimensions -> BeginLayout -> walk -> EndLayout"
  - "Render command storage (_cel_clay_get_render_commands) for render bridge"
  - "Module init wiring: layout init + system registration + cleanup"
affects:
  - "02-layout-system-core/03 (smoke test / integration test)"
  - "03-render-bridge (consumes render commands via _cel_clay_get_render_commands)"
  - "04-ncurses-renderer (needs render commands to draw)"

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Direct ecs_system_init for standalone zero-query systems (matching cels-ncurses)"
    - "ecs_children/ecs_children_next for parent-child tree iteration"
    - "ecs_each_id for component-based entity queries"
    - "ecs_iter_t* callback signature for direct flecs system registration"

key-files:
  created: []
  modified:
    - "src/clay_layout.c"
    - "src/clay_engine.c"

key-decisions:
  - "System callback uses ecs_iter_t* (flecs native) not CELS_Iter* -- direct flecs system requires flecs callback signature"
  - "Render commands stored in static global, returned by value via _cel_clay_get_render_commands"
  - "Layout cleanup called before Clay arena free in clay_cleanup (independent allocations, safe ordering)"

patterns-established:
  - "Tree walk pattern: clay_walk_entity (recursive) + clay_walk_children (iteration) -- mutually recursive depth-first"
  - "Save/restore g_layout_current_entity for nested CEL_Clay_Children calls"
  - "Layout pass bracketed by g_layout_pass_active flag -- guards CEL_Clay_Children from misuse"

# Metrics
duration: 4min
completed: 2026-02-08
---

# Phase 2 Plan 2: Layout System Core Summary

**Depth-first entity tree walk with PreStore layout system, CEL_Clay_Children emission, and Clay_Engine module wiring**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-08T18:35:26Z
- **Completed:** 2026-02-08T18:39:04Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Replaced Plan 01 stubs with complete layout system implementation in clay_layout.c
- Depth-first recursive tree walk: entities with ClayUI get layout_fn called, non-ClayUI entities are transparent pass-throughs
- CEL_Clay_Children delegates to clay_walk_children at call site, giving developers control over child placement in CLAY tree
- PreStore layout system finds ClaySurface entities via ClaySurfaceConfig query, runs full BeginLayout/EndLayout pass
- Module init in clay_engine.c wires layout subsystem lifecycle (init after Clay_Initialize, cleanup before arena free)

## Task Commits

Each task was committed atomically:

1. **Task 1: Entity tree walk + CEL_Clay_Children + layout system** - `f24779e` (feat)
2. **Task 2: Wire layout subsystem into Clay_Engine module init** - `3e46b72` (feat)

## Files Created/Modified
- `src/clay_layout.c` - Complete layout system: tree walk, CEL_Clay_Children, PreStore system, system registration, render command storage
- `src/clay_engine.c` - Module init wiring: layout init, system registration, cleanup ordering

## Decisions Made
- System callback uses `ecs_iter_t*` (flecs native) instead of plan's `CELS_Iter*` -- direct `ecs_system_init` requires flecs callback signature, matching the established cels-ncurses pattern exactly
- Render commands stored in module-static `g_last_render_commands`, returned by value -- simple, no allocation, sufficient for single-surface use
- Layout cleanup called before Clay arena free -- frame arena and Clay arena are independent allocations

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed nonexistent CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE**
- **Found during:** Task 2 (clay_engine.c modification)
- **Issue:** Pre-existing error handler case referenced `CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE` which does not exist in Clay v0.14
- **Fix:** Removed the nonexistent case from the switch statement
- **Files modified:** src/clay_engine.c
- **Verification:** Syntax check passes, all 8 valid Clay error types still handled
- **Committed in:** 3e46b72 (Task 2 commit)

**2. [Rule 1 - Bug] Changed system callback signature from CELS_Iter* to ecs_iter_t***
- **Found during:** Task 1 (layout system implementation)
- **Issue:** Plan specified `ClayLayoutSystem_Run(CELS_Iter* it)` but direct `ecs_system_init` requires flecs-native `ecs_iter_t*` callback signature (verified against cels-ncurses tui_frame.c and tui_input.c)
- **Fix:** Used `ClayLayoutSystem_callback(ecs_iter_t* it)` matching the established pattern
- **Files modified:** src/clay_layout.c
- **Verification:** Syntax check passes, callback signature matches cels-ncurses reference implementations
- **Committed in:** f24779e (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (2 bugs)
**Impact on plan:** Both fixes necessary for correct compilation. No scope creep.

## Issues Encountered
- INTERFACE library means cels-clay sources only compile in consumer context -- no consumer target links to cels-clay yet, so verification used `gcc -fsyntax-only` with all include paths instead of CMake build

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Layout system complete: tree walk, child emission, PreStore system, module wiring all implemented
- Ready for Plan 03 (smoke test / integration test) to verify the pipeline end-to-end
- Render commands stored and accessible for Phase 3 render bridge
- No consumer target links to cels-clay yet -- Plan 03 or a later phase will need to create one for runtime testing

---
*Phase: 02-layout-system-core*
*Completed: 2026-02-08*
