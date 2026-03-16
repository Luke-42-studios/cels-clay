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
#include <flecs.h>
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
 * Resize System -- sync ClaySurface to terminal size each frame
 * ============================================================================
 *
 * Reads current terminal dimensions from NCurses_WindowState and updates
 * ClaySurfaceConfig on all surface entities. This handles initial sizing
 * (first frame after ncurses init) and terminal resize events.
 *
 * Width is divided by 2.0 (cell_aspect_ratio) because the renderer
 * multiplies horizontal coordinates by 2.0 to compensate for terminal
 * cells being ~2x taller than wide.
 */

CEL_System(SyncSurfaceSize, .phase = OnUpdate) {
    cel_run {
        const struct NCurses_WindowState* ws = cel_read(NCurses_WindowState);
        if (!ws || ws->width <= 0 || ws->height <= 0) return;

        float w = (float)ws->width / 2.0f;
        float h = (float)ws->height;

        /* Update all ClaySurfaceConfig entities */
        ecs_world_t* world = cels_get_world(cels_get_context());
        ecs_iter_t it = ecs_each_id(world, ClaySurfaceConfig_id);
        while (ecs_each_next(&it)) {
            for (int i = 0; i < it.count; i++) {
                ClaySurfaceConfig new_config = { .width = w, .height = h };
                ecs_set_id(world, it.entities[i], ClaySurfaceConfig_id,
                           sizeof(ClaySurfaceConfig), &new_config);
            }
        }
    }
}

/* ============================================================================
 * Root Composition -- NCurses window + ClaySurface + shared UI
 * ============================================================================ */

CEL_Compose(NCursesApp) {
    NCursesWindow(.title = "cels-clay example", .fps = 30) {
        /* Initial size — SyncSurfaceSize system updates this each frame */
        ClaySurface(.width = 40.0f, .height = 24.0f) {
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
    cels_register(QuitInput, SyncSurfaceSize);

    cels_session(NCursesApp) {
        while (cels_running()) {
            cels_step(0);
        }
    }
}
