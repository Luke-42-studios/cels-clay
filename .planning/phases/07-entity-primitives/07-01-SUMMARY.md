---
phase: 07-entity-primitives
plan: 01
subsystem: ui
tags: [clay, ecs, components, compositions, layout-primitives]

# Dependency graph
requires:
  - phase: 06-module-modernization
    provides: CEL_Module pattern, cels_register for component registration
provides:
  - 5 component structs (ClayContainerConfig, ClayTextConfig, ClaySpacerConfig, ClayImageConfig, ClayBorderStyle)
  - 6 static-linkage compositions (ClayRow, ClayColumn, ClayBox, ClayText, ClaySpacer, ClayImage)
  - Component registration via clay_primitives.c
  - Module-level component registration in Clay_Engine init
affects:
  - 08-layout-rewrite (layout walker reads these components to emit CLAY() calls)
  - 09-ncurses-migration (renderer reads ClayContainerConfig/ClayTextConfig for rendering)
  - 10-sdl3-renderer (same)
  - 11-demo-app (uses compositions to build UI)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Static-linkage composition pattern with Name_props/Name_impl/Name_factory naming"
    - "Additive component pattern (ClayBorderStyle can be attached to any container)"
    - "Composition-level defaults (ClayText defaults color to white, font_size to 16)"

key-files:
  created:
    - include/cels-clay/clay_primitives.h
    - src/clay_primitives.c
  modified:
    - CMakeLists.txt
    - src/clay_engine.c
    - include/cels-clay/clay_layout.h

key-decisions:
  - "Used Clay_BorderElementConfig (color + width) instead of plan's Clay_Border (which doesn't exist in Clay v0.14)"
  - "Fixed composition naming to use Name_props/Name_impl (underscore + lowercase) matching _cel_compose_impl macro requirements"
  - "Added const char* id field to all composition props structs for spawn-once/key compatibility"

patterns-established:
  - "Static-linkage composition: Name_props, Name_impl, Name_factory, #define Name(...) cel_init(Name, ...)"
  - "Additive components: ClayBorderStyle attaches conditionally only when border widths are non-zero"

# Metrics
duration: 27min
completed: 2026-03-16
---

# Phase 7 Plan 1: Entity Primitives Summary

**5 component structs and 6 static-linkage compositions for declarative Clay UI entity composition (Row/Column/Box/Text/Spacer/Image)**

## Performance

- **Duration:** 27 min
- **Started:** 2026-03-16T00:26:50Z
- **Completed:** 2026-03-16T00:54:43Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Created 5 component types mapping Clay layout properties to ECS components (ClayContainerConfig, ClayTextConfig, ClaySpacerConfig, ClayImageConfig, ClayBorderStyle)
- Created 6 static-linkage compositions (ClayRow, ClayColumn, ClayBox, ClayText, ClaySpacer, ClayImage) with call macros for natural DSL syntax
- Fixed ClaySurface naming convention to match CELS macro requirements (ClaySurfaceProps -> ClaySurface_props, ClaySurface_Impl -> ClaySurface_impl)
- Wired all 5 primitive components into Clay_Engine module initialization via cels_register()

## Task Commits

Each task was committed atomically:

1. **Task 1: Create clay_primitives.h -- component structs and static compositions** - `0353170` (feat)
2. **Task 2: Create clay_primitives.c -- component registration and CMake update** - `5780cef` (feat)

**Plan metadata:** (pending)

## Files Created/Modified
- `include/cels-clay/clay_primitives.h` - Component structs (5) and static-linkage compositions (6) with call macros
- `src/clay_primitives.c` - Component ID externs and registration functions with idempotent guards
- `CMakeLists.txt` - Added clay_primitives.c to INTERFACE target_sources
- `src/clay_engine.c` - Added clay_primitives.h include and cels_register for all 5 primitive components
- `include/cels-clay/clay_layout.h` - Fixed ClaySurface naming to match _cel_compose_impl expectations

## Decisions Made
- **Clay_BorderElementConfig instead of Clay_Border:** Plan referenced `Clay_Border` which doesn't exist in Clay v0.14. Used the actual Clay type `Clay_BorderElementConfig` (which has `.color` and `.width` fields). This correctly maps to Clay's border API.
- **Composition naming convention fix:** Discovered that the existing `ClaySurface` used `ClaySurfaceProps` and `ClaySurface_Impl`, but the CELS `_cel_compose_impl` macro expects `Name_props` and `Name_impl` (underscore + lowercase). All new compositions use the correct naming, and ClaySurface was fixed.
- **Added `const char* id` field:** All composition props include an `id` field for spawn-once/key compatibility, matching the pattern from `CEL_Define_Composition` and `_cel_compose_impl` (which references `.id` for key hashing).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed ClaySurface composition naming convention**
- **Found during:** Task 1 (creating clay_primitives.h)
- **Issue:** Existing ClaySurface used `ClaySurfaceProps` and `ClaySurface_Impl` but the CELS `_cel_compose_impl` macro generates references to `ClaySurface_props` and `ClaySurface_impl` (underscore + lowercase). Compilation test confirmed: `error: unknown type name 'ClaySurface_props'; did you mean 'ClaySurfaceProps'?`
- **Fix:** Renamed to `ClaySurface_props`, `ClaySurface_impl`, `ClaySurface_factory` (factory was already correct)
- **Files modified:** include/cels-clay/clay_layout.h
- **Verification:** Compilation test with `cel_init(ClaySurface, .width=80, .height=24) {}` passes
- **Committed in:** 0353170 (Task 1 commit)

**2. [Rule 1 - Bug] Used Clay_BorderElementConfig instead of non-existent Clay_Border**
- **Found during:** Task 1 (creating component structs)
- **Issue:** Plan specified `Clay_Border` as the type for ClayBorderStyle.border and ClayBoxProps.border, but Clay v0.14 has no `Clay_Border` type. The correct type is `Clay_BorderElementConfig` (with `.color` Clay_Color and `.width` Clay_BorderWidth fields).
- **Fix:** Used `Clay_BorderElementConfig` in ClayBorderStyle struct and ClayBox_props
- **Files modified:** include/cels-clay/clay_primitives.h
- **Verification:** Header compiles cleanly; border width check `props.border.width.top || ...` works correctly with `Clay_BorderWidth` fields
- **Committed in:** 0353170 (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (2 bugs)
**Impact on plan:** Both fixes necessary for correctness. The naming fix prevents compilation errors in all ClaySurface usage. The type fix uses the actual Clay v0.14 API. No scope creep.

## Issues Encountered
None - plan tasks executed successfully after deviations were resolved.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All 5 component types are defined and registered, ready for Phase 8 layout walker to read them
- All 6 compositions are available for UI construction in Phase 11 demo app
- Component registration is wired into Clay_Engine module init, so components are available as soon as the engine is registered
- No blockers for Phase 8 (layout rewrite)

---
*Phase: 07-entity-primitives*
*Completed: 2026-03-16*
