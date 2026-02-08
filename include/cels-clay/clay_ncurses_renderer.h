/*
 * Clay ncurses Renderer - Terminal renderer for Clay render commands
 *
 * Translates Clay_RenderCommandArray into visible terminal output using
 * cels-ncurses drawing primitives. Registers as a _CEL_Provides backend
 * for the ClayRenderable feature.
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
 *   // In module init, after Clay_Engine_use():
 *   clay_ncurses_renderer_init(NULL);  // NULL = default theme
 *
 *   // Or with custom theme:
 *   static const ClayNcursesTheme my_theme = {
 *       .border = { ... },
 *       .cell_aspect_ratio = 1.8f,
 *   };
 *   clay_ncurses_renderer_init(&my_theme);
 *
 * NOTE: This header does NOT include ncurses or cels-ncurses headers.
 * The theme struct is pure C data. The renderer source includes ncurses
 * internally. Consumers include both cels-clay and cels-ncurses modules
 * independently.
 */

#ifndef CELS_CLAY_NCURSES_RENDERER_H
#define CELS_CLAY_NCURSES_RENDERER_H

#include <stdbool.h>

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
 * Renderer API
 * ============================================================================ */

/* Initialize the ncurses Clay renderer and register as a ClayRenderable
 * provider. Pass NULL to use CLAY_NCURSES_THEME_DEFAULT.
 *
 * This function:
 *   1. Stores the theme pointer
 *   2. Converts theme UTF-8 border strings to cchar_t for ncurses
 *   3. Registers text measurement callback via Clay_SetMeasureTextFunction
 *   4. Registers as provider via _CEL_Provides(NCurses, ClayRenderable, ...)
 *
 * Call after Clay_Engine_use() and cels-ncurses initialization. */
extern void clay_ncurses_renderer_init(const ClayNcursesTheme* theme);

/* Change the renderer theme at runtime. Pass NULL for default theme.
 * Re-initializes border character conversions. */
extern void clay_ncurses_renderer_set_theme(const ClayNcursesTheme* theme);

#endif /* CELS_CLAY_NCURSES_RENDERER_H */
