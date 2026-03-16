---
phase: 08-layout-system-rewrite
plan: 01
subsystem: ui
tags: [clay, layout, ecs, component-dispatch, tree-walk]

# Dependency graph
requires:
  - phase: 07-entity-primitives
    provides: ClayContainerConfig, ClayTextConfig, ClaySpacerConfig, ClayImageConfig, ClayBorderStyle component structs and IDs
provides:
  - Property-driven tree walk in clay_walk_entity() dispatching by component presence
  - Cleaned clay_layout.h with no ClayUI or CEL_Clay_Layout
  - Emit functions translating component data to CLAY()/CLAY_TEXT() calls
affects: [09-ncurses-migration, 10-sdl3-renderer, 11-demo-app]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Component-presence dispatch: ecs_get_id() checks replace function pointer dispatch"
    - "Emit function pattern: static emit_*(world, entity, config) generates Clay elements"
    - "Sizing safety net: _sizing_or_grow() treats zero-initialized Clay_SizingAxis as GROW(0)"

key-files:
  created: []
  modified:
    - src/clay_layout.c
    - include/cels-clay/clay_layout.h
    - src/clay_engine.c
    - src/clay_primitives.c
    - include/cels-clay/clay_primitives.h

key-decisions:
  - "Fixed auto-ID discriminators (0-3) per emit function instead of __COUNTER__ -- entity ID provides uniqueness via seed"
  - "Clay_ClipElementConfig with .horizontal/.vertical booleans for clip (not enum -- Clay v0.14 API)"
  - "Clay_ImageElementConfig only has .imageData -- no sourceDimensions field in Clay v0.14"
  - "Zero-initialized Clay_SizingAxis treated as GROW(0) via _sizing_or_grow() helper"

patterns-established:
  - "Component-presence dispatch: layout walker checks for component structs instead of calling function pointers"
  - "Emit function separation: each primitive type has its own static emit function for clean separation"

# Metrics
duration: 13min
completed: 2026-03-16
---

# Phase 8 Plan 1: Layout System Rewrite Summary

**Property-driven tree walk replacing layout_fn dispatch with ClayContainerConfig/ClayTextConfig/ClaySpacerConfig/ClayImageConfig component-presence checks and emit functions**

## Performance

- **Duration:** 13 min
- **Started:** 2026-03-16T00:58:21Z
- **Completed:** 2026-03-16T01:11:22Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Rewrote clay_walk_entity() to dispatch by component presence instead of ClayUI.layout_fn pointer
- Implemented 4 emit functions: emit_container (with ClayBorderStyle additive check), emit_text, emit_spacer, emit_image
- Removed all ClayUI and CEL_Clay_Layout references from headers and source files
- Added _sizing_or_grow() helper to treat zero-initialized sizing axes as CLAY_SIZING_GROW(0)
- Verified clean compilation via consumer project (persona)

## Task Commits

Each task was committed atomically:

1. **Task 1: Rewrite clay_walk_entity() with component-presence dispatch** - `e6dbb87` (feat)
2. **Task 2: Remove ClayUI from clay_layout.h** - `432283a` (refactor)

**Plan metadata:** (pending)

## Files Created/Modified
- `src/clay_layout.c` - Rewrote clay_walk_entity() with component dispatch, added 4 emit functions, removed ClayUI_id/ClayUI_register
- `include/cels-clay/clay_layout.h` - Removed ClayUI typedef, CelClayLayoutFn, CEL_Clay_Layout macro; updated header comment
- `src/clay_engine.c` - Removed ClayUI from cels_register() call
- `src/clay_primitives.c` - Updated comment referencing ClayUI to ClaySurfaceConfig
- `include/cels-clay/clay_primitives.h` - Updated comment referencing ClayUI_id to ClaySurfaceConfig_id

## Decisions Made
- Used fixed integer discriminators (0-3) for auto-ID in emit functions rather than __COUNTER__. Since emit functions are regular C functions (not macros), __COUNTER__ would produce the same value every call. The entity ID seed provides uniqueness per entity.
- Adapted Clay API usage to actual v0.14 structs: Clay_ClipElementConfig uses .horizontal/.vertical bools (not enum), Clay_ImageElementConfig has only .imageData (no sourceDimensions).
- Added _sizing_or_grow() safety net: zero-initialized Clay_SizingAxis (type=FIT, min=0, max=0) would collapse to zero size; converted to GROW(0) for sensible defaults.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Removed ClayUI from cels_register() in clay_engine.c**
- **Found during:** Task 1 (clay_layout.c rewrite)
- **Issue:** clay_engine.c line 116 had `cels_register(ClaySurfaceConfig, ClayUI)` which would fail to compile after ClayUI removal
- **Fix:** Changed to `cels_register(ClaySurfaceConfig)` -- ClayUI is no longer a component
- **Files modified:** src/clay_engine.c
- **Verification:** Consumer project builds successfully
- **Committed in:** e6dbb87 (Task 1 commit)

**2. [Rule 1 - Bug] Fixed Clay API usage for clip and image**
- **Found during:** Task 1 (emit function implementation)
- **Issue:** Plan's emit_container used `CLAY_CLIP_ELEMENT`/`CLAY_CLIP_NONE` enums and emit_image used `.sourceDimensions` -- neither exist in Clay v0.14
- **Fix:** Used `Clay_ClipElementConfig { .horizontal = true, .vertical = true }` for clip; used `Clay_ImageElementConfig { .imageData = config->source }` for image
- **Files modified:** src/clay_layout.c
- **Verification:** Clean compilation against actual clay.h
- **Committed in:** e6dbb87 (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 bug)
**Impact on plan:** Both fixes necessary for correct compilation against actual Clay v0.14 API. No scope creep.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Layout system now reads property components from entity primitives (Phase 7)
- Ready for Phase 9 (NCurses Migration) -- renderer code is unchanged, only layout tree generation changed
- CEL_Clay and CEL_Clay_Children macros preserved for any advanced layout patterns

---
*Phase: 08-layout-system-rewrite*
*Completed: 2026-03-16*
