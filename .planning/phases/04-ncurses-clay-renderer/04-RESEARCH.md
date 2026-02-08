# Phase 4: ncurses Clay Renderer - Research

**Researched:** 2026-02-08
**Domain:** Clay render command dispatch to ncurses terminal output
**Confidence:** HIGH

## Summary

This phase translates Clay's `Clay_RenderCommandArray` into visible terminal output using the existing cels-ncurses drawing infrastructure. The research examines six domains: the Clay render command format (what data the renderer receives), the cels-ncurses drawing primitives (what tools already exist), the Feature/Provider integration pattern (how the renderer registers), color mapping (RGBA to 256-color), coordinate mapping (float to cell), and scroll container navigation (keyboard to Clay_UpdateScrollContainers).

The key architectural insight is that cels-ncurses already provides nearly all needed primitives -- `tui_draw_fill_rect`, `tui_draw_text`, `tui_draw_border`, `tui_push_scissor`/`tui_pop_scissor`, `tui_color_rgb`, and `tui_style_apply`. The renderer's primary job is to iterate Clay_RenderCommandArray, convert each command's bounding box and data into the corresponding cels-ncurses API calls. The theme system and text measurement callback are the only genuinely new code.

**Primary recommendation:** Build the renderer as a thin translation layer that maps Clay render commands to existing cels-ncurses drawing primitives. Invest design effort in the theme struct (border characters, color overrides) and the text measurement callback (wcwidth-based). Do not re-implement rectangle filling, text rendering, or scissor clipping -- use cels-ncurses directly.

## Standard Stack

### Core (already available -- no new dependencies)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Clay | v0.14 | Layout engine, produces `Clay_RenderCommandArray` | Already fetched via FetchContent in Phase 1 |
| ncursesw | 6.5 | Terminal drawing, color pairs, wide chars | System package, already linked by cels-ncurses |
| cels-ncurses | (in-repo) | Drawing primitives, layers, scissor, color | Sibling module with full TUI infrastructure |
| cels-clay | (in-repo) | ClayRenderable Feature, render bridge | Phase 3 output -- provides `ClayRenderableData` |

### Supporting

| Library | Purpose | When to Use |
|---------|---------|-------------|
| `<wchar.h>` (wcwidth/mbstowcs) | Wide character measurement for CJK text | Text measurement callback (REND-07) |
| `<math.h>` (roundf, fmaxf) | Float-to-cell coordinate rounding | Coordinate mapping (REND-05) |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| cels-ncurses `tui_draw_*` | Direct ncurses calls (mvwaddch, etc.) | Bypasses clipping, color pair management, UTF-8 handling. Use cels-ncurses primitives. |
| `alloc_pair()` for color pairs | `init_pair()` with manual tracking | alloc_pair handles LRU eviction automatically, already used by tui_color.c |
| `tui_color_rgb()` for nearest-match | Custom RGB-to-256 lookup table | tui_color.c already implements Euclidean distance matching against xterm-256 cube + greyscale ramp |

**No new dependencies required.** The renderer only needs headers from cels-ncurses and cels-clay.

## Architecture Patterns

### Where the Renderer Lives

The ncurses Clay renderer is a **provider** for the `ClayRenderable` feature. It lives in the **application** or in a **bridge module** -- NOT inside cels-clay (which must not depend on cels-ncurses). The ROADMAP groups it under cels-clay Phase 4 for planning purposes, but the actual source files can live in cels-clay as long as they are conditionally compiled or structured to avoid hard ncurses dependency.

**Recommended approach:** Create new source files within cels-clay that implement the renderer, guarded by a build option. The renderer includes cels-ncurses headers and registers as a provider.

```
modules/cels-clay/
  include/cels-clay/
    clay_ncurses_renderer.h    # Public API: theme struct, init function
  src/
    clay_ncurses_renderer.c    # Provider implementation
```

The CMakeLists.txt conditionally adds these files when cels-ncurses is available as a target.

### Renderer Data Flow

```
Clay_RenderCommandArray (from Phase 3 render bridge)
       |
       v
ClayRenderableData singleton entity (OnStore phase)
       |
       v
_CEL_Provides(NCurses, ClayRenderable, ClayRenderableData, ncurses_renderer_callback)
       |
       v
ncurses_renderer_callback:
  1. Get TUI_DrawContext from background layer
  2. Reset scissor stack
  3. For each Clay_RenderCommand in array:
     - Convert bounding box: float -> cell coords (roundf, min 1)
     - Switch on commandType:
       RECTANGLE -> tui_draw_fill_rect()
       TEXT      -> tui_draw_text() with StringSlice handling
       BORDER    -> tui_draw_border() using theme chars
       SCISSOR_START -> tui_push_scissor()
       SCISSOR_END   -> tui_pop_scissor()
  4. (No wrefresh/doupdate -- frame pipeline handles compositing)
```

### ECS Phase Ordering

```
OnLoad:     TUI_InputSystem (reads getch, handles KEY_RESIZE)
PreStore:   TUI_FrameBeginSystem (clears dirty layers)
PreStore:   ClayLayoutSystem (BeginLayout -> walk -> EndLayout)
OnStore:    ClayRenderDispatch (copies commands to singleton)
OnStore:    [NCurses provider] (draws commands to layer)   <-- NEW
PostFrame:  TUI_FrameEndSystem (update_panels + doupdate)
```

The provider callback runs at OnStore (the phase specified by `_CEL_DefineFeature(ClayRenderable, .phase = CELS_Phase_OnStore)`), after `ClayRenderDispatch` has updated the singleton. Frame begin (PreStore) clears dirty layers before rendering. Frame end (PostFrame) composites after rendering. This ordering is already correct -- no system registration tricks needed.

### Theme Pattern

```c
typedef struct ClayNcursesTheme {
    // Border characters (6 per set: hline, vline, ul, ur, ll, lr)
    struct {
        const char* hline;     // "─" (U+2500)
        const char* vline;     // "│" (U+2502)
        const char* ul;        // "┌" (U+250C)
        const char* ur;        // "┐" (U+2510)
        const char* ll;        // "└" (U+2514)
        const char* lr;        // "┘" (U+2518)
    } border;

    // Scrollbar characters
    struct {
        const char* track;     // "│" or "┃"
        const char* thumb;     // "█" or "▓"
    } scrollbar;

    // Aspect ratio compensation
    float cell_aspect_ratio;   // default 2.0 (cells are ~2x taller than wide)

    // Alpha handling
    bool alpha_as_dim;         // true = alpha < 128 uses A_DIM attribute
} ClayNcursesTheme;

// Default theme
static const ClayNcursesTheme CLAY_NCURSES_THEME_DEFAULT = {
    .border = {
        .hline = "\xe2\x94\x80",  // ─
        .vline = "\xe2\x94\x82",  // │
        .ul    = "\xe2\x94\x8c",  // ┌
        .ur    = "\xe2\x94\x90",  // ┐
        .ll    = "\xe2\x94\x94",  // └
        .lr    = "\xe2\x94\x98",  // ┘
    },
    .scrollbar = {
        .track = "\xe2\x94\x82",  // │
        .thumb = "\xe2\x96\x88",  // █
    },
    .cell_aspect_ratio = 2.0f,
    .alpha_as_dim = true,
};
```

**Why UTF-8 strings (not cchar_t):** The theme is defined by the developer at compile time. UTF-8 string literals are portable and simple. Convert to cchar_t at renderer init time (via `setcchar`) for use with `mvwadd_wch()`. This matches the rounded-corner pattern in `tui_draw.c` (lines 35-54).

### Anti-Patterns to Avoid

- **Do NOT create a separate WINDOW for Clay rendering.** Use the existing background layer from `tui_frame_get_background()`. Creating a new window would fight with the panel compositing system.
- **Do NOT call wrefresh/wnoutrefresh/doupdate from the renderer.** The frame pipeline (TUI_FrameEndSystem at PostFrame) handles this. Adding refresh calls causes double-buffer corruption and flickering.
- **Do NOT manually track color pair numbers.** Use `tui_color_rgb()` + `tui_style_apply()` which call `alloc_pair()` internally.
- **Do NOT re-implement scissor clipping.** Use `tui_push_scissor()` / `tui_pop_scissor()` which manages the stack and updates `ctx->clip`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| RGB to xterm-256 color mapping | Custom lookup table | `tui_color_rgb(r, g, b)` | Already implements Euclidean distance matching against 256-color cube + greyscale ramp |
| Color pair allocation | Manual init_pair numbering | `tui_style_apply()` -> `alloc_pair()` | Handles LRU eviction, dedup, default pair |
| Filled rectangle drawing | Custom cell-by-cell loop | `tui_draw_fill_rect()` | Handles clipping intersection, style application |
| Text rendering with UTF-8 | mbstowcs + manual clipping | `tui_draw_text()` | Handles wcwidth, clip-at-boundary, wide char skip |
| Bounded text rendering | Temporary clip manipulation | `tui_draw_text_bounded()` | Saves/restores clip, delegates to tui_draw_text |
| Scissor stack management | Manual rect stack | `tui_push_scissor()` / `tui_pop_scissor()` | 16-deep stack with intersection, ctx->clip sync |
| Box-drawing border rendering | Custom corner logic | `tui_draw_border()` with per-side mask | Handles corner adjacency, 3 border styles |
| Float-to-cell conversion | Manual (int) casts | `tui_rect_to_cells()` | floorf for position, ceilf for dimensions |
| Rect intersection test | Manual min/max arithmetic | `tui_cell_rect_intersect()` | Handles zero-area, negative coords |
| Terminal resize handling | Custom SIGWINCH handler | KEY_RESIZE in tui_input.c | Already resizes layers, invalidates frames, notifies state observers |

**Key insight:** The cels-ncurses module was designed as a complete TUI drawing API. The Clay renderer should be a ~200-300 line translation layer, not a second drawing system.

## Common Pitfalls

### Pitfall 1: Clay_StringSlice is NOT null-terminated

**What goes wrong:** Passing `text.chars` directly to `printf("%s")`, `mvwaddstr()`, or `mbstowcs()` reads past the string slice into garbage memory.
**Why it happens:** Clay_StringSlice has `.chars` (pointer) and `.length` (int32_t). The pointer is into Clay's arena and is NOT null-terminated. Multiple text slices may be adjacent in memory.
**How to avoid:** Always use the length-bounded variant: `mvwaddnstr(win, y, x, text.chars, text.length)` for narrow strings, or copy to a null-terminated buffer before calling `mbstowcs()` for wide character conversion. The existing `tui_draw_text()` takes a `const char*` (null-terminated) -- the renderer must null-terminate before calling it, or use a Clay-specific text path.
**Warning signs:** Text rendering shows garbage characters at the end of strings.

### Pitfall 2: Aspect ratio compensation applied inconsistently

**What goes wrong:** Horizontal padding/margins look disproportionate because terminal cells are typically 2x taller than wide, but only some dimensions are scaled.
**Why it happens:** Clay computes layout in uniform float units. If you pass raw terminal dimensions (cols x rows) to Clay and render 1:1, a "square" Clay element (50x50) renders as a tall rectangle because each cell is taller than it is wide.
**How to avoid:** The decided approach is: pass raw cols x rows to Clay as layout dimensions, and apply aspect ratio compensation during coordinate mapping at render time. Scale horizontal bounding box values by `cell_aspect_ratio` (default 2.0). Apply to ALL horizontal values: x, width, padding, margin. Apply consistently at a single point (the coordinate conversion function).
**Warning signs:** Elements look vertically stretched; circles look like ovals; padding looks uneven.

**Important nuance from CONTEXT.md:** The context says "Raw terminal cols x rows passed to Clay as layout dimensions" AND "aspect ratio compensation enabled." This means the text measurement callback must also account for aspect ratio -- a character is 1 cell wide but should report width as `1.0 * aspect_ratio` to Clay so layout proportions are correct. Or alternatively, DO NOT scale the text measurement and only scale the non-text bounding boxes at render time. This needs careful design in Plan 04-01.

### Pitfall 3: Border rendering ignores theme (uses Clay color data)

**What goes wrong:** Borders render with Clay's native color data instead of the theme's border colors and character sets. Different border elements (single-line, double-line, rounded) all look the same.
**Why it happens:** The natural approach is to read `Clay_BorderRenderData.color` and use it directly. But the CONTEXT decision says "Border color comes from theme, not Clay's border config" and "Theme overrides Clay's native border/color data."
**How to avoid:** The renderer must ignore `Clay_BorderRenderData.color` and instead use the theme's border style (characters) and color. Clay handles the layout math (border width, bounding box), but the visual appearance comes entirely from the theme.
**Warning signs:** All borders look identical; border styles can't be customized per-surface.

### Pitfall 4: Scissor start/end without proper stack management

**What goes wrong:** Nested scroll containers clip incorrectly because scissor regions don't intersect properly, or a scissor end restores the wrong parent region.
**Why it happens:** Clay emits `SCISSOR_START` and `SCISSOR_END` as flat commands in the render array. Without a stack, each SCISSOR_START replaces the current clip instead of intersecting with it.
**How to avoid:** Use `tui_push_scissor()` on SCISSOR_START (which intersects with parent) and `tui_pop_scissor()` on SCISSOR_END. The existing scissor stack (16 deep) handles this correctly. Reset the stack at the start of each frame via `tui_scissor_reset()`.
**Warning signs:** Content from nested scroll containers bleeds through parent boundaries.

### Pitfall 5: Frame pipeline race -- rendering into un-erased layer

**What goes wrong:** Stale content from the previous frame shows through because the renderer draws before the frame pipeline clears the layer.
**Why it happens:** If the renderer runs before `TUI_FrameBeginSystem`, the layer hasn't been erased yet.
**How to avoid:** The provider runs at OnStore phase, TUI_FrameBeginSystem runs at PreStore. PreStore < OnStore, so frame begin always runs first. However, the layer must be marked dirty for the frame pipeline to erase it. The renderer should call `tui_layer_get_draw_context()` which auto-marks the layer dirty, ensuring next frame's frame_begin will erase it.
**Warning signs:** Ghost images of previous frame content visible beneath current frame.

### Pitfall 6: CELS_Input raw_key doesn't include Vim-style multi-key sequences

**What goes wrong:** Vim-style scroll navigation (Ctrl-U, Ctrl-D, gg, G) requires sequences or control codes that may not map cleanly to CELS_Input fields.
**Why it happens:** CELS_Input has `raw_key` but Ctrl-U is character 21 (NAK), Ctrl-D is character 4 (EOT). These are raw key codes, not semantic actions. The `gg` sequence requires tracking the previous key across frames.
**How to avoid:** For single control keys (Ctrl-U = 21, Ctrl-D = 4), read from `input->raw_key`. For multi-key sequences (gg), implement a small state machine in the scroll input system that tracks the previous key. For j/k, use `input->raw_key`. Keep the scroll system simple -- it reads raw keys and calls `Clay_UpdateScrollContainers()` with the appropriate scroll delta.
**Warning signs:** Vim-style navigation doesn't work or responds to wrong keys.

## Code Examples

### Renderer Provider Registration (from CELS Feature/Provider pattern)

```c
// Source: cels-clay/src/clay_render.h pattern + cels/cels.h _CEL_Provides macro

// In the renderer init function:
void clay_ncurses_renderer_init(ClayNcursesTheme* theme) {
    // Store theme pointer for use in callback
    g_theme = theme ? theme : &CLAY_NCURSES_THEME_DEFAULT;

    // Register as provider for ClayRenderable feature
    _CEL_Provides(NCurses, ClayRenderable, ClayRenderableData, clay_ncurses_render);
}
```

### Render Command Iteration (core loop)

```c
// Source: Clay clay.h render command types + cels-ncurses drawing API

static void clay_ncurses_render(ecs_iter_t* it) {
    ecs_world_t* world = it->world;

    // Get singleton render data
    for (int i = 0; i < it->count; i++) {
        ClayRenderableData* data = ecs_field(it, ClayRenderableData, 0);
        if (!data->dirty) continue;

        // Get draw context from background layer
        TUI_Layer* layer = tui_frame_get_background();
        TUI_DrawContext ctx = tui_layer_get_draw_context(layer);
        tui_scissor_reset(&ctx);

        Clay_RenderCommandArray cmds = data->render_commands;
        for (int32_t j = 0; j < cmds.length; j++) {
            Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&cmds, j);
            TUI_CellRect cell_rect = clay_bbox_to_cells(cmd->boundingBox);

            switch (cmd->commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                    render_rectangle(&ctx, cell_rect, &cmd->renderData.rectangle);
                    break;
                case CLAY_RENDER_COMMAND_TYPE_TEXT:
                    render_text(&ctx, cell_rect, &cmd->renderData.text);
                    break;
                case CLAY_RENDER_COMMAND_TYPE_BORDER:
                    render_border(&ctx, cell_rect, &cmd->renderData.border);
                    break;
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                    tui_push_scissor(&ctx, cell_rect);
                    break;
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                    tui_pop_scissor(&ctx);
                    break;
                default:
                    break;  // IMAGE, CUSTOM, NONE -- skip silently
            }
        }
    }
}
```

### Float-to-Cell Coordinate Mapping with Aspect Ratio

```c
// Source: tui_types.h pattern + CONTEXT.md aspect ratio decision

static TUI_CellRect clay_bbox_to_cells(Clay_BoundingBox bbox) {
    float ar = g_theme->cell_aspect_ratio;

    // Scale horizontal values by aspect ratio
    float x = bbox.x * ar;
    float w = bbox.width * ar;

    // Round to nearest cell (not floor/ceil -- CONTEXT.md decision)
    int cx = (int)roundf(x);
    int cy = (int)roundf(bbox.y);
    int cw = (int)roundf(w);
    int ch = (int)roundf(bbox.height);

    // Minimum 1 cell for non-zero dimensions
    if (bbox.width > 0 && cw < 1) cw = 1;
    if (bbox.height > 0 && ch < 1) ch = 1;

    return (TUI_CellRect){ .x = cx, .y = cy, .w = cw, .h = ch };
}
```

### Rectangle Rendering

```c
// Source: tui_draw.c tui_draw_fill_rect + tui_color.c tui_color_rgb

static void render_rectangle(TUI_DrawContext* ctx, TUI_CellRect rect,
                              Clay_RectangleRenderData* data) {
    Clay_Color c = data->backgroundColor;

    TUI_Style style = {
        .fg = TUI_COLOR_DEFAULT,
        .bg = tui_color_rgb(c.r, c.g, c.b),
        .attrs = TUI_ATTR_NORMAL,
    };

    // Alpha handling: low alpha = dim
    if (g_theme->alpha_as_dim && c.a < 128) {
        style.attrs |= TUI_ATTR_DIM;
    }

    tui_draw_fill_rect(ctx, rect, ' ', style);
}
```

### Text Rendering (StringSlice to null-terminated)

```c
// Source: Clay_TextRenderData.stringContents is Clay_StringSlice (NOT null-terminated)

static void render_text(TUI_DrawContext* ctx, TUI_CellRect rect,
                         Clay_TextRenderData* data) {
    Clay_StringSlice text = data->stringContents;
    if (text.length <= 0 || text.chars == NULL) return;

    // Copy to null-terminated buffer (stack for typical, malloc for long)
    char buf_stack[512];
    char* buf = buf_stack;
    if (text.length >= (int32_t)sizeof(buf_stack)) {
        buf = malloc((size_t)text.length + 1);
        if (!buf) return;
    }
    memcpy(buf, text.chars, (size_t)text.length);
    buf[text.length] = '\0';

    Clay_Color c = data->textColor;
    TUI_Style style = {
        .fg = tui_color_rgb(c.r, c.g, c.b),
        .bg = TUI_COLOR_DEFAULT,
        .attrs = TUI_ATTR_NORMAL,
    };

    tui_draw_text(ctx, rect.x, rect.y, buf, style);

    if (buf != buf_stack) free(buf);
}
```

### Border Rendering with Theme

```c
// Source: tui_draw.c tui_draw_border + CONTEXT.md theme decision

static void render_border(TUI_DrawContext* ctx, TUI_CellRect rect,
                           Clay_BorderRenderData* data) {
    // Build per-side mask from Clay border widths
    uint8_t sides = 0;
    if (data->width.top > 0)    sides |= TUI_SIDE_TOP;
    if (data->width.right > 0)  sides |= TUI_SIDE_RIGHT;
    if (data->width.bottom > 0) sides |= TUI_SIDE_BOTTOM;
    if (data->width.left > 0)   sides |= TUI_SIDE_LEFT;

    if (sides == 0) return;

    // Use theme border style (not Clay color)
    // Note: need to render with theme's box-drawing characters
    // The cels-ncurses tui_draw_border uses TUI_BorderStyle enum,
    // but theme uses custom characters. May need a custom border draw
    // path that accepts cchar_t characters from the theme.
    // Alternatively, convert theme chars to TUI_BorderChars at init time.
    TUI_Style style = {
        .fg = tui_color_rgb(/* theme border color */),
        .bg = TUI_COLOR_DEFAULT,
        .attrs = TUI_ATTR_NORMAL,
    };

    tui_draw_border(ctx, rect, sides, TUI_BORDER_SINGLE, style);
}
```

### Text Measurement Callback (wcwidth-aware)

```c
// Source: Clay_SetMeasureTextFunction signature + wcwidth(3) POSIX

static Clay_Dimensions clay_ncurses_measure_text(
    Clay_StringSlice text,
    Clay_TextElementConfig* config,
    void* userData)
{
    (void)config;
    (void)userData;

    float max_width = 0;
    float line_width = 0;
    float height = 1;

    // Convert to wide chars for wcwidth measurement
    // Need null-terminated copy for mbstowcs
    char buf_stack[512];
    char* buf = buf_stack;
    if (text.length >= (int32_t)sizeof(buf_stack)) {
        buf = malloc((size_t)text.length + 1);
        if (!buf) return (Clay_Dimensions){0, 0};
    }
    memcpy(buf, text.chars, (size_t)text.length);
    buf[text.length] = '\0';

    wchar_t wbuf_stack[256];
    wchar_t* wbuf = wbuf_stack;
    size_t needed = mbstowcs(NULL, buf, 0);
    if (needed == (size_t)-1) {
        // Fallback: count bytes as columns
        if (buf != buf_stack) free(buf);
        return (Clay_Dimensions){ .width = (float)text.length, .height = 1 };
    }
    if (needed >= 256) {
        wbuf = malloc((needed + 1) * sizeof(wchar_t));
        if (!wbuf) {
            if (buf != buf_stack) free(buf);
            return (Clay_Dimensions){0, 0};
        }
    }
    mbstowcs(wbuf, buf, needed + 1);

    for (size_t i = 0; i < needed; i++) {
        if (wbuf[i] == L'\n') {
            if (line_width > max_width) max_width = line_width;
            line_width = 0;
            height++;
        } else {
            int cw = wcwidth(wbuf[i]);
            if (cw < 0) cw = 0;
            line_width += (float)cw;
        }
    }
    if (line_width > max_width) max_width = line_width;

    if (wbuf != wbuf_stack) free(wbuf);
    if (buf != buf_stack) free(buf);

    return (Clay_Dimensions){ .width = max_width, .height = height };
}
```

### Scroll Container Navigation

```c
// Source: Clay_UpdateScrollContainers API + CONTEXT.md Vim bindings

// Call before Clay_BeginLayout each frame
static void handle_scroll_input(const CELS_Input* input, float delta_time) {
    Clay_Vector2 scroll_delta = {0, 0};

    // j/k: single line
    if (input->has_raw_key) {
        if (input->raw_key == 'j') scroll_delta.y = 1.0f;
        if (input->raw_key == 'k') scroll_delta.y = -1.0f;
        // Ctrl-D (0x04): half page down
        if (input->raw_key == 4) scroll_delta.y = 12.0f;
        // Ctrl-U (0x15): half page up
        if (input->raw_key == 21) scroll_delta.y = -12.0f;
        // G: bottom (large scroll)
        if (input->raw_key == 'G') scroll_delta.y = 10000.0f;
    }

    // Arrow keys via axis
    if (input->axis_left[1] > 0.5f) scroll_delta.y = 1.0f;
    if (input->axis_left[1] < -0.5f) scroll_delta.y = -1.0f;

    // Page Up/Down
    if (input->key_page_up) scroll_delta.y = -12.0f;
    if (input->key_page_down) scroll_delta.y = 12.0f;

    Clay_UpdateScrollContainers(false, scroll_delta, delta_time);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `init_pair()` with manual pair tracking | `alloc_pair()` with automatic LRU | ncurses 6.1 (2018) | No need to pre-allocate or track color pairs |
| ANSI escape codes for terminal color | ncurses `init_color` + 256-color palette | Widespread since xterm-256color | Much richer color palette |
| `addstr` for text | `mvwaddnwstr` for wide chars | ncurses wide-char support | Correct CJK/emoji rendering |
| `wrefresh()` per window | `update_panels()` + `doupdate()` | Panel library | Single composite update, no flicker |
| Custom SIGWINCH handler | KEY_RESIZE from wgetch | ncurses built-in | Safe, no signal handler race conditions |

**Deprecated/outdated:**
- Manual `init_pair(pair_number, fg, bg)` tracking: replaced by `alloc_pair(fg, bg)` which auto-manages pairs
- Direct SIGWINCH signal handlers: ncurses internally converts SIGWINCH to KEY_RESIZE; the existing cels-ncurses input system already handles this (tui_input.c line 95-102)

## Integration Points with Existing Code

### What the renderer receives (from Phase 3)

`ClayRenderableData` singleton entity updated each frame at OnStore:
- `render_commands` -- `Clay_RenderCommandArray` with length + internalArray
- `layout_width`, `layout_height` -- current layout dimensions
- `frame_number`, `delta_time` -- frame metadata
- `dirty` -- true when `commands.length > 0`

### What the renderer uses from cels-ncurses

| cels-ncurses API | Used for | File |
|------------------|----------|------|
| `tui_frame_get_background()` | Get the WINDOW to draw into | tui_frame.h |
| `tui_layer_get_draw_context()` | Create TUI_DrawContext (auto-marks layer dirty) | tui_layer.h |
| `tui_scissor_reset()` | Reset clip stack per frame | tui_scissor.h |
| `tui_push_scissor()` / `tui_pop_scissor()` | Manage clip regions | tui_scissor.h |
| `tui_draw_fill_rect()` | Rectangle commands | tui_draw.h |
| `tui_draw_text()` | Text commands (null-terminated) | tui_draw.h |
| `tui_draw_text_bounded()` | Text with column limit | tui_draw.h |
| `tui_draw_border()` | Border commands with per-side mask | tui_draw.h |
| `tui_color_rgb()` | RGBA -> xterm-256 color mapping | tui_color.h |
| `tui_style_apply()` | Apply fg/bg/attrs to WINDOW | tui_color.h |
| `TUI_CellRect`, `tui_rect_to_cells()` | Coordinate conversion | tui_types.h |
| `tui_cell_rect_intersect()` | Geometry operations | tui_types.h |

### Key adaptation needed: tui_draw_text expects null-terminated strings

The existing `tui_draw_text()` signature is `(TUI_DrawContext* ctx, int x, int y, const char* text, TUI_Style style)` -- it takes `const char*` (null-terminated). Clay's `Clay_StringSlice` has `.chars` (not null-terminated) and `.length`. The renderer must null-terminate before passing to `tui_draw_text()`, or a new `tui_draw_text_n()` variant accepting a length parameter could be added to cels-ncurses.

**Recommendation:** Null-terminate in the renderer (stack buffer with malloc fallback, as shown in code examples). This avoids modifying cels-ncurses.

### Key adaptation needed: theme-based border rendering

The existing `tui_draw_border()` takes a `TUI_BorderStyle` enum (SINGLE, DOUBLE, ROUNDED). The theme system needs custom character sets. Two options:
1. Add a new `tui_draw_border_custom()` that takes `TUI_BorderChars` directly
2. Convert theme characters to `TUI_BorderChars` at init time and call the low-level path

**Recommendation:** Convert theme UTF-8 strings to `cchar_t` values at renderer init, store in a `TUI_BorderChars` struct, and use the existing `tui_border_chars_get()` pattern with a custom override. This may require adding a `TUI_BORDER_CUSTOM` style to the enum, or bypassing `tui_draw_border()` and calling `mvwadd_wch` directly (the per-cell loop is simple enough to inline).

## Open Questions

1. **Aspect ratio + text measurement interaction**
   - What we know: CONTEXT.md says to pass raw cols x rows to Clay and apply aspect ratio at render time. Text measurement returns character widths.
   - What's unclear: If text measurement reports width=10 for a 10-char string, but the renderer scales horizontal coords by 2x aspect ratio, the text bounding box at render time would be 20 cells wide -- too wide. The text measurement callback may need to report widths that are already aspect-ratio-adjusted, OR the renderer must not scale text bounding boxes.
   - Recommendation: Research during Plan 04-01. The likely solution is that the text measurement callback returns widths in the same unit system as Clay layout (unscaled), and the aspect ratio scaling happens uniformly at render time. Text characters occupy exactly their measured width in cells, so text bounding boxes should NOT be aspect-ratio-scaled. Only non-text bounding boxes (rectangles, borders) need scaling. This needs validation.

2. **gg (go-to-top) requires multi-key sequence tracking**
   - What we know: 'g' followed by 'g' means go-to-top. Single 'G' means go-to-bottom.
   - What's unclear: The CELS_Input system provides one key per frame. Tracking the previous key requires per-system state.
   - Recommendation: Implement a simple state variable `static int prev_key = 0` in the scroll system. If current key is 'g' and prev was 'g', emit go-to-top. Reset prev_key each frame after checking. Defer if complexity is unwanted in v1.

3. **Per-surface theme when multiple ClaySurfaces exist**
   - What we know: CONTEXT.md says "Theme scoped per ClaySurface, not global." The current architecture has a single ClayRenderableData singleton.
   - What's unclear: How to associate different themes with different surfaces when the render bridge produces a single combined command array.
   - Recommendation: For v1 (single ClaySurface), store a single global theme. The per-surface theme can be implemented later by adding a surface ID to each render command (via Clay's userData pointer) or by producing separate command arrays per surface.

## Sources

### Primary (HIGH confidence)
- Clay v0.14 source header: `/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-src/clay.h` -- render command types, StringSlice, BoundingBox, ScrollContainerData, UpdateScrollContainers API
- Clay terminal renderer: `/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-src/renderers/terminal/clay_renderer_terminal_ansi.c` -- reference terminal renderer implementation
- cels-ncurses source: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/` -- complete drawing API (color, draw, scissor, layer, frame)
- cels-clay Phase 3 source: `/home/cachy/workspaces/libs/cels/modules/cels-clay/src/clay_render.c` -- ClayRenderable Feature, render bridge
- cels-clay API Design: `/home/cachy/workspaces/libs/cels/modules/cels-clay/.planning/API-DESIGN.md` -- execution timeline, phase ordering

### Secondary (MEDIUM confidence)
- [ncurses alloc_pair man page](https://www.man7.org/linux/man-pages/man3/alloc_pair.3x.html) -- verified alloc_pair behavior, LRU eviction
- [ncurses resizeterm man page](https://man7.org/linux/man-pages/man3/resizeterm.3x.html) -- SIGWINCH -> KEY_RESIZE pattern
- [More than 256 color pairs in curses](https://reversed.top/2019-02-05/more-than-256-curses-color-pairs/) -- alloc_pair vs init_pair tradeoffs

### Tertiary (LOW confidence)
- None -- all findings verified against source code or official man pages

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all libraries are already in the project, versions confirmed from CMakeLists.txt and pkg-config
- Architecture: HIGH -- patterns directly observed in cels-ncurses and cels-clay source code, provider pattern verified
- Code examples: HIGH -- derived from reading actual source files, adapting existing patterns
- Pitfalls: HIGH -- identified from Clay StringSlice documentation and cels-ncurses API contracts
- Scroll navigation: MEDIUM -- Vim key codes confirmed but multi-key sequence tracking needs validation

**Research date:** 2026-02-08
**Valid until:** 2026-03-10 (stable -- Clay v0.14 pinned, ncurses 6.5 stable)
