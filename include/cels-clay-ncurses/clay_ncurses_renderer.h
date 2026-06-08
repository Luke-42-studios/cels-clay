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
 * Clay NCurses Renderer Module - Terminal renderer for Clay render commands
 *
 * Translates Clay_RenderCommandArray into visible terminal output using
 * cels-ncurses drawing primitives. Registered as a CEL_Module for
 * automatic initialization via cels_register().
 *
 * The renderer handles 5 Clay command types:
 *   RECTANGLE    -> tui_draw_fill_rect (filled background)
 *   TEXT         -> tui_draw_text (null-terminated copy of StringSlice)
 *   BORDER       -> tui_draw_border (theme-driven box-drawing)
 *   SCISSOR_START -> tui_push_scissor (nested clip regions)
 *   SCISSOR_END   -> tui_pop_scissor (restore parent clip)
 *
 * Theme system: ClayNcursesTheme controls visual appearance (border
 * characters, scrollbar characters, aspect ratio, alpha handling).
 * The default theme uses single-line Unicode box-drawing and 2:1
 * aspect ratio compensation for typical terminal fonts.
 *
 * Usage:
 *   #include <cels-clay/clay_ncurses_renderer.h>
 *
 *   CEL_Build(App) {
 *       cels_register(Clay_Engine, Clay_NCurses);
 *       // Optional: Clay_NCurses_configure(&custom_theme); before register
 *   }
 *
 *   // Or with custom theme (call before cels_register):
 *   static const ClayNcursesTheme my_theme = {
 *       .border = { ... },
 *       .cell_aspect_ratio = 1.8f,
 *   };
 *   Clay_NCurses_configure(&my_theme);
 *   cels_register(Clay_NCurses);
 *
 * NOTE: This header does NOT include ncurses or cels-ncurses headers.
 * The theme struct is pure C data. The renderer source includes ncurses
 * internally. Consumers include both cels-clay and cels-ncurses modules
 * independently.
 */

#ifndef CELS_CLAY_NCURSES_RENDERER_H
#define CELS_CLAY_NCURSES_RENDERER_H

#include <stdbool.h>
#include <cels/cels.h>

/* ============================================================================
 * ClayNcursesTheme - Visual appearance configuration
 * ============================================================================
 *
 * Controls how Clay render commands map to terminal visuals. Border and
 * scrollbar characters are UTF-8 string literals (converted to cchar_t
 * at renderer init time). Aspect ratio compensates for terminal cells
 * being taller than wide.
 */
typedef struct ClayNcursesTheme {
    /* Box-drawing characters for borders (UTF-8 string literals) */
    struct {
        const char* hline;  /* Horizontal line (e.g., U+2500 "---") */
        const char* vline;  /* Vertical line   (e.g., U+2502 "|")   */
        const char* ul;     /* Upper-left corner  (e.g., U+250C)    */
        const char* ur;     /* Upper-right corner (e.g., U+2510)    */
        const char* ll;     /* Lower-left corner  (e.g., U+2514)    */
        const char* lr;     /* Lower-right corner (e.g., U+2518)    */
    } border;

    /* Scrollbar characters (UTF-8 string literals) */
    struct {
        const char* track;  /* Track character  (e.g., U+2502)      */
        const char* thumb;  /* Thumb character  (e.g., U+2588 full block) */
    } scrollbar;

    /* Aspect ratio compensation: terminal cells are typically ~2x taller
     * than wide. This scales horizontal bounding box values at render time
     * so that Clay layout proportions appear correct on screen.
     * Default: 2.0f (cells are 2x taller than wide). */
    float cell_aspect_ratio;

    /* Alpha channel handling: when true, Clay colors with alpha < 128
     * are rendered with the A_DIM ncurses attribute. */
    bool alpha_as_dim;
} ClayNcursesTheme;

/* ============================================================================
 * Default Theme
 * ============================================================================
 *
 * Single-line Unicode box-drawing borders, full-block scrollbar thumb,
 * 2:1 aspect ratio, alpha-as-dim enabled.
 *
 * UTF-8 byte sequences used instead of \u escapes for C99 portability.
 */
static const ClayNcursesTheme CLAY_NCURSES_THEME_DEFAULT = {
    .border = {
        .hline = "\xe2\x94\x80",  /* U+2500 BOX DRAWINGS LIGHT HORIZONTAL */
        .vline = "\xe2\x94\x82",  /* U+2502 BOX DRAWINGS LIGHT VERTICAL   */
        .ul    = "\xe2\x94\x8c",  /* U+250C BOX DRAWINGS LIGHT DOWN AND RIGHT */
        .ur    = "\xe2\x94\x90",  /* U+2510 BOX DRAWINGS LIGHT DOWN AND LEFT  */
        .ll    = "\xe2\x94\x94",  /* U+2514 BOX DRAWINGS LIGHT UP AND RIGHT   */
        .lr    = "\xe2\x94\x98",  /* U+2518 BOX DRAWINGS LIGHT UP AND LEFT    */
    },
    .scrollbar = {
        .track = "\xe2\x94\x82",  /* U+2502 (same as vline) */
        .thumb = "\xe2\x96\x88",  /* U+2588 FULL BLOCK      */
    },
    .cell_aspect_ratio = 2.0f,
    .alpha_as_dim = true,
};

/* ============================================================================
 * Module Declaration
 * ============================================================================ */

CEL_Module(Clay_NCurses);

/* ============================================================================
 * Renderer API
 * ============================================================================ */

/* Configure NCurses Clay renderer theme before module registration.
 * Call before cels_register(Clay_NCurses). Pass NULL for default theme.
 * If not called, default theme is used. */
extern void Clay_NCurses_configure(const ClayNcursesTheme* theme);

/* Change the renderer theme at runtime. Pass NULL for default theme.
 * Re-initializes border character conversions. */
extern void clay_ncurses_renderer_set_theme(const ClayNcursesTheme* theme);

/* Forward declaration for NCurses input state (defined in cels_ncurses.h) */
struct NCurses_InputState;

/* Process keyboard input for Clay scroll containers.
 *
 * Maps Vim-style key bindings to scroll deltas and feeds them to
 * Clay_UpdateScrollContainers. Call this each frame BEFORE Clay_BeginLayout.
 *
 * Key bindings:
 *   j / Down arrow    -> scroll down 1 line
 *   k / Up arrow      -> scroll up 1 line
 *   Ctrl-D / Page Down -> scroll down half page (12 lines)
 *   Ctrl-U / Page Up   -> scroll up half page (12 lines)
 *   G                  -> scroll to bottom (large positive delta)
 *   gg                 -> scroll to top (large negative delta, requires two
 *                         consecutive 'g' presses across frames)
 *
 * This is a plain function the app calls explicitly -- NOT an ECS system.
 * The app controls which container receives scroll input (focus management).
 *
 * @param input     NCurses_InputState from the current frame (NULL-safe: sends zero delta)
 * @param delta_time Time elapsed since last frame in seconds */
extern void clay_ncurses_handle_scroll_input(const struct NCurses_InputState* input,
                                             float delta_time);

#endif /* CELS_CLAY_NCURSES_RENDERER_H */
