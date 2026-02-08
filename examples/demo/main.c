/*
 * cels-clay Demo Application
 *
 * Three-page terminal app demonstrating the full CELS + Clay + ncurses
 * pipeline. Sidebar navigation (Home, Settings, About), keyboard
 * interaction, live theme toggling, and scroll containers.
 *
 * This is the first consumer of the cels-clay module and serves as the
 * canonical example for building a cels-clay application.
 *
 * Architecture:
 *   CEL_Build(DemoApp) initializes three modules:
 *     1. TUI_Engine_use  - ncurses window + input + frame pipeline
 *     2. Clay_Engine_use - Clay arena + layout system + render bridge
 *     3. clay_ncurses_renderer_init - ncurses as Clay renderer backend
 *
 *   AppUI root composition observes window state and mounts ClaySurface
 *   with aspect-ratio-adjusted dimensions. Inside: AppShell with title
 *   bar, sidebar, content router, and status bar.
 *
 *   Input system reads raw_key for Vim-style navigation (j/k/h/l) and
 *   button_accept for Enter. State mutations via CEL_Update trigger
 *   reactive recomposition in the affected compositions only.
 *
 * File structure:
 *   main.c       - this file (entry point, input system, root)
 *   components.h - NavState, DemoSettings, NavItemData definitions
 *   theme.h      - Theme A (blue) and Theme B (amber) color palettes
 *   pages.h      - layout functions + compositions for all UI elements
 */

#include <cels/cels.h>
#include <cels-ncurses/tui_engine.h>
#include <cels-clay/clay_engine.h>
#include <cels-clay/clay_layout.h>
#include <cels-clay/clay_ncurses_renderer.h>
#include <clay.h>
#include "components.h"
#include "pages.h"
#include "theme.h"
#include <string.h>

/* ============================================================================
 * Input System
 * ============================================================================
 *
 * Runs at OnUpdate phase. Reads CELS_Input for Vim-style navigation:
 *   j/k  - move selection down/up (sidebar or settings toggles)
 *   h/l  - switch focus between sidebar and content pane
 *   Enter - select sidebar item or toggle setting
 *   Scroll keys - only when About page is focused
 *
 * Uses raw_key (NOT axis_left) for j/k/h/l to avoid WASD conflict.
 * Previous input tracking prevents key repeat on held keys.
 *
 * Quit: handled by TUI input provider (Q key sets g_running = 0).
 */

static CELS_Input g_prev_input = {0};

CEL_System(DemoInputSystem, .phase = CELS_Phase_OnUpdate) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    const CELS_Input* input = cels_input_get(ctx);

    /* ---- Vim navigation keys via raw_key ---- */
    if (input->has_raw_key && !g_prev_input.has_raw_key) {
        switch (input->raw_key) {
            case 'j': /* Move selection down */
                CEL_Update(NavState) {
                    if (NavState.focus_pane == 0) {
                        /* Sidebar: cycle through 3 items */
                        NavState.sidebar_selected++;
                        if (NavState.sidebar_selected > 2)
                            NavState.sidebar_selected = 0;
                    } else if (NavState.current_page == 1) {
                        /* Settings: cycle through 2 toggles */
                        NavState.sidebar_selected++;
                        if (NavState.sidebar_selected > 1)
                            NavState.sidebar_selected = 0;
                    }
                }
                break;

            case 'k': /* Move selection up */
                CEL_Update(NavState) {
                    if (NavState.focus_pane == 0) {
                        NavState.sidebar_selected--;
                        if (NavState.sidebar_selected < 0)
                            NavState.sidebar_selected = 2;
                    } else if (NavState.current_page == 1) {
                        NavState.sidebar_selected--;
                        if (NavState.sidebar_selected < 0)
                            NavState.sidebar_selected = 1;
                    }
                }
                break;

            case 'h': /* Focus sidebar */
                CEL_Update(NavState) {
                    NavState.focus_pane = 0;
                }
                break;

            case 'l': /* Focus content */
                CEL_Update(NavState) {
                    NavState.focus_pane = 1;
                }
                break;
        }
    }

    /* ---- Enter: select item or toggle setting ---- */
    if (input->button_accept && !g_prev_input.button_accept) {
        if (NavState.focus_pane == 0) {
            /* Sidebar: switch to selected page, reset content selection */
            CEL_Update(NavState) {
                NavState.current_page = NavState.sidebar_selected;
                /* When entering content, reset selection for settings page */
            }
        } else if (NavState.current_page == 1) {
            /* Settings page: toggle the selected setting */
            switch (NavState.sidebar_selected) {
                case 0:
                    CEL_Update(DemoSettings) {
                        DemoSettings.show_borders = !DemoSettings.show_borders;
                    }
                    break;
                case 1:
                    CEL_Update(DemoSettings) {
                        DemoSettings.color_mode =
                            (DemoSettings.color_mode == 0) ? 1 : 0;
                    }
                    break;
            }
        }
    }

    /* ---- Scroll: only when About page content is focused ---- */
    if (NavState.focus_pane == 1 && NavState.current_page == 2) {
        clay_ncurses_handle_scroll_input(input, g_frame_state.delta_time);
    }

    /* Track previous frame input for edge detection */
    memcpy((void*)&g_prev_input, input, sizeof(CELS_Input));
}

/* ============================================================================
 * Root Composition
 * ============================================================================
 *
 * Observes TUI_WindowState for terminal dimensions. When WINDOW_STATE_READY,
 * mounts ClaySurface with aspect-ratio-adjusted width (terminal cols / 2.0)
 * so Clay's coordinate system maps correctly to terminal cells via the
 * ncurses renderer's 2:1 aspect ratio compensation.
 */

CEL_Root(AppUI, TUI_EngineContext) {
    TUI_WindowState_t* win = CEL_WatchId(ctx.windowState, TUI_WindowState_t);

    if (win->state == WINDOW_STATE_READY) {
        ClaySurface(.width = (float)win->width / 2.0f,
                    .height = (float)win->height) {
            AppShell() {}
        }
    }
}

/* ============================================================================
 * Application Entry Point
 * ============================================================================
 *
 * CEL_Build initializes the three module layers in order:
 *   1. TUI engine (ncurses session, input provider, frame pipeline)
 *   2. Clay engine (arena allocation, layout system, render bridge)
 *   3. ncurses renderer (text measurement, render provider)
 *
 * Then sets initial application state and registers the input system.
 */

CEL_Build(DemoApp) {
    (void)props;

    /* Module initialization */
    TUI_Engine_use((TUI_EngineConfig){
        .title = "cels-clay demo",
        .version = "0.2.0",
        .fps = 60,
        .root = AppUI
    });

    Clay_Engine_use(NULL);  /* All defaults */
    clay_ncurses_renderer_init(NULL);  /* Default theme */

    /* Register input system (global, not lifecycle-scoped) */
    DemoInputSystem_ensure();

    /* Initial application state */
    NavState = (NavState_t){
        .current_page = 0,
        .sidebar_selected = 0,
        .focus_pane = 0  /* Start with sidebar focused */
    };

    DemoSettings = (DemoSettings_t){
        .show_borders = true,
        .color_mode = 0  /* Theme A */
    };
}
