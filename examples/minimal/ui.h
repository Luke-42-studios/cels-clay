/*
 * Shared UI Composition for the Minimal Dual-Renderer Example
 *
 * This file is included by both main_ncurses.c and main_sdl3.c.
 * The entity tree is identical -- only the renderer module differs.
 *
 * Layout: sidebar (25% width) + content area (grow).
 * Uses only ClayColumn, ClayRow, ClayBox, ClayText, ClaySpacer
 * compositions from clay_primitives.h. No backend-specific code.
 *
 * All sizing uses relative Clay macros (GROW, PERCENT, FIT).
 * Colors are high-contrast dark theme, visible on both terminals
 * and graphical windows.
 */

#ifndef CELS_CLAY_EXAMPLE_UI_H
#define CELS_CLAY_EXAMPLE_UI_H

#include <cels-clay/clay_primitives.h>
#include <cels-clay/clay_layout.h>

/* ============================================================================
 * Color Palette -- high contrast, visible on dark terminals and windows
 * ============================================================================ */

#define UI_BG_DARK     (Clay_Color){30, 30, 40, 255}
#define UI_BG_SIDEBAR  (Clay_Color){40, 45, 60, 255}
#define UI_BG_CONTENT  (Clay_Color){35, 38, 50, 255}
#define UI_BG_CARD     (Clay_Color){50, 55, 75, 255}
#define UI_FG_TITLE    (Clay_Color){220, 220, 240, 255}
#define UI_FG_TEXT     (Clay_Color){180, 185, 200, 255}
#define UI_FG_DIM      (Clay_Color){120, 125, 140, 255}
#define UI_ACCENT      (Clay_Color){100, 140, 220, 255}

/* ============================================================================
 * build_ui() -- constructs the complete UI entity tree
 * ============================================================================
 *
 * Call inside a ClaySurface scope. Builds a sidebar + content layout:
 *
 *   AppLayout (Row, full size, dark bg)
 *     Sidebar (Column, 25% width, sidebar bg)
 *       "MENU" title
 *       spacer
 *       "Dashboard" / "Settings" / "About" items
 *       flexible spacer
 *       "v0.6" version label
 *     Content (Column, grow width, content bg)
 *       "Dashboard" page title
 *       Welcome card (Column, card bg)
 *         "Welcome" heading
 *         description text
 *       Color bar card (Column, card bg)
 *         Row of three colored boxes
 *       flexible spacer
 *       footer text
 */
static inline void build_ui(void) {
    /* Root: horizontal layout filling all available space */
    ClayRow(.width = CLAY_SIZING_GROW(0),
            .height = CLAY_SIZING_GROW(0),
            .bg = UI_BG_DARK) {

        /* Sidebar: 25% width vertical panel */
        ClayColumn(.width = CLAY_SIZING_PERCENT(0.25f),
                   .height = CLAY_SIZING_GROW(0),
                   .padding = {.left = 1, .right = 1, .top = 1, .bottom = 1},
                   .gap = 0,
                   .bg = UI_BG_SIDEBAR) {

            ClayText(.text = "MENU", .color = UI_ACCENT) {}
            ClaySpacer(.height = CLAY_SIZING_FIT(1)) {}

            /* Navigation items */
            ClayRow(.width = CLAY_SIZING_GROW(0),
                    .padding = {.left = 1, .right = 1}) {
                ClayText(.text = "Dashboard", .color = UI_FG_TITLE) {}
            }
            ClayRow(.width = CLAY_SIZING_GROW(0),
                    .padding = {.left = 1, .right = 1}) {
                ClayText(.text = "Settings", .color = UI_FG_TEXT) {}
            }
            ClayRow(.width = CLAY_SIZING_GROW(0),
                    .padding = {.left = 1, .right = 1}) {
                ClayText(.text = "About", .color = UI_FG_DIM) {}
            }

            /* Push version label to bottom */
            ClaySpacer() {}
            ClayText(.text = "v0.6", .color = UI_FG_DIM) {}
        }

        /* Content area: grows to fill remaining space */
        ClayColumn(.width = CLAY_SIZING_GROW(0),
                   .height = CLAY_SIZING_GROW(0),
                   .padding = {.left = 1, .right = 1, .top = 1, .bottom = 1},
                   .gap = 1,
                   .bg = UI_BG_CONTENT) {

            ClayText(.text = "Dashboard", .color = UI_FG_TITLE) {}

            /* Welcome card */
            ClayColumn(.width = CLAY_SIZING_GROW(0),
                       .padding = {.left = 1, .right = 1, .top = 1, .bottom = 1},
                       .gap = 0,
                       .bg = UI_BG_CARD) {
                ClayText(.text = "Welcome", .color = UI_ACCENT) {}
                ClayText(.text = "This is a minimal cels-clay example rendering"
                         " on both NCurses and SDL3 backends.",
                         .color = UI_FG_TEXT) {}
            }

            /* Color bar card */
            ClayColumn(.width = CLAY_SIZING_GROW(0),
                       .padding = {.left = 1, .right = 1, .top = 1, .bottom = 1},
                       .bg = UI_BG_CARD) {
                ClayRow(.gap = 1) {
                    ClayBox(.width = CLAY_SIZING_GROW(0),
                            .height = CLAY_SIZING_FIT(3),
                            .bg = UI_ACCENT) {}
                    ClayBox(.width = CLAY_SIZING_GROW(0),
                            .height = CLAY_SIZING_FIT(3),
                            .bg = UI_BG_SIDEBAR) {}
                    ClayBox(.width = CLAY_SIZING_GROW(0),
                            .height = CLAY_SIZING_FIT(3),
                            .bg = UI_BG_CARD) {}
                }
            }

            /* Push footer to bottom */
            ClaySpacer() {}
            ClayText(.text = "cels-clay v0.6 -- Entity-Based UI + Dual Renderers",
                     .color = UI_FG_DIM) {}
        }
    }
}

#endif /* CELS_CLAY_EXAMPLE_UI_H */
