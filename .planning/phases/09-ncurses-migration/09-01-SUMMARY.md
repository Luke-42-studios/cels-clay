---
phase: 09-ncurses-migration
plan: 01
subsystem: renderer
tags: [ncurses, cel-module, cmake, conditional-compilation, renderer-backend]

# Dependency graph
requires:
  - phase: 06-module-modernization
    provides: CEL_Module pattern (Clay_Engine), Clay_Engine_configure pre-init
  - phase: 08-layout-rewrite
    provides: Component-based layout walk, ClayRenderableData render bridge
provides:
  - CEL_Module(Clay_NCurses) for renderer selection via cels_register
  - Clay_NCurses_configure(theme) pre-init API
  - CMake if(TARGET cels-sdl3) placeholder guard for Phase 10
affects: [10-sdl3-renderer, 11-dual-renderer-example]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Renderer as CEL_Module: cels_register(Clay_NCurses) for backend selection"
    - "Pre-init configure pattern: Clay_NCurses_configure() before cels_register()"
    - "CMake conditional compilation: if(TARGET cels-*) guards for optional renderer backends"

key-files:
  modified:
    - include/cels-clay/clay_ncurses_renderer.h
    - src/clay_ncurses_renderer.c
    - CMakeLists.txt
    - examples/demo/main.c

key-decisions:
  - "g_theme initialized to NULL (not default) so module body detects if configure was called"
  - "Module body falls back to CLAY_NCURSES_THEME_DEFAULT when g_theme is NULL"

patterns-established:
  - "Renderer module pattern: CEL_Module body registers text measurement + render system"
  - "Consumer registration: cels_register(Clay_Engine, Clay_NCurses) in single call"

# Metrics
duration: 8min
completed: 2026-03-15
---

# Phase 9 Plan 1: NCurses Renderer Module Migration Summary

**NCurses renderer wrapped in CEL_Module(Clay_NCurses) with pre-init theme configure and CMake SDL3 placeholder guard**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-16T01:14:22Z
- **Completed:** 2026-03-16T01:22:00Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- NCurses renderer is now a standalone CEL_Module(Clay_NCurses) -- developers select renderer via cels_register(Clay_NCurses)
- Clay_NCurses_configure(theme) provides pre-init theme configuration matching Clay_Engine_configure pattern
- CMake infrastructure ready for dual-renderer support with if(TARGET cels-sdl3) placeholder

## Task Commits

Each task was committed atomically:

1. **Task 1: Wrap NCurses renderer in CEL_Module(Clay_NCurses)** - `930ab31` (feat)
2. **Task 2: CMake renderer infrastructure -- add SDL3 placeholder guard** - `a8fccde` (chore)

## Files Created/Modified
- `include/cels-clay/clay_ncurses_renderer.h` - Added CEL_Module(Clay_NCurses) declaration, Clay_NCurses_configure, removed clay_ncurses_renderer_init
- `src/clay_ncurses_renderer.c` - Added CEL_Module(Clay_NCurses, init) body, Clay_NCurses_configure impl, removed clay_ncurses_renderer_init
- `CMakeLists.txt` - Added if(TARGET cels-sdl3) empty guard with commented placeholders
- `examples/demo/main.c` - Updated from clay_ncurses_renderer_init(NULL) to cels_register(Clay_Engine, Clay_NCurses)

## Decisions Made
- g_theme initialized to NULL instead of &CLAY_NCURSES_THEME_DEFAULT so module body can detect if Clay_NCurses_configure was called, falling back to default if not
- Module body pattern: register text measurement + render system inside CEL_Module(Clay_NCurses, init) -- same responsibilities as old clay_ncurses_renderer_init()

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Updated demo app to use new module API**
- **Found during:** Task 1 (NCurses module wrap)
- **Issue:** examples/demo/main.c called clay_ncurses_renderer_init(NULL) which was removed -- would fail to compile
- **Fix:** Changed to cels_register(Clay_Engine, Clay_NCurses) and updated comments
- **Files modified:** examples/demo/main.c
- **Verification:** No references to clay_ncurses_renderer_init in include/ or src/
- **Committed in:** 930ab31 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Demo app update was necessary to prevent compilation failure. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- CEL_Module(Clay_NCurses) establishes the renderer module contract that Phase 10 SDL3 renderer will follow
- CMake if(TARGET cels-sdl3) guard ready for Phase 10 to uncomment and add SDL3 renderer sources
- Text measurement conflict between NCurses (cells) and SDL3 (pixels) remains a concern -- active renderer must own the Clay_SetMeasureTextFunction callback

---
*Phase: 09-ncurses-migration*
*Completed: 2026-03-15*
