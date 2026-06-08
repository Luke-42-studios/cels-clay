/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Clay ncurses Renderer - Implementation
 *
 * Translates Clay_RenderCommandArray into terminal output using the
 * cels-ncurses drawing API. Registers as a render backend via
 * cels_system_declare().
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
 *   - No manual color pair tracking (uses ncurses_color_rgb + ncurses_style_apply)
 *   - No re-implemented scissor (uses ncurses_push_scissor/ncurses_pop_scissor)
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 *       It is conditionally included when the cels-ncurses target exists.
 */

#include "cels-clay-ncurses/clay_ncurses_renderer.h"
#include "cels-clay/clay_render.h"
#include "clay.h"
#include <cels/cels.h>

#include <cels_ncurses.h>
#include <cels_ncurses_draw.h>

/* CEL_TextAttr -- text attribute flags unpacked from pointer-packed bits.
 * Previously in cels-layout/types.h; defined locally to avoid the dependency. */
typedef struct CEL_TextAttr {
    bool bold;
    bool dim;
    bool underline;
    bool reverse;
    bool italic;
} CEL_TextAttr;

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
 * Convert to NCURSES_ATTR_* bitmask for the ncurses style system.
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

static inline uint32_t _text_attr_to_ncurses(CEL_TextAttr a) {
    uint32_t flags = NCURSES_ATTR_NORMAL;
    if (a.bold)      flags |= NCURSES_ATTR_BOLD;
    if (a.dim)       flags |= NCURSES_ATTR_DIM;
    if (a.underline) flags |= NCURSES_ATTR_UNDERLINE;
    if (a.reverse)   flags |= NCURSES_ATTR_REVERSE;
    if (a.italic)    flags |= NCURSES_ATTR_ITALIC;
    return flags;
}

/* Clamp a float color channel to the valid [0,255] uint8 range.
 * The color resolution chain is supposed to replace the CELS_DEFAULT_COLOR
 * sentinel {-1,-1,-1,-1} before commands are emitted, but if any path slips
 * an unresolved sentinel through, a bare (uint8_t)(-1.0f) cast is
 * implementation-defined (typically 255) and paints a wrong color. Clamping
 * at the renderer boundary fails predictably (to black) instead. */
static inline uint8_t clamp8(float v) {
    if (v < 0.0f)   return 0;
    if (v > 255.0f) return 255;
    return (uint8_t)v;
}

/* ============================================================================
 * Static State
 * ============================================================================ */

static const ClayNcursesTheme* g_theme = NULL;

/* Draw context pointer for the active surface (set by the consumer or
 * derived from stdscr as a fallback). */

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

static NCurses_CellRect clay_bbox_to_cells(Clay_BoundingBox bbox) {
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

    return (NCurses_CellRect){ .x = cx, .y = cy, .w = cw, .h = ch };
}

static NCurses_CellRect clay_text_bbox_to_cells(Clay_BoundingBox bbox) {
    /* x origin IS aspect-scaled to align with AR-scaled rectangles;
     * width/height stay in cell columns (already terminal-accurate). */
    int cx = (int)roundf(bbox.x * g_theme->cell_aspect_ratio);
    int cy = (int)roundf(bbox.y);
    int cw = (int)roundf(bbox.width);
    int ch = (int)roundf(bbox.height);

    /* Minimum 1 cell for non-zero dimensions */
    if (bbox.width > 0 && cw < 1) cw = 1;
    if (bbox.height > 0 && ch < 1) ch = 1;

    return (NCurses_CellRect){ .x = cx, .y = cy, .w = cw, .h = ch };
}

/* (Overlay layer removed -- the old NCurses_Layer API no longer exists.
 * Clay sorts commands by zIndex so overlay content draws naturally
 * after background content.) */

/* ============================================================================
 * Rectangle Rendering (REND-01, REND-06)
 * ============================================================================
 *
 * Draws a filled rectangle with the Clay element's background color.
 * Alpha < 128 maps to A_DIM when theme->alpha_as_dim is true.
 */

static void render_rectangle(NCurses_DrawContext* ctx, NCurses_CellRect rect,
                              Clay_RectangleRenderData* data) {
    Clay_Color c = data->backgroundColor;

    NCurses_Style style = {
        .fg = NCURSES_COLOR_DEFAULT,
        .bg = ncurses_color_rgb(clamp8(c.r), clamp8(c.g), clamp8(c.b)),
        .attrs = NCURSES_ATTR_NORMAL,
    };

    if (g_theme->alpha_as_dim && c.a < 128) {
        style.attrs |= NCURSES_ATTR_DIM;
    }

    ncurses_draw_fill_rect(ctx, rect, ' ', style);
}

/* ============================================================================
 * Text Rendering (REND-02, REND-06)
 * ============================================================================
 *
 * Renders Clay_StringSlice text. The slice is NOT null-terminated, so we
 * copy to a stack buffer (with malloc fallback for long strings) before
 * passing to ncurses_draw_text which expects const char*.
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

        /* Skip transparent rectangles (alpha < 2 = border-only, no fill) */
        Clay_Color bg = prev->renderData.rectangle.backgroundColor;
        if (bg.a < 2.0f) continue;

        Clay_BoundingBox rb = prev->boundingBox;
        if (rb.x <= tb.x && rb.y <= tb.y &&
            rb.x + rb.width >= tb.x + tb.width &&
            rb.y + rb.height >= tb.y + tb.height) {
            return bg;
        }
    }

    return (Clay_Color){0, 0, 0, 0};  /* alpha=0: no parent bg found */
}

static void render_text(NCurses_DrawContext* ctx, NCurses_CellRect rect,
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
    NCurses_Color bg = (parent_bg.a > 0)
        ? ncurses_color_rgb(clamp8(parent_bg.r), clamp8(parent_bg.g),
                            clamp8(parent_bg.b))
        : NCURSES_COLOR_DEFAULT;

    /* Decode text attributes from userData (packed by w_pack_text_attr) */
    uint32_t attrs = NCURSES_ATTR_NORMAL;
    if (userData) {
        CEL_TextAttr ta = _unpack_text_attr(userData);
        attrs = _text_attr_to_ncurses(ta);
    }

    NCurses_Style style = {
        .fg = ncurses_color_rgb(clamp8(c.r), clamp8(c.g), clamp8(c.b)),
        .bg = bg,
        .attrs = attrs,
    };

    ncurses_draw_text(ctx, rect.x, rect.y, buf, style);

    if (buf != buf_stack) free(buf);
}

/* ============================================================================
 * Border Rendering (REND-03)
 * ============================================================================
 *
 * Builds a per-side bitmask from Clay_BorderRenderData.width and draws
 * using ncurses_draw_border with NCURSES_BORDER_SINGLE style. The default theme
 * uses single-line Unicode characters which match NCURSES_BORDER_SINGLE exactly.
 *
 * Theme overrides Clay's border color (CONTEXT.md decision). For v1, border
 * color uses NCURSES_COLOR_DEFAULT (terminal foreground). Custom theme chars
 * beyond single/double/rounded would need a custom draw path in v2.
 */

static void render_border(NCurses_DrawContext* ctx, NCurses_CellRect rect,
                           Clay_BorderRenderData* data,
                           Clay_Color parent_bg) {
    /* Build per-side mask from Clay border widths */
    uint8_t sides = 0;
    if (data->width.top > 0)    sides |= NCURSES_SIDE_TOP;
    if (data->width.right > 0)  sides |= NCURSES_SIDE_RIGHT;
    if (data->width.bottom > 0) sides |= NCURSES_SIDE_BOTTOM;
    if (data->width.left > 0)   sides |= NCURSES_SIDE_LEFT;

    if (sides == 0) return;

    /* Use Clay border color when provided, else terminal default */
    Clay_Color c = data->color;
    NCurses_Color fg = (c.r || c.g || c.b || c.a)
        ? ncurses_color_rgb(clamp8(c.r), clamp8(c.g), clamp8(c.b))
        : NCURSES_COLOR_DEFAULT;

    /* Use parent rectangle's bg so border chars blend with the fill */
    NCurses_Color bg = (parent_bg.a > 0)
        ? ncurses_color_rgb(clamp8(parent_bg.r), clamp8(parent_bg.g),
                            clamp8(parent_bg.b))
        : NCURSES_COLOR_DEFAULT;

    NCurses_Style style = {
        .fg = fg,
        .bg = bg,
        .attrs = NCURSES_ATTR_NORMAL,
    };

    /* Map Clay properties to NCurses border style:
     * - cornerRadius > 0 → rounded
     * - any borderWidth >= 2 → double
     * - else → single (default) */
    NCurses_BorderStyle border_style = NCURSES_BORDER_SINGLE;
    if (data->cornerRadius.topLeft > 0 || data->cornerRadius.topRight > 0 ||
        data->cornerRadius.bottomLeft > 0 || data->cornerRadius.bottomRight > 0) {
        border_style = NCURSES_BORDER_ROUNDED;
    } else if (data->width.top >= 2 || data->width.right >= 2 ||
               data->width.bottom >= 2 || data->width.left >= 2) {
        border_style = NCURSES_BORDER_DOUBLE;
    }

    ncurses_draw_border(ctx, rect, sides, border_style, style);
}

/* ============================================================================
 * Border Decoration Rendering (CelClayBorderDecor)
 * ============================================================================
 *
 * Draws a NCurses border at the RECTANGLE edges when a CelClayBorderDecor is
 * attached via userData. Bypasses Clay's border system (which AR-scales
 * uint16_t widths to 2+ cells) — draws 1-cell-wide box-drawing characters.
 * Optional title-in-border overlays text on the top border line.
 */

static void render_border_decor(NCurses_DrawContext* ctx, NCurses_CellRect rect,
                                 CelClayBorderDecor* decor,
                                 Clay_Color parent_bg) {
    /* Map border style enum to NCurses border style */
    NCurses_BorderStyle bs;
    switch (decor->border_style) {
        case 1:  bs = NCURSES_BORDER_SINGLE; break;
        case 2:  bs = NCURSES_BORDER_DOUBLE; break;
        default: bs = NCURSES_BORDER_ROUNDED; break;
    }

    /* Fill ONLY the interior (inside the border) with panel bg.
     * Skip fill when bg alpha < 2 (alpha 0-1 = transparent / border-only). */
    Clay_Color ibg = decor->bg_color;
    NCurses_CellRect inner = {
        .x = rect.x + 1, .y = rect.y + 1,
        .w = rect.w - 2, .h = rect.h - 2
    };
    if (inner.w > 0 && inner.h > 0 && ibg.a >= 2.0f) {
        NCurses_Style fill_style = {
            .fg = NCURSES_COLOR_DEFAULT,
            .bg = ncurses_color_rgb(clamp8(ibg.r), clamp8(ibg.g), clamp8(ibg.b)),
            .attrs = NCURSES_ATTR_NORMAL,
        };
        ncurses_draw_fill_rect(ctx, inner, ' ', fill_style);
    }

    /* Border style: border fg on parent/terminal bg (no panel bg bleed) */
    Clay_Color c = decor->border_color;
    NCurses_Color border_bg = (parent_bg.a > 0)
        ? ncurses_color_rgb(clamp8(parent_bg.r), clamp8(parent_bg.g),
                            clamp8(parent_bg.b))
        : NCURSES_COLOR_DEFAULT;
    NCurses_Style style = {
        .fg = ncurses_color_rgb(clamp8(c.r), clamp8(c.g), clamp8(c.b)),
        .bg = border_bg,
        .attrs = NCURSES_ATTR_NORMAL,
    };

    /* Draw border at rectangle edges */
    ncurses_draw_border(ctx, rect, NCURSES_SIDE_ALL, bs, style);

    /* Draw title on top border line: " Title " overlaying the hline */
    int title_end = rect.x + 1; /* track where title ends for right_text */
    if (decor->title && decor->title[0] != '\0') {
        Clay_Color tc = decor->title_color;
        uint32_t attrs = NCURSES_ATTR_NORMAL;
        if (decor->title_text_attr) {
            CEL_TextAttr ta = _unpack_text_attr((void*)decor->title_text_attr);
            attrs = _text_attr_to_ncurses(ta);
        }

        NCurses_Style title_style = {
            .fg = ncurses_color_rgb(clamp8(tc.r), clamp8(tc.g), clamp8(tc.b)),
            .bg = border_bg,
            .attrs = attrs,
        };

        /* Format: " Title " with spaces as visual separator from border */
        char title_buf[256];
        int len = snprintf(title_buf, sizeof(title_buf), " %s ", decor->title);
        (void)len;

        /* Position 1 cell after upper-left corner, bounded to panel width */
        int title_x = rect.x + 1;
        int max_cols = rect.w - 2; /* Leave 1 cell for each corner */
        if (max_cols > 0) {
            ncurses_draw_text_bounded(ctx, title_x, rect.y, title_buf,
                                      max_cols, title_style);
            int tlen = (int)strlen(title_buf);
            title_end = title_x + (tlen < max_cols ? tlen : max_cols);
        }
    }

    /* Draw right-aligned text on top border line (e.g., "[X]" for windows) */
    if (decor->right_text && decor->right_text[0] != '\0') {
        Clay_Color rc = decor->right_color;
        NCurses_Style right_style = {
            .fg = ncurses_color_rgb(clamp8(rc.r), clamp8(rc.g), clamp8(rc.b)),
            .bg = border_bg,
            .attrs = NCURSES_ATTR_NORMAL,
        };

        char right_buf[64];
        int rlen = snprintf(right_buf, sizeof(right_buf), " %s ", decor->right_text);
        (void)rlen;

        int right_end = rect.x + rect.w - 1; /* 1 cell before right corner */
        int right_x = right_end - (int)strlen(right_buf);
        if (right_x > title_end && right_x >= rect.x + 1) {
            int max_cols = right_end - right_x;
            ncurses_draw_text_bounded(ctx, right_x, rect.y, right_buf,
                                      max_cols, right_style);
        }
    }
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

static void clay_ncurses_render(cels_iter_t* it) {
    (void)it;
    /* Read render commands directly from the layout system getter
     * rather than querying the singleton via cels_iter_column. */
    Clay_RenderCommandArray cmds_check = cel_clay_get_render_commands();
    if (cmds_check.length <= 0) return;

    {
        /* Clear screen before drawing — no NCursesSurface means no automatic
         * clear/refresh cycle. We own the full stdscr lifecycle here. */
        werase(stdscr);

        /* Create draw context from stdscr (full terminal surface). */
        NCurses_DrawContext bg_ctx = ncurses_draw_context_create(
            stdscr, 0, 0, COLS, LINES);

        /* Reset scissor stack */
        ncurses_scissor_reset(&bg_ctx);

        Clay_RenderCommandArray cmds = cmds_check;

        /* Render pass: all commands draw to background surface */
        NCurses_DrawContext* ctx = &bg_ctx;

        for (int32_t j = 0; j < cmds.length; j++) {
            Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&cmds, j);

            switch (cmd->commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                    NCurses_CellRect cell_rect = clay_bbox_to_cells(cmd->boundingBox);
                    if (cmd->userData) {
                        /* Border decoration: skip normal full-area fill.
                         * render_border_decor fills only the interior (inside
                         * border) so panel bg doesn't bleed outside. */
                        Clay_Color parent_bg = find_parent_bg(cmds, j);
                        render_border_decor(ctx, cell_rect,
                                             (CelClayBorderDecor*)cmd->userData,
                                             parent_bg);
                    } else {
                        render_rectangle(ctx, cell_rect, &cmd->renderData.rectangle);
                    }
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                    /* Text bounding boxes are NOT aspect-ratio-scaled */
                    NCurses_CellRect cell_rect = clay_text_bbox_to_cells(cmd->boundingBox);
                    Clay_Color parent_bg = find_parent_bg(cmds, j);
                    render_text(ctx, cell_rect, &cmd->renderData.text,
                                parent_bg, cmd->userData);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                    NCurses_CellRect cell_rect = clay_bbox_to_cells(cmd->boundingBox);
                    Clay_Color border_parent_bg = find_parent_bg(cmds, j);
                    render_border(ctx, cell_rect, &cmd->renderData.border,
                                  border_parent_bg);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                    NCurses_CellRect cell_rect = clay_bbox_to_cells(cmd->boundingBox);
                    ncurses_push_scissor(ctx, cell_rect);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                    ncurses_pop_scissor(ctx);
                    break;
                }
                default:
                    break;  /* IMAGE, CUSTOM, NONE -- skip silently */
            }
        }

        /* Present the frame */
        wnoutrefresh(stdscr);
        doupdate();
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
    /* Re-check the second conversion: if the locale/multibyte state changes
     * between the two calls (or an invalid sequence appears mid-string), this
     * can return (size_t)-1, leaving wbuf partially/never initialized -- the
     * wcwidth loop below would then read uninitialized memory. Bail to the
     * byte-count fallback on failure. */
    size_t converted = mbstowcs(wbuf, buf, needed + 1);
    if (converted == (size_t)-1) {
        if (wbuf != wbuf_stack) free(wbuf);
        if (buf != buf_stack) free(buf);
        return (Clay_Dimensions){ .width = (float)text.length / g_theme->cell_aspect_ratio, .height = 1 };
    }

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
 * Module Definition
 * ============================================================================ */

CEL_Module(Clay_NCurses, init) {
    /* Use stored theme (from Clay_NCurses_configure) or default */
    if (!g_theme) g_theme = &CLAY_NCURSES_THEME_DEFAULT;

    /* Register text measurement callback */
    Clay_SetMeasureTextFunction(clay_ncurses_measure_text, NULL);

    /* Register render system at OnRender phase */
    ClayRenderableData_register();
    cels_entity_t comp_ids[] = { ClayRenderableData_id };
    cels_system_declare("NCurses_ClayRenderable_ClayRenderableData",
                        OnRender, clay_ncurses_render, comp_ids, 1);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void Clay_NCurses_configure(const ClayNcursesTheme* theme) {
    g_theme = theme ? theme : &CLAY_NCURSES_THEME_DEFAULT;
}

void clay_ncurses_renderer_set_theme(const ClayNcursesTheme* theme) {
    g_theme = theme ? theme : &CLAY_NCURSES_THEME_DEFAULT;
}

/* ============================================================================
 * Scroll Input Handler (REND-08)
 * ============================================================================
 *
 * Translates NCurses_InputState key events to Clay scroll deltas. Vim-style
 * bindings are checked first via raw keys; ncurses navigation keys
 * (Page Up/Down, arrows) are fallbacks that only apply if no Vim key set
 * a delta.
 *
 * Multi-key gg detection: when 'g' is pressed and the PREVIOUS frame also
 * had 'g' as raw_key, this is the gg sequence (scroll to top). A single 'g'
 * with no preceding 'g' does nothing (waits for the second keypress).
 */

void clay_ncurses_handle_scroll_input(const struct NCurses_InputState* input,
                                      float delta_time) {
    Clay_Vector2 scroll_delta = {0.0f, 0.0f};

    if (input == NULL) {
        Clay_UpdateScrollContainers(false, scroll_delta, delta_time);
        return;
    }

    /* Scan all keys pressed this frame */
    int last_raw_key = 0;
    for (int ki = 0; ki < input->key_count && ki < 16; ki++) {
        int key = input->keys[ki];
        last_raw_key = key;

        /* Phase 1: Vim key bindings */
        switch (key) {
            case 'j':
            case KEY_DOWN:
                if (scroll_delta.y == 0.0f) scroll_delta.y = 1.0f;
                break;
            case 'k':
            case KEY_UP:
                if (scroll_delta.y == 0.0f) scroll_delta.y = -1.0f;
                break;
            case 4:   /* Ctrl-D (ASCII EOT) */
            case KEY_NPAGE:
                scroll_delta.y = 12.0f;
                break;
            case 21:  /* Ctrl-U (ASCII NAK) */
            case KEY_PPAGE:
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

    /* Update prev_key state for multi-key sequence detection */
    if (input->key_count > 0) {
        g_prev_raw_key = last_raw_key;
    } else {
        g_prev_raw_key = 0;  /* Reset if no key this frame */
    }

    Clay_UpdateScrollContainers(false, scroll_delta, delta_time);
}
