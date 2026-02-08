---
phase: 02-layout-system-core
plan: 01
subsystem: layout
tags: [clay, layout, component, frame-arena, text-measurement, auto-id, macros]

# Dependency graph
requires:
  - phase: 01-build-system-clay-initialization
    plan: 02
    provides: "Clay_Engine module init with arena, Clay_Initialize, error handler"
provides:
  - "clay_layout.h: complete public API header for layout system (CEL_Clay, CEL_Clay_Layout, CEL_Clay_Children, CEL_Clay_Text)"
  - "ClayUI component with layout_fn pointer for attaching layout functions to entities"
  - "ClaySurfaceConfig component with width/height for layout dimensions"
  - "16KB per-frame bump arena for dynamic string lifetime management"
  - "Terminal text measurement function (character-cell counting)"
  - "Auto-ID generation via Clay__HashNumber(__COUNTER__, entity_id)"
affects:
  - "02-02-PLAN (tree walk, layout system) builds on types, macros, and state from this plan"
  - "02-03-PLAN (ClaySurface, validation) uses ClaySurfaceConfig component and init/cleanup lifecycle"
  - "All future layout work references clay_layout.h as the public API"

# Tech tracking
tech-stack:
  added: []
  patterns: [bump-allocator frame arena, __COUNTER__-based auto-ID, Clay__HashNumber for element IDs]

key-files:
  created:
    - "include/cels-clay/clay_layout.h"
    - "src/clay_layout.c"
  modified:
    - "CMakeLists.txt"

key-decisions:
  - "Component named ClayUI (not ClayLayout) to match API-DESIGN.md reference code and API-02"
  - "CEL_Clay uses __COUNTER__ (not __LINE__) for per-call-site uniqueness even with same-line macros"
  - "Frame arena fixed at 16KB with overflow-to-stderr (no dynamic growth)"
  - "Forward-declared Clay__HashNumber since it lacks CLAY_DLL_EXPORT but linker symbol exists from clay_impl.c"
  - "Text measurement: 1 char = 1 unit width, newlines increment height (terminal cell model)"

patterns-established:
  - "extern component registration: ClayUIID/ensure() pattern for cross-file module components"
  - "Frame arena bump allocation: malloc once, bump offset, reset per frame, free on cleanup"
  - "Layout pass state via static globals: g_layout_world, g_layout_current_entity, g_layout_pass_active"

# Metrics
duration: 3 min
completed: 2026-02-08
---

# Phase 2 Plan 1: Public Header + Layout Infrastructure Summary

**clay_layout.h with CEL_Clay/CEL_Clay_Layout/CEL_Clay_Children/CEL_Clay_Text macros, ClayUI component, 16KB frame arena, terminal text measurement, auto-ID via Clay__HashNumber**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-08T18:29:30Z
- **Completed:** 2026-02-08T18:32:44Z
- **Tasks:** 2
- **Files created/modified:** 3

## Accomplishments
- clay_layout.h provides the complete public API: 4 macros (CEL_Clay, CEL_Clay_Layout, CEL_Clay_Children, CEL_Clay_Text), 2 component types (ClayUI, ClaySurfaceConfig), all internal function declarations, and lifecycle function declarations
- clay_layout.c implements all infrastructure: frame arena (16KB bump allocator with overflow protection), terminal text measurement (character-cell counting with newline support), auto-ID generation, component registration via cels_component_register, init/cleanup lifecycle
- CEL_Clay() wraps CLAY() with auto-generated Clay_ElementId from entity ID seed + __COUNTER__ offset, producing globally unique IDs
- CMakeLists.txt updated with clay_layout.c in target_sources
- Full project compiles cleanly with zero errors, no duplicate symbols, no undefined references

## Task Commits

Each task was committed atomically:

1. **Task 1: clay_layout.h complete public header** - `bbf1d68` (feat)
2. **Task 2: clay_layout.c infrastructure + CMakeLists.txt update** - `f474583` (feat)

## Files Created/Modified
- `include/cels-clay/clay_layout.h` - Complete public API: ClayUI + ClaySurfaceConfig types, CEL_Clay/CEL_Clay_Layout/CEL_Clay_Children/CEL_Clay_Text macros, internal and lifecycle function declarations
- `src/clay_layout.c` - Infrastructure: component registration, 16KB frame arena, text measurement, auto-ID, layout pass state, init/cleanup, stubs for tree walk and system registration
- `CMakeLists.txt` - Added clay_layout.c to target_sources INTERFACE

## Decisions Made
- Component named ClayUI (not ClayLayout) for consistency with API-DESIGN.md reference code and requirement API-02
- CEL_Clay uses __COUNTER__ (not __LINE__) because __COUNTER__ increments per macro expansion, so two CEL_Clay calls on the same line still get unique IDs
- Frame arena fixed at 16KB -- terminal UIs are small (typically < 100 dynamic strings at < 64 bytes each). Overflow logs to stderr and returns empty string (no crash)
- Forward-declared Clay__HashNumber since it lacks CLAY_DLL_EXPORT in clay.h header section but the linker symbol exists from CLAY_IMPLEMENTATION in clay_impl.c
- Text measurement uses 1 char = 1 unit width with newline-based height counting (terminal cell model matching Clay's own terminal renderer example)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Forward declaration for Clay__HashNumber**
- **Found during:** Task 2
- **Issue:** Clay__HashNumber is only defined in the CLAY_IMPLEMENTATION section of clay.h (line 1359) but lacks a CLAY_DLL_EXPORT declaration in the header section. clay_layout.c (which does NOT define CLAY_IMPLEMENTATION) cannot see the function.
- **Fix:** Added `extern Clay_ElementId Clay__HashNumber(const uint32_t offset, const uint32_t seed);` forward declaration in clay_layout.c. The linker symbol exists from clay_impl.c.
- **Files modified:** src/clay_layout.c
- **Commit:** f474583

## Issues Encountered
None beyond the Clay__HashNumber forward declaration (handled as deviation above).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- clay_layout.h provides all types and macros needed by Plan 02 (layout system, tree walk)
- Frame arena, text measurement, and auto-ID infrastructure are ready for the layout pass
- Stubs for _cel_clay_emit_children and _cel_clay_layout_system_register are in place for Plan 02 to implement
- Layout pass state globals (g_layout_world, g_layout_current_entity, g_layout_pass_active) ready for tree walk
- No blockers for Plan 02

---
*Phase: 02-layout-system-core*
*Completed: 2026-02-08*
