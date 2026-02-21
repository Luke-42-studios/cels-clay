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
 * Demo App Color Themes
 *
 * Two switchable color themes for the cels-clay demo. Layout functions
 * read DemoSettings.color_mode and call demo_get_theme() to get the
 * active theme's color palette. All colors are Clay_Color {r, g, b, a}.
 *
 * Theme A: Cool dark scheme with blue tints
 * Theme B: Warm dark scheme with amber/brown tints
 *
 * Colors are specified as 0-255 RGBA. The ncurses renderer maps these
 * to terminal colors via alloc_pair() -- approximate RGB matching.
 */

#ifndef DEMO_THEME_H
#define DEMO_THEME_H

#include <clay.h>

/* ============================================================================
 * DemoTheme - Color palette for a single theme
 * ============================================================================ */
typedef struct DemoTheme {
    Clay_Color sidebar_bg;       /* Sidebar background */
    Clay_Color content_bg;       /* Content area background */
    Clay_Color title_bar_bg;     /* Title bar background */
    Clay_Color status_bar_bg;    /* Status bar background */
    Clay_Color selected_bg;      /* Selected/highlighted item background */
    Clay_Color focused_border;   /* Border color on focused pane */
    Clay_Color text_primary;     /* Main text color */
    Clay_Color text_secondary;   /* Dimmer text (hints, labels) */
    Clay_Color text_accent;      /* Accent text (headings, highlights) */
} DemoTheme;

/* ============================================================================
 * Theme A - Cool Blue
 * ============================================================================
 *
 * Dark background with subtle blue tints. Sidebar is slightly darker
 * than content area for visual separation.
 */
static const DemoTheme DEMO_THEME_A = {
    .sidebar_bg      = { 25,  28,  38, 255},
    .content_bg      = { 30,  33,  45, 255},
    .title_bar_bg    = { 20,  22,  35, 255},
    .status_bar_bg   = { 20,  22,  35, 255},
    .selected_bg     = { 50,  55,  80, 255},
    .focused_border  = { 80, 100, 180, 255},
    .text_primary    = {200, 205, 220, 255},
    .text_secondary  = {120, 130, 160, 255},
    .text_accent     = {130, 160, 255, 255},
};

/* ============================================================================
 * Theme B - Warm Amber
 * ============================================================================
 *
 * Dark background with warm amber/brown tints. Same structural contrast
 * between sidebar and content as Theme A.
 */
static const DemoTheme DEMO_THEME_B = {
    .sidebar_bg      = { 35,  28,  22, 255},
    .content_bg      = { 40,  33,  28, 255},
    .title_bar_bg    = { 30,  22,  18, 255},
    .status_bar_bg   = { 30,  22,  18, 255},
    .selected_bg     = { 75,  55,  35, 255},
    .focused_border  = {180, 130,  60, 255},
    .text_primary    = {220, 205, 190, 255},
    .text_secondary  = {160, 130, 100, 255},
    .text_accent     = {255, 180,  80, 255},
};

/* ============================================================================
 * demo_get_theme - Retrieve active theme by color_mode index
 * ============================================================================ */
static inline const DemoTheme* demo_get_theme(int color_mode) {
    return (color_mode == 1) ? &DEMO_THEME_B : &DEMO_THEME_A;
}

#endif /* DEMO_THEME_H */
