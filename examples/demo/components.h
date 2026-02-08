/*
 * Demo App Components
 *
 * App-level component and state definitions for the cels-clay demo.
 * Defines navigation state (current page, sidebar selection, focus pane)
 * and settings state (border toggle, color mode toggle).
 *
 * Architecture:
 *   components.h - shared state + component definitions
 *   theme.h      - color theme structs (Theme A, Theme B)
 *   pages.h      - layout functions + compositions (Sidebar, Content, Pages)
 *   main.c       - entry point, module init, input system, root composition
 */

#ifndef DEMO_COMPONENTS_H
#define DEMO_COMPONENTS_H

#include <cels/cels.h>

/* ============================================================================
 * Navigation State
 * ============================================================================
 *
 * Controls which page is displayed, which sidebar item is highlighted,
 * and which pane (sidebar vs content) has keyboard focus.
 *
 * focus_pane: 0 = sidebar (j/k navigates sidebar items)
 *             1 = content (j/k operates within the active page)
 */
CEL_State(NavState, {
    int current_page;       /* 0=Home, 1=Settings, 2=About */
    int sidebar_selected;   /* Highlighted sidebar item index */
    int focus_pane;         /* 0=sidebar, 1=content */
});

/* ============================================================================
 * Demo Settings State
 * ============================================================================
 *
 * Persisted across page navigation. Toggled from the Settings page.
 * show_borders: controls whether the content area has visible borders
 * color_mode: 0 = Theme A (cool blue), 1 = Theme B (warm amber)
 */
CEL_State(DemoSettings, {
    bool show_borders;      /* Content area border visibility */
    int color_mode;         /* 0=Theme A, 1=Theme B */
});

/* ============================================================================
 * NavItemData Component
 * ============================================================================
 *
 * Attached to NavItem entities so layout functions can read the label
 * text and item index for highlight logic. Defined as a component
 * (not state) because each NavItem entity has its own instance.
 */
CEL_Define(NavItemData, {
    const char* label;      /* Display text for sidebar item */
    int index;              /* Index for highlight comparison with NavState */
});

#endif /* DEMO_COMPONENTS_H */
