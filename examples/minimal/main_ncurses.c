/*
 * cels-clay Minimal Example -- NCurses Backend
 *
 * Demonstrates the same entity tree from ui.h rendering in a terminal.
 * The only difference from main_sdl3.c is the module registration and
 * window setup -- the UI composition (build_ui) is identical.
 *
 * Build: cmake -DCELS_CLAY_BUILD_EXAMPLES=ON ..
 * Run:   ./cels-clay-example-ncurses
 * Quit:  Press 'q'
 */

#include <cels/cels.h>
#include <cels_ncurses.h>
#include <cels-clay/clay_engine.h>
#include <cels-clay/clay_ncurses_renderer.h>
#include <cels-clay/clay_primitives.h>
#include <cels-clay/clay_layout.h>
#include "ui.h"

/* ============================================================================
 * Input System -- quit on 'q'
 * ============================================================================ */

CEL_System(QuitInput, .phase = OnUpdate) {
    cel_run {
        const struct NCurses_InputState* input = cel_read(NCurses_InputState);
        if (!input) return;
        for (int i = 0; i < input->key_count; i++) {
            if (input->keys[i] == 'q' || input->keys[i] == 'Q') {
                cels_quit();
                return;
            }
        }
    }
}

/* ============================================================================
 * Root Composition -- NCurses window + ClaySurface + shared UI
 * ============================================================================
 *
 * NCursesWindow creates the terminal session. ClaySurface dimensions use
 * aspect-ratio-compensated width (cols / 2.0) because Clay's coordinate
 * system maps to terminal cells via the NCurses renderer's 2:1 aspect
 * ratio compensation.
 *
 * For this minimal example, fixed dimensions (80x24) target a standard
 * terminal. A production app would watch NCurses_WindowState for resize.
 */

CEL_Compose(NCursesApp) {
    NCursesWindow(.title = "cels-clay example", .fps = 30) {
        /* Terminal dimensions: width / 2.0 for aspect ratio compensation.
         * The NCurses renderer scales horizontal values by cell_aspect_ratio
         * (default 2.0), so Clay width = terminal cols / 2.0.
         * Height maps 1:1 to terminal rows.
         * cel_watch makes this reactive — recomposes on terminal resize. */
        const struct NCurses_WindowState* ws = cel_read(NCurses_WindowState);
        float w = ws && ws->width > 0 ? (float)ws->width / 2.0f : 40.0f;
        float h = ws && ws->height > 0 ? (float)ws->height : 30.0f;
        ClaySurface(.width = w, .height = h) {
            build_ui();
        }
    }
}

/* ============================================================================
 * Application Entry Point
 * ============================================================================
 *
 * Registers three modules:
 *   1. NCurses     -- terminal session, input, frame pipeline
 *   2. Clay_Engine -- Clay arena, layout system, render bridge
 *   3. Clay_NCurses -- text measurement + terminal render provider
 *
 * Then enters the session loop with the NCursesApp root composition.
 */

cels_main() {
    cels_register(NCurses, Clay_Engine, Clay_NCurses);
    cels_register(QuitInput);

    cels_session(NCursesApp) {
        while (cels_running()) {
            cels_step(0);
        }
    }
}
