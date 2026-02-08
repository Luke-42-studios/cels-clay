---
phase: 04-ncurses-clay-renderer
plan: 01
subsystem: rendering
tags: [ncurses, clay, renderer, tui, wcwidth, aspect-ratio, 256-color]

# Dependency graph
requires:
  - phase: 03-render-bridge-module-definition
    provides: ClayRenderable Feature, ClayRenderableData singleton, _CEL_Provides pattern
provides:
  - ClayNcursesTheme struct with configurable border/scrollbar/aspect-ratio/alpha
  - Provider callback rendering all 5 Clay command types to terminal
  - wcwidth-based text measurement callback for Clay layout
  - Conditional CMake wiring (renderer compiled only when cels-ncurses available)
affects: [04-02 scroll navigation, 05-demo-app]

# Tech tracking
tech-stack:
  added: [wchar.h/wcwidth, math.h/roundf]
  patterns: [dual bbox converter (aspect-ratio vs text), stack-buffer-with-malloc-fallback for StringSlice]

key-files:
  created:
    - modules/cels-clay/include/cels-clay/clay_ncurses_renderer.h
    - modules/cels-clay/src/clay_ncurses_renderer.c
  modified:
    - modules/cels-clay/CMakeLists.txt

key-decisions:
  - "TUI_BORDER_SINGLE for v1 borders -- default theme matches exactly, custom chars deferred to v2"
  - "Dual bbox converters: clay_bbox_to_cells (aspect-ratio) vs clay_text_bbox_to_cells (cell-accurate)"
  - "Text x-position still aspect-scaled even for text bbox -- only width is unscaled"
  - "Omitted init_theme_border_chars cchar_t conversion -- not needed when using TUI_BORDER_SINGLE"
  - "Stack buffer 512 bytes for StringSlice null-termination, malloc fallback for long strings"

patterns-established:
  - "Dual coordinate mapping: aspect-ratio-scaled for geometry, unscaled for text"
  - "Stack-buffer-with-malloc-fallback for Clay_StringSlice null-termination"
  - "Conditional CMake compilation: if(TARGET dep) guard for optional module sources"

# Metrics
duration: 4min
completed: 2026-02-08
---

# Phase 4 Plan 1: ncurses Clay Renderer Summary

**350-line terminal renderer translating Clay render commands to ncurses via cels-ncurses drawing API, with wcwidth text measurement and 2:1 aspect-ratio compensation**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-08T20:47:27Z
- **Completed:** 2026-02-08T20:51:09Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Complete renderer translating all 5 Clay render command types (rectangle, text, border, scissor start/end) to terminal output via cels-ncurses primitives
- wcwidth-based text measurement callback returning accurate cell-column dimensions for CJK and multi-byte text
- Dual coordinate mapping: aspect-ratio-scaled for non-text geometry, unscaled for text bounding boxes
- Theme struct with configurable border characters, scrollbar characters, aspect ratio, and alpha-as-dim handling
- Conditional CMake integration: renderer source compiled only when cels-ncurses target exists

## Task Commits

Each task was committed atomically:

1. **Task 1: Public header with theme struct and renderer API** - `f380325` (feat)
2. **Task 2: Renderer implementation and CMake wiring** - `5edfa57` (feat)

## Files Created/Modified

- `include/cels-clay/clay_ncurses_renderer.h` - ClayNcursesTheme struct, CLAY_NCURSES_THEME_DEFAULT, init/set_theme declarations
- `src/clay_ncurses_renderer.c` - Provider callback, 5 command handlers, text measurement, coordinate mapping (350 lines)
- `CMakeLists.txt` - Conditional `if(TARGET cels-ncurses)` block for renderer source and link

## Decisions Made

1. **TUI_BORDER_SINGLE for v1 borders** - The default theme uses single-line Unicode box-drawing characters which match TUI_BORDER_SINGLE exactly. Omitted the init_theme_border_chars cchar_t conversion since it's not needed. Custom theme chars beyond single/double/rounded can add a custom draw path in v2.

2. **Dual bbox converters** - `clay_bbox_to_cells` applies aspect ratio to horizontal values (x, width) for rectangles/borders/scissors. `clay_text_bbox_to_cells` does NOT scale width because text measurement already returns cell-column-accurate widths. However, text x-position IS still scaled to stay aligned with the geometry coordinate system.

3. **Stack buffer for StringSlice** - 512-byte stack buffer with malloc fallback for null-termination of Clay_StringSlice (used in both render_text and measure_text). Avoids per-frame allocation for typical text lengths.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Omitted init_theme_border_chars cchar_t conversion**
- **Found during:** Task 2 (renderer implementation)
- **Issue:** Plan specified converting theme UTF-8 strings to cchar_t via mbstowcs/setcchar. But the REVISED approach in the plan itself recommends using TUI_BORDER_SINGLE directly for v1, which makes the cchar_t conversion unnecessary.
- **Fix:** Removed static border char state and init_theme_border_chars function. Borders use tui_draw_border with TUI_BORDER_SINGLE enum directly.
- **Files modified:** src/clay_ncurses_renderer.c
- **Verification:** CMake configures successfully, border rendering uses tui_draw_border API correctly
- **Committed in:** 5edfa57

---

**Total deviations:** 1 simplification (plan's own REVISED approach followed)
**Impact on plan:** Reduces code complexity with no functional loss. Theme border chars can be added in v2.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Renderer is complete for static rendering (rectangles, text, borders, scissors)
- Plan 04-02 (scroll container navigation) can proceed immediately
- End-to-end visual verification requires a consumer app linking both cels-clay and cels-ncurses
- Requirements covered: REND-01 through REND-07. REND-08 (scroll navigation) is in plan 04-02.

---
*Phase: 04-ncurses-clay-renderer*
*Completed: 2026-02-08*
