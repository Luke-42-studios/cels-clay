---
phase: 04-ncurses-clay-renderer
verified: 2026-02-08T21:10:00Z
status: passed
score: 5/5 must-haves verified
---

# Phase 4: ncurses Clay Renderer Verification Report

**Phase Goal:** Clay render commands produce visible terminal output -- rectangles, text, borders, and colors rendered correctly in ncurses
**Verified:** 2026-02-08T21:10:00Z
**Status:** passed
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Clay rectangle commands render as filled cells with correct background color in the terminal | VERIFIED | `render_rectangle()` at line 106-121 of `clay_ncurses_renderer.c` calls `tui_draw_fill_rect(ctx, cell_rect, ' ', style)` with bg color from `tui_color_rgb(r, g, b)`. Signature matches `tui_draw.h:92`. Alpha-as-dim support present (line 116-118). |
| 2 | Clay text commands render via mvaddnstr with correct positioning, handling non-null-terminated StringSlice | VERIFIED | `render_text()` at line 132-157 copies StringSlice to stack buffer (512 bytes, malloc fallback), null-terminates, calls `tui_draw_text(ctx, rect.x, rect.y, buf, style)`. Signature matches `tui_draw.h:112`. Guard for null/empty text at line 135. |
| 3 | Borders render with Unicode box-drawing characters (single-line with corners and edges) | VERIFIED | `render_border()` at line 172-191 builds per-side mask from Clay_BorderRenderData.width, calls `tui_draw_border(ctx, rect, sides, TUI_BORDER_SINGLE, style)`. TUI_BORDER_SINGLE uses WACS single-line box-drawing chars. Theme default has matching Unicode characters. |
| 4 | Scissor/clipping restricts rendering to the scissor rect (content outside bounds is not drawn) | VERIFIED | `clay_ncurses_render()` switch at lines 241-249: SCISSOR_START calls `tui_push_scissor(&ctx, cell_rect)`, SCISSOR_END calls `tui_pop_scissor(&ctx)`. Scissor reset at frame start (line 218). All three functions confirmed in `tui_scissor.h:52-54`. |
| 5 | RGBA colors from Clay map to ncurses 8/16 color pairs via nearest-match lookup | VERIFIED | `tui_color_rgb(r, g, b)` called in `render_rectangle()` (line 112 for bg) and `render_text()` (line 149 for fg). `tui_color_rgb` declared in `tui_color.h:84` wraps xterm-256 nearest-match. Alpha < 128 maps to TUI_ATTR_DIM (line 116-118). |

**Score:** 5/5 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `modules/cels-clay/include/cels-clay/clay_ncurses_renderer.h` | ClayNcursesTheme struct, default theme, init/set_theme/scroll declarations | VERIFIED (150 lines) | Theme struct with border/scrollbar/aspect-ratio/alpha fields. CLAY_NCURSES_THEME_DEFAULT with UTF-8 box-drawing chars. Three function declarations. No ncurses/cels-ncurses includes in public header. |
| `modules/cels-clay/src/clay_ncurses_renderer.c` | Provider callback, all 5 command handlers, text measurement, coordinate mapping, scroll handler | VERIFIED (439 lines) | All 5 Clay command types handled (RECTANGLE, TEXT, BORDER, SCISSOR_START, SCISSOR_END). Dual bbox converters. wcwidth text measurement. _CEL_Provides registration. Scroll input handler with Vim bindings. |
| `modules/cels-clay/CMakeLists.txt` (modified) | Conditional compilation with if(TARGET cels-ncurses) guard | VERIFIED (75 lines) | Lines 67-74: `if(TARGET cels-ncurses)` guard adds renderer source and links cels-ncurses. CMake configures successfully. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `clay_ncurses_renderer.c` | ClayRenderable Feature | `_CEL_Provides(NCurses, ClayRenderable, ClayRenderableData, clay_ncurses_render)` | WIRED | Line 355 of renderer. ClayRenderableData type imported from `clay_render.h`. Provider callback signature matches ecs_iter_t pattern. |
| `clay_ncurses_renderer.c` | cels-ncurses drawing API | `tui_draw_fill_rect`, `tui_draw_text`, `tui_draw_border`, `tui_push_scissor`, `tui_pop_scissor` | WIRED | All 5 drawing functions called with correct signatures verified against `tui_draw.h`, `tui_scissor.h` headers. Also uses `tui_frame_get_background()`, `tui_layer_get_draw_context()`, `tui_scissor_reset()`, `tui_color_rgb()`. |
| `clay_ncurses_renderer.c` | Clay text measurement | `Clay_SetMeasureTextFunction(clay_ncurses_measure_text, NULL)` | WIRED | Line 352 registers callback. Callback at lines 270-332 uses wcwidth for accurate cell-column measurement. |
| `clay_ncurses_renderer.c` | Clay scroll containers | `Clay_UpdateScrollContainers(false, scroll_delta, delta_time)` | WIRED | Called at lines 380 and 438. Scroll delta computed from CELS_Input raw_key (Vim bindings), key_page_up/down, and axis_left. |
| `CMakeLists.txt` | cels-ncurses target | `if(TARGET cels-ncurses)` conditional + `target_link_libraries` | WIRED | Lines 67-74 conditionally compile renderer and link cels-ncurses. CMake configure verified -- no errors. |

### Requirements Coverage

| Requirement | Status | Blocking Issue |
|-------------|--------|----------------|
| REND-01: Rectangle rendering | SATISFIED | `render_rectangle()` fills cells with `tui_draw_fill_rect` |
| REND-02: Text rendering via mvaddnstr | SATISFIED | `render_text()` with StringSlice null-termination, `tui_draw_text` |
| REND-03: Border rendering with Unicode box-drawing | SATISFIED | `render_border()` with `TUI_BORDER_SINGLE` matching default theme chars |
| REND-04: Scissor/clipping support | SATISFIED | `tui_push_scissor`/`tui_pop_scissor` in command switch, `tui_scissor_reset` at frame start |
| REND-05: Float-to-cell coordinate mapping | SATISFIED | Dual converters: `clay_bbox_to_cells` (aspect-scaled) and `clay_text_bbox_to_cells` (text-accurate) |
| REND-06: Color mapping RGBA to ncurses | SATISFIED | `tui_color_rgb(r, g, b)` for fg/bg, alpha < 128 maps to TUI_ATTR_DIM |
| REND-07: Wide character text measurement | SATISFIED | `clay_ncurses_measure_text` uses mbstowcs + wcwidth with newline/multi-line support |
| REND-08: Scroll container keyboard navigation | SATISFIED | `clay_ncurses_handle_scroll_input` with j/k/Ctrl-U/Ctrl-D/G/gg/PageUp/PageDown/arrows |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (none) | - | - | - | - |

No anti-patterns detected:
- No TODO/FIXME/XXX/HACK/placeholder comments in either source file
- No wrefresh/wnoutrefresh/doupdate/init_pair direct ncurses calls (only mentioned in header comment as anti-pattern documentation)
- No empty implementations or return null patterns
- No console.log or debug-only code
- Stack buffer pattern has proper malloc fallback for long strings

### Human Verification Required

### 1. Visual Rectangle Rendering

**Test:** Run a demo app that creates a Clay element with a colored background (e.g., red). Verify the terminal shows a filled rectangle region with the correct color.
**Expected:** A rectangular area filled with red background color at the correct position and dimensions.
**Why human:** Verifying visual correctness of terminal color output requires seeing the actual terminal. Automated checks confirm API calls are correct but cannot confirm the visual result.

### 2. Text Positioning and CJK/Wide Characters

**Test:** Render a Clay layout with both ASCII and CJK/emoji text. Verify text aligns with element boundaries and wide characters occupy correct column widths.
**Expected:** Text renders at the correct position within its containing element. CJK characters occupy 2 columns each.
**Why human:** wcwidth accuracy and visual text alignment in actual terminal rendering cannot be verified structurally.

### 3. Border Box-Drawing Characters

**Test:** Render a Clay element with all-sides border. Verify single-line Unicode box-drawing characters appear as corners and edges forming a complete box.
**Expected:** A box with U+250C/U+2510/U+2514/U+2518 corners and U+2500/U+2502 horizontal/vertical lines.
**Why human:** Whether the terminal renders the correct Unicode glyphs depends on font and locale, which requires visual inspection.

### 4. Scissor Clipping

**Test:** Create a Clay layout where content overflows a container with scissor/clipping. Verify overflow content is not visible.
**Expected:** Content extending beyond the scissor rect boundary is clipped and not drawn.
**Why human:** Clipping correctness requires visual confirmation that nothing bleeds outside bounds.

### 5. Aspect Ratio Compensation

**Test:** Create a Clay layout intended to produce a square element (equal width and height). Verify it appears approximately square in the terminal (accounting for the 2:1 cell aspect ratio compensation).
**Expected:** The element appears roughly proportional despite terminal cells being rectangular.
**Why human:** Proportional appearance is a visual judgment.

### Gaps Summary

No gaps found. All 5 observable truths are verified through structural code analysis. All 8 REND requirements (REND-01 through REND-08) have substantive implementations with correct API wiring to cels-ncurses drawing primitives. The CMake conditional compilation is correctly guarded. No anti-patterns or stub code detected.

The phase delivers:
- A 439-line renderer source implementing all 5 Clay render command types
- A 150-line public header with configurable theme struct and sensible defaults
- Dual coordinate mapping (aspect-ratio-scaled for geometry, cell-accurate for text)
- wcwidth-based text measurement for CJK/emoji correctness
- Vim-style keyboard scroll handler for scroll containers
- Conditional CMake integration that activates only when cels-ncurses is available

Visual verification of actual terminal output requires the Phase 5 demo app, which is the designed next step in the roadmap.

---

_Verified: 2026-02-08T21:10:00Z_
_Verifier: Claude (gsd-verifier)_
