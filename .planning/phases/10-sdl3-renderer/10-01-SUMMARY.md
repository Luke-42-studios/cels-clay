---
phase: 10-sdl3-renderer
plan: 01
subsystem: rendering
tags: [sdl3, sdl3-ttf, renderer, graphical, gpu, clay]

# Dependency graph
requires:
  - phase: 09-ncurses-migration
    provides: Renderer module pattern (CEL_Module with text measurement + render system registration), CMakeLists.txt conditional compilation structure
  - phase: 06-clay-engine-module
    provides: Clay_Engine CEL_Module with ClayRenderableData singleton pattern
provides:
  - CEL_Module(Clay_SDL3) graphical renderer for Clay render commands
  - ClaySDL3Config struct with font_path/font_size configuration
  - Clay_SDL3_configure() pre-registration configuration function
  - Full render command dispatch (RECTANGLE, TEXT, BORDER, SCISSOR, IMAGE)
  - CMake conditional compilation when cels-sdl3 target available
affects: [11-dual-renderer-example, future SDL3 apps using cels-clay]

# Tech tracking
tech-stack:
  added: [SDL3 (via cels-sdl3), SDL3_ttf (TTF_CreateText/TTF_DrawRendererText text engine API)]
  patterns: [lazy renderer initialization from window component, scissor intersection stack, TTF text engine pattern]

key-files:
  created:
    - include/cels-clay/clay_sdl3_renderer.h
    - src/clay_sdl3_renderer.c
  modified:
    - CMakeLists.txt

key-decisions:
  - "TTF text engine API (TTF_CreateText + TTF_DrawRendererText) instead of surface->texture pipeline -- follows Clay reference renderer, more efficient with internal glyph caching"
  - "Lazy SDL_Renderer creation from SDL3_WindowComponent -- window may not exist at module init time, renderer created on first render frame"
  - "Borders rendered as SDL_RenderFillRect per side -- matches Clay reference renderer pattern, proper thickness support"
  - "Corner radius deferred to v2 -- requires SDL_RenderGeometry triangle fan tessellation"
  - "cels_iter_t* callback signature -- matches current CELS v0.4 public API"
  - "Font size set per text element via TTF_SetFontSize -- supports Clay multi-size text"

patterns-established:
  - "SDL3 renderer module: CEL_Module(Clay_SDL3, init) registers text measurement + render system, mirrors NCurses pattern"
  - "Lazy initialization: ensure_renderer_initialized() queries for SDL3_WindowComponent, creates SDL_Renderer on first call"
  - "Scissor intersection stack: nested clip regions via manual rect intersection + SDL_SetRenderClipRect"

# Metrics
duration: 4min
completed: 2026-03-15
---

# Phase 10 Plan 01: SDL3 Renderer Summary

**SDL3 graphical renderer via CEL_Module(Clay_SDL3) with TTF text engine API, lazy renderer init from window component, and scissor intersection stack for nested clipping**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-16T03:15:37Z
- **Completed:** 2026-03-16T03:19:07Z
- **Tasks:** 3/3
- **Files created:** 2
- **Files modified:** 1

## Accomplishments
- CEL_Module(Clay_SDL3) translates full Clay render command array to SDL3 draw calls each frame
- All 6 Clay command types handled: RECTANGLE (SDL_RenderFillRect), TEXT (TTF text engine), BORDER (per-side filled rects), SCISSOR_START/END (intersection stack), IMAGE (SDL_RenderTexture)
- Clean public API header with no SDL3 header dependencies (pure C config struct)
- CMake conditional compilation mirrors NCurses block exactly

## Task Commits

Each task was committed atomically:

1. **Task 1: Create clay_sdl3_renderer.h** - `923adbe` (feat)
2. **Task 2: Create clay_sdl3_renderer.c** - `8161a21` (feat)
3. **Task 3: Update CMakeLists.txt** - `db2dc85` (chore)

## Files Created/Modified
- `include/cels-clay/clay_sdl3_renderer.h` - CEL_Module(Clay_SDL3) declaration, ClaySDL3Config struct, Clay_SDL3_configure function
- `src/clay_sdl3_renderer.c` - Full renderer: module init, lazy SDL_Renderer creation, TTF text measurement, render system with all 6 command types, scissor intersection stack
- `CMakeLists.txt` - Filled if(TARGET cels-sdl3) block with renderer source and link

## Decisions Made
- **TTF text engine API over surface->texture pipeline:** Used TTF_CreateText + TTF_DrawRendererText following the Clay reference SDL3 renderer. More efficient due to internal glyph caching by SDL3_ttf.
- **Lazy renderer initialization:** SDL_Renderer created in ensure_renderer_initialized() on first render frame, not at module init time. The SDL3_WindowComponent may not exist yet when CEL_Module(Clay_SDL3, init) runs.
- **Borders as filled rects per side:** Matches Clay reference renderer pattern. Each border side is an SDL_FRect with proper width/thickness from Clay_BorderRenderData.
- **Corner radius deferred to v2:** Would require SDL_RenderGeometry triangle fan tessellation (see Clay reference renderer SDL_Clay_RenderFillRoundedRect). Not worth the complexity for v1.
- **cels_iter_t* callback signature:** Used the current CELS v0.4 public API type for the system callback, which is the correct type for cels_system_declare.
- **Font size per text element:** TTF_SetFontSize called before each text measurement/render, supporting Clay's per-element fontSize in Clay_TextElementConfig.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added SDL_SetRenderDrawBlendMode for alpha support**
- **Found during:** Task 2 (render callback implementation)
- **Issue:** Plan did not specify blend mode setup. Without SDL_BLENDMODE_BLEND, semi-transparent colors would render incorrectly (alpha ignored).
- **Fix:** Added SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND) in ensure_renderer_initialized() and per rectangle render call.
- **Files modified:** src/clay_sdl3_renderer.c
- **Verification:** Blend mode set matches Clay reference renderer behavior.
- **Committed in:** 8161a21 (Task 2 commit)

**2. [Rule 1 - Bug] Used TTF text engine API instead of surface->texture pipeline**
- **Found during:** Task 2 (text rendering implementation)
- **Issue:** Plan specified TTF_RenderText_Blended + SDL_CreateTextureFromSurface pipeline, but the Clay reference SDL3 renderer uses TTF_CreateText + TTF_DrawRendererText (text engine API). The surface pipeline is older SDL2-era approach.
- **Fix:** Used TTF_CreateRendererTextEngine + TTF_CreateText + TTF_DrawRendererText pattern matching the Clay reference renderer.
- **Files modified:** src/clay_sdl3_renderer.c
- **Verification:** Text rendering follows identical pattern to Clay's own SDL3 renderer.
- **Committed in:** 8161a21 (Task 2 commit)

**3. [Rule 1 - Bug] Used SDL_RenderFillRect for borders instead of SDL_RenderLine**
- **Found during:** Task 2 (border rendering implementation)
- **Issue:** Plan specified SDL_RenderLine for borders, but SDL_RenderLine draws 1px lines regardless of border width. Clay reference renderer uses SDL_RenderFillRect per side with proper thickness.
- **Fix:** Used SDL_RenderFillRect per side with width/height from Clay_BorderRenderData.width, matching the Clay reference renderer.
- **Files modified:** src/clay_sdl3_renderer.c
- **Verification:** Borders render with correct thickness matching Clay's border width values.
- **Committed in:** 8161a21 (Task 2 commit)

---

**Total deviations:** 3 auto-fixed (2 bug fixes, 1 missing critical)
**Impact on plan:** All auto-fixes improve correctness by aligning with Clay's own SDL3 reference renderer patterns. No scope creep.

## Issues Encountered
None - implementation followed NCurses renderer structure with SDL3 API substitutions. Clay reference renderer provided clear API patterns for SDL3_ttf text engine and border rendering.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- SDL3 renderer complete and ready for Phase 11 (dual renderer example)
- Both NCurses and SDL3 renderers follow identical module pattern (CEL_Module with text measurement + render system)
- Module selection is via cels_register: `cels_register(Clay_NCurses)` vs `cels_register(Clay_SDL3)`
- Consumer links both cels-ncurses and cels-sdl3 targets, CMake conditionally includes both renderers

---
*Phase: 10-sdl3-renderer*
*Completed: 2026-03-15*
