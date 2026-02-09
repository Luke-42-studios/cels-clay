/*
 * Clay ncurses Renderer - Implementation
 *
 * Translates Clay_RenderCommandArray into terminal output using the
 * cels-ncurses drawing API. Registers as a _CEL_Provides backend for
 * the ClayRenderable feature defined in clay_render.c.
 *
 * Coordinate mapping:
 *   Clay computes layout in float units. The text measurement callback
 *   returns dimensions in terminal cell columns (via wcwidth). Non-text
 *   bounding boxes (rectangles, borders, scissors) are scaled horizontally
 *   by cell_aspect_ratio to compensate for terminal cells being taller
 *   than wide. Text bounding boxes are NOT aspect-ratio-scaled because
 *   the measurement callback already reports cell-accurate widths.
 *
 * Anti-patterns avoided (per RESEARCH.md):
 *   - No separate WINDOW creation (uses background layer)
 *   - No wrefresh/wnoutrefresh/doupdate (frame pipeline handles compositing)
 *   - No manual color pair tracking (uses tui_color_rgb + tui_style_apply)
 *   - No re-implemented scissor (uses tui_push_scissor/tui_pop_scissor)
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 *       It is conditionally included when the cels-ncurses target exists.
 */

#include "cels-clay/clay_ncurses_renderer.h"
#include "cels-clay/clay_render.h"
#include "clay.h"
#include <cels/cels.h>

#include <cels-ncurses/tui_draw.h>
#include <cels-ncurses/tui_color.h>
#include <cels-ncurses/tui_types.h>
#include <cels-ncurses/tui_scissor.h>
#include <cels-ncurses/tui_frame.h>
#include <cels-ncurses/tui_layer.h>

#include <cels-layout/types.h>   /* CEL_TextAttr */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wchar.h>

/* ============================================================================
 * Text Attribute Helpers
 * ============================================================================
 *
 * Decode CEL_TextAttr from a void* pointer (packed by w_pack_text_attr in
 * cels-widgets/style.h). Each bool occupies one bit of the pointer value.
 * Convert to TUI_ATTR_* bitmask for the ncurses style system.
 */

static inline CEL_TextAttr _unpack_text_attr(void* userData) {
    uintptr_t packed = (uintptr_t)userData;
    return (CEL_TextAttr){
        .bold      = (packed & 0x01) != 0,
        .dim       = (packed & 0x02) != 0,
        .underline = (packed & 0x04) != 0,
        .reverse   = (packed & 0x08) != 0,
        .italic    = (packed & 0x10) != 0,
    };
}

static inline uint32_t _text_attr_to_tui(CEL_TextAttr a) {
    uint32_t flags = TUI_ATTR_NORMAL;
    if (a.bold)      flags |= TUI_ATTR_BOLD;
    if (a.dim)       flags |= TUI_ATTR_DIM;
    if (a.underline) flags |= TUI_ATTR_UNDERLINE;
    if (a.reverse)   flags |= TUI_ATTR_REVERSE;
    if (a.italic)    flags |= TUI_ATTR_ITALIC;
    return flags;
}

/* ============================================================================
 * Static State
 * ============================================================================ */

static const ClayNcursesTheme* g_theme = &CLAY_NCURSES_THEME_DEFAULT;

/* ============================================================================
 * Coordinate Mapping
 * ============================================================================
 *
 * Two conversion functions:
 *
 * clay_bbox_to_cells: For rectangles, borders, scissors. Applies aspect
 *   ratio scaling to horizontal values (x, width) so that Clay layout
 *   proportions render correctly in the terminal.
 *
 * clay_text_bbox_to_cells: For text commands. Does NOT apply aspect ratio
 *   because the text measurement callback returns widths in cell columns
 *   (wcwidth units), which are already terminal-accurate.
 */

static TUI_CellRect clay_bbox_to_cells(Clay_BoundingBox bbox) {
    float ar = g_theme->cell_aspect_ratio;

    /* Scale horizontal values by aspect ratio */
    float x = bbox.x * ar;
    float w = bbox.width * ar;

    /* Round to nearest cell (CONTEXT.md decision) */
    int cx = (int)roundf(x);
    int cy = (int)roundf(bbox.y);
    int cw = (int)roundf(w);
    int ch = (int)roundf(bbox.height);

    /* Minimum 1 cell for non-zero dimensions */
    if (bbox.width > 0 && cw < 1) cw = 1;
    if (bbox.height > 0 && ch < 1) ch = 1;

    return (TUI_CellRect){ .x = cx, .y = cy, .w = cw, .h = ch };
}

static TUI_CellRect clay_text_bbox_to_cells(Clay_BoundingBox bbox) {
    /* No aspect ratio scaling -- text widths are already in cell columns */
    int cx = (int)roundf(bbox.x * g_theme->cell_aspect_ratio);
    int cy = (int)roundf(bbox.y);
    int cw = (int)roundf(bbox.width);
    int ch = (int)roundf(bbox.height);

    /* Minimum 1 cell for non-zero dimensions */
    if (bbox.width > 0 && cw < 1) cw = 1;
    if (bbox.height > 0 && ch < 1) ch = 1;

    return (TUI_CellRect){ .x = cx, .y = cy, .w = cw, .h = ch };
}

/* ============================================================================
 * Rectangle Rendering (REND-01, REND-06)
 * ============================================================================
 *
 * Draws a filled rectangle with the Clay element's background color.
 * Alpha < 128 maps to A_DIM when theme->alpha_as_dim is true.
 */

static void render_rectangle(TUI_DrawContext* ctx, TUI_CellRect rect,
                              Clay_RectangleRenderData* data) {
    Clay_Color c = data->backgroundColor;

    TUI_Style style = {
        .fg = TUI_COLOR_DEFAULT,
        .bg = tui_color_rgb((uint8_t)c.r, (uint8_t)c.g, (uint8_t)c.b),
        .attrs = TUI_ATTR_NORMAL,
    };

    if (g_theme->alpha_as_dim && c.a < 128) {
        style.attrs |= TUI_ATTR_DIM;
    }

    tui_draw_fill_rect(ctx, rect, ' ', style);
}

/* ============================================================================
 * Text Rendering (REND-02, REND-06)
 * ============================================================================
 *
 * Renders Clay_StringSlice text. The slice is NOT null-terminated, so we
 * copy to a stack buffer (with malloc fallback for long strings) before
 * passing to tui_draw_text which expects const char*.
 */

/* Find the background color of the nearest parent RECTANGLE that contains
 * the given bounding box. Clay emits commands depth-first: a parent's
 * RECTANGLE always precedes its children's TEXT commands. Scanning backwards
 * from the TEXT index, the first containing RECTANGLE is the innermost parent. */
static Clay_Color find_parent_bg(Clay_RenderCommandArray cmds, int32_t text_idx) {
    Clay_BoundingBox tb = Clay_RenderCommandArray_Get(&cmds, text_idx)->boundingBox;

    for (int32_t j = text_idx - 1; j >= 0; j--) {
        Clay_RenderCommand* prev = Clay_RenderCommandArray_Get(&cmds, j);
        if (prev->commandType != CLAY_RENDER_COMMAND_TYPE_RECTANGLE) continue;

        Clay_BoundingBox rb = prev->boundingBox;
        if (rb.x <= tb.x && rb.y <= tb.y &&
            rb.x + rb.width >= tb.x + tb.width &&
            rb.y + rb.height >= tb.y + tb.height) {
            return prev->renderData.rectangle.backgroundColor;
        }
    }

    return (Clay_Color){0, 0, 0, 0};  /* alpha=0: no parent bg found */
}

static void render_text(TUI_DrawContext* ctx, TUI_CellRect rect,
                         Clay_TextRenderData* data,
                         Clay_Color parent_bg,
                         void* userData) {
    Clay_StringSlice text = data->stringContents;
    if (text.length <= 0 || text.chars == NULL) return;

    /* Null-terminate: stack buffer for typical strings, malloc fallback */
    char buf_stack[512];
    char* buf = buf_stack;
    if (text.length >= (int32_t)sizeof(buf_stack)) {
        buf = (char*)malloc((size_t)text.length + 1);
        if (!buf) return;
    }
    memcpy(buf, text.chars, (size_t)text.length);
    buf[text.length] = '\0';

    Clay_Color c = data->textColor;
    TUI_Color bg = (parent_bg.a > 0)
        ? tui_color_rgb((uint8_t)parent_bg.r, (uint8_t)parent_bg.g,
                        (uint8_t)parent_bg.b)
        : TUI_COLOR_DEFAULT;

    /* Decode text attributes from userData (packed by w_pack_text_attr) */
    uint32_t attrs = TUI_ATTR_NORMAL;
    if (userData) {
        CEL_TextAttr ta = _unpack_text_attr(userData);
        attrs = _text_attr_to_tui(ta);
    }

    TUI_Style style = {
        .fg = tui_color_rgb((uint8_t)c.r, (uint8_t)c.g, (uint8_t)c.b),
        .bg = bg,
        .attrs = attrs,
    };

    tui_draw_text(ctx, rect.x, rect.y, buf, style);

    if (buf != buf_stack) free(buf);
}

/* ============================================================================
 * Border Rendering (REND-03)
 * ============================================================================
 *
 * Builds a per-side bitmask from Clay_BorderRenderData.width and draws
 * using tui_draw_border with TUI_BORDER_SINGLE style. The default theme
 * uses single-line Unicode characters which match TUI_BORDER_SINGLE exactly.
 *
 * Theme overrides Clay's border color (CONTEXT.md decision). For v1, border
 * color uses TUI_COLOR_DEFAULT (terminal foreground). Custom theme chars
 * beyond single/double/rounded would need a custom draw path in v2.
 */

static void render_border(TUI_DrawContext* ctx, TUI_CellRect rect,
                           Clay_BorderRenderData* data,
                           Clay_Color parent_bg) {
    /* Build per-side mask from Clay border widths */
    uint8_t sides = 0;
    if (data->width.top > 0)    sides |= TUI_SIDE_TOP;
    if (data->width.right > 0)  sides |= TUI_SIDE_RIGHT;
    if (data->width.bottom > 0) sides |= TUI_SIDE_BOTTOM;
    if (data->width.left > 0)   sides |= TUI_SIDE_LEFT;

    if (sides == 0) return;

    /* Use Clay border color when provided, else terminal default */
    Clay_Color c = data->color;
    TUI_Color fg = (c.r || c.g || c.b || c.a)
        ? tui_color_rgb((uint8_t)c.r, (uint8_t)c.g, (uint8_t)c.b)
        : TUI_COLOR_DEFAULT;

    /* Use parent rectangle's bg so border chars blend with the fill */
    TUI_Color bg = (parent_bg.a > 0)
        ? tui_color_rgb((uint8_t)parent_bg.r, (uint8_t)parent_bg.g,
                        (uint8_t)parent_bg.b)
        : TUI_COLOR_DEFAULT;

    TUI_Style style = {
        .fg = fg,
        .bg = bg,
        .attrs = TUI_ATTR_NORMAL,
    };

    /* Map Clay properties to TUI border style:
     * - cornerRadius > 0 → rounded
     * - any borderWidth >= 2 → double
     * - else → single (default) */
    TUI_BorderStyle border_style = TUI_BORDER_SINGLE;
    if (data->cornerRadius.topLeft > 0 || data->cornerRadius.topRight > 0 ||
        data->cornerRadius.bottomLeft > 0 || data->cornerRadius.bottomRight > 0) {
        border_style = TUI_BORDER_ROUNDED;
    } else if (data->width.top >= 2 || data->width.right >= 2 ||
               data->width.bottom >= 2 || data->width.left >= 2) {
        border_style = TUI_BORDER_DOUBLE;
    }

    tui_draw_border(ctx, rect, sides, border_style, style);
}

/* ============================================================================
 * Provider Callback (REND-04 scissor, REND-05 coordinate mapping)
 * ============================================================================
 *
 * The main render loop. Called as a ClayRenderable provider at OnStore phase,
 * after ClayRenderDispatch has updated the ClayRenderableData singleton.
 *
 * Flow:
 *   1. Check dirty flag (skip if no commands)
 *   2. Get background layer draw context
 *   3. Reset scissor stack
 *   4. Iterate render commands, dispatch by type
 */

static void clay_ncurses_render(CELS_Iter* it) {
    int count = cels_iter_count(it);
    ClayRenderableData* data = (ClayRenderableData*)cels_iter_column(
        it, ClayRenderableDataID, sizeof(ClayRenderableData));
    for (int i = 0; i < count; i++) {
        if (!data->dirty) continue;

        /* Get background layer and draw context */
        TUI_Layer* layer = tui_frame_get_background();
        if (!layer) continue;
        TUI_DrawContext ctx = tui_layer_get_draw_context(layer);

        /* Reset scissor stack for this frame */
        tui_scissor_reset(&ctx);

        Clay_RenderCommandArray cmds = data->render_commands;
        for (int32_t j = 0; j < cmds.length; j++) {
            Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&cmds, j);

            switch (cmd->commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                    TUI_CellRect cell_rect = clay_bbox_to_cells(cmd->boundingBox);
                    render_rectangle(&ctx, cell_rect, &cmd->renderData.rectangle);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                    /* Text bounding boxes are NOT aspect-ratio-scaled */
                    TUI_CellRect cell_rect = clay_text_bbox_to_cells(cmd->boundingBox);
                    Clay_Color parent_bg = find_parent_bg(cmds, j);
                    render_text(&ctx, cell_rect, &cmd->renderData.text,
                                parent_bg, cmd->userData);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                    TUI_CellRect cell_rect = clay_bbox_to_cells(cmd->boundingBox);
                    Clay_Color border_parent_bg = find_parent_bg(cmds, j);
                    render_border(&ctx, cell_rect, &cmd->renderData.border,
                                  border_parent_bg);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                    TUI_CellRect cell_rect = clay_bbox_to_cells(cmd->boundingBox);
                    tui_push_scissor(&ctx, cell_rect);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                    tui_pop_scissor(&ctx);
                    break;
                }
                default:
                    break;  /* IMAGE, CUSTOM, NONE -- skip silently */
            }
        }
    }
}

/* ============================================================================
 * Text Measurement Callback (REND-07)
 * ============================================================================
 *
 * Provides wcwidth-accurate text dimensions for Clay_SetMeasureTextFunction.
 * Returns width in cell columns and height in lines (newline-separated).
 *
 * The width is NOT divided by aspect ratio. Text widths are reported in
 * cell columns, which is the native terminal unit. The renderer applies
 * aspect ratio only to non-text bounding boxes, keeping text layout
 * pixel-accurate in cell space.
 */

static Clay_Dimensions clay_ncurses_measure_text(
    Clay_StringSlice text,
    Clay_TextElementConfig* config,
    void* userData)
{
    (void)config;
    (void)userData;

    if (text.length <= 0 || text.chars == NULL) {
        return (Clay_Dimensions){ .width = 0, .height = 0 };
    }

    /* Null-terminate for mbstowcs */
    char buf_stack[512];
    char* buf = buf_stack;
    if (text.length >= (int32_t)sizeof(buf_stack)) {
        buf = (char*)malloc((size_t)text.length + 1);
        if (!buf) return (Clay_Dimensions){ .width = 0, .height = 0 };
    }
    memcpy(buf, text.chars, (size_t)text.length);
    buf[text.length] = '\0';

    /* Convert to wide characters for wcwidth measurement */
    wchar_t wbuf_stack[256];
    wchar_t* wbuf = wbuf_stack;
    size_t needed = mbstowcs(NULL, buf, 0);
    if (needed == (size_t)-1) {
        /* Fallback: count bytes as columns */
        if (buf != buf_stack) free(buf);
        return (Clay_Dimensions){ .width = (float)text.length / g_theme->cell_aspect_ratio, .height = 1 };
    }
    if (needed >= 256) {
        wbuf = (wchar_t*)malloc((needed + 1) * sizeof(wchar_t));
        if (!wbuf) {
            if (buf != buf_stack) free(buf);
            return (Clay_Dimensions){ .width = 0, .height = 0 };
        }
    }
    mbstowcs(wbuf, buf, needed + 1);

    /* Walk wide chars: accumulate column width via wcwidth, track newlines */
    float max_width = 0;
    float line_width = 0;
    float height = 1;

    for (size_t i = 0; i < needed; i++) {
        if (wbuf[i] == L'\n') {
            if (line_width > max_width) max_width = line_width;
            line_width = 0;
            height++;
        } else {
            int cw = wcwidth(wbuf[i]);
            if (cw < 0) cw = 0;  /* Non-printable: zero width */
            line_width += (float)cw;
        }
    }
    if (line_width > max_width) max_width = line_width;

    if (wbuf != wbuf_stack) free(wbuf);
    if (buf != buf_stack) free(buf);

    /* Return width in Clay units (divide by aspect ratio).
     * Text measurement in cell columns is terminal-accurate, but Clay's
     * coordinate space is pre-divided by AR (ClaySurface width = terminal/AR).
     * Without this division, Clay over-allocates space for text and centering
     * calculations produce misaligned results in terminal rendering. */
    return (Clay_Dimensions){ .width = max_width / g_theme->cell_aspect_ratio, .height = height };
}

/* ============================================================================
 * Scroll Input State (REND-08)
 * ============================================================================
 *
 * Tracks the previous frame's raw key for multi-key sequence detection (gg).
 * Reset to 0 when no key is pressed in a frame.
 */

static int g_prev_raw_key = 0;

/* ============================================================================
 * Public API
 * ============================================================================ */

void clay_ncurses_renderer_init(const ClayNcursesTheme* theme) {
    g_theme = theme ? theme : &CLAY_NCURSES_THEME_DEFAULT;

    /* Register text measurement callback */
    Clay_SetMeasureTextFunction(clay_ncurses_measure_text, NULL);

    /* Register as ClayRenderable provider (backend name must match TUI_Engine's "TUI") */
    _CEL_Provides(TUI, ClayRenderable, ClayRenderableData, clay_ncurses_render);
}

void clay_ncurses_renderer_set_theme(const ClayNcursesTheme* theme) {
    g_theme = theme ? theme : &CLAY_NCURSES_THEME_DEFAULT;
}

/* ============================================================================
 * Scroll Input Handler (REND-08)
 * ============================================================================
 *
 * Translates CELS_Input key events to Clay scroll deltas. Vim-style bindings
 * are checked first via raw_key; CELS navigation keys (Page Up/Down, arrows)
 * are fallbacks that only apply if no Vim key set a delta.
 *
 * Multi-key gg detection: when 'g' is pressed and the PREVIOUS frame also
 * had 'g' as raw_key, this is the gg sequence (scroll to top). A single 'g'
 * with no preceding 'g' does nothing (waits for the second keypress).
 */

void clay_ncurses_handle_scroll_input(const CELS_Input* input,
                                      float delta_time) {
    Clay_Vector2 scroll_delta = {0.0f, 0.0f};

    if (input == NULL) {
        Clay_UpdateScrollContainers(false, scroll_delta, delta_time);
        return;
    }

    /* Phase 1: Vim key bindings (from raw_key) */
    if (input->has_raw_key) {
        switch (input->raw_key) {
            case 'j':
                scroll_delta.y = 1.0f;
                break;
            case 'k':
                scroll_delta.y = -1.0f;
                break;
            case 4:   /* Ctrl-D (ASCII EOT) */
                scroll_delta.y = 12.0f;
                break;
            case 21:  /* Ctrl-U (ASCII NAK) */
                scroll_delta.y = -12.0f;
                break;
            case 'G':
                scroll_delta.y = 10000.0f;   /* Large value, Clay clamps */
                break;
            case 'g':
                if (g_prev_raw_key == 'g') {
                    scroll_delta.y = -10000.0f;  /* gg: scroll to top */
                }
                /* Single g: no scroll (wait for second g) */
                break;
            default:
                break;
        }
    }

    /* Phase 2: CELS navigation keys (fallback if Vim key didn't fire) */
    if (scroll_delta.y == 0.0f) {
        if (input->key_page_down) {
            scroll_delta.y = 12.0f;
        } else if (input->key_page_up) {
            scroll_delta.y = -12.0f;
        }
    }

    /* Phase 3: Arrow keys via axis (fallback if nothing else fired) */
    if (scroll_delta.y == 0.0f) {
        if (input->axis_left[1] > 0.5f) {
            scroll_delta.y = 1.0f;   /* Down */
        } else if (input->axis_left[1] < -0.5f) {
            scroll_delta.y = -1.0f;  /* Up */
        }
    }

    /* Update prev_key state for multi-key sequence detection */
    if (input->has_raw_key) {
        g_prev_raw_key = input->raw_key;
    } else {
        g_prev_raw_key = 0;  /* Reset if no key this frame */
    }

    Clay_UpdateScrollContainers(false, scroll_delta, delta_time);
}
