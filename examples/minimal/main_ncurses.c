/*
 * cels-clay Minimal Example -- NCurses Backend
 *
 * Demonstrates the same entity tree from ui.h rendering in a terminal.
 * The only difference from main_sdl3.c is the module registration and
 * window setup -- the UI composition (build_ui) is identical.
 *
 * Build: cmake -DCELS_CLAY_BUILD_EXAMPLES=ON ..
 * Run:   ./main_ncurses
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
                cels_request_quit();
                return;
            }
        }
    }
}

/* ============================================================================
 * Root Composition -- NCurses window + ClaySurface + shared UI
 * ============================================================================
 *
 * cel_watch(NCurses_WindowState) makes this composition reactive --
 * it recomposes whenever the terminal resizes, updating ClaySurface
 * dimensions automatically.
 *
 * Width divided by 2.0 for aspect ratio: the renderer multiplies
 * horizontal coordinates by cell_aspect_ratio (2.0) to compensate
 * for terminal cells being ~2x taller than wide.
 */

CEL_Compose(NCursesApp) {
    NCursesWindow(.title = "cels-clay example", .fps = 30) {
        const struct NCurses_WindowState* ws = cel_watch(NCurses_WindowState);
        if (!ws) return;

        float w = ws->width > 0 ? (float)ws->width / 2.0f : 40.0f;
        float h = ws->height > 0 ? (float)ws->height : 24.0f;

        ClaySurface(.width = w, .height = h) {
            build_ui();
        }
    }
}

/* ============================================================================
 * Application Entry Point
 * ============================================================================ */

cels_main() {
    cels_register(NCurses, Clay_Engine, Clay_NCurses);
    cels_register(QuitInput);

    cels_session(NCursesApp) {
        while (cels_running()) cels_step(0);
    }
}
