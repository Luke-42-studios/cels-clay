/*
 * cels-clay Minimal Example -- SDL3 Backend
 *
 * Demonstrates the same entity tree from ui.h rendering in a graphical window.
 * The only difference from main_ncurses.c is the module registration and
 * window setup -- the UI composition (build_ui) is identical.
 *
 * Build: cmake -DCELS_CLAY_BUILD_EXAMPLES=ON ..
 * Run:   ./cels-clay-example-sdl3
 * Quit:  Close the window (SDL_QUIT event handled by cels-sdl3)
 */

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <cels-clay/clay_engine.h>
#include <cels-clay/clay_sdl3_renderer.h>
#include <cels-clay/clay_primitives.h>
#include <cels-clay/clay_layout.h>
#include "ui.h"

/* ============================================================================
 * Root Composition -- SDL3 context + window + ClaySurface + shared UI
 * ============================================================================
 *
 * SDL3Context initializes the SDL3 subsystem. SDL3Window creates the
 * graphical window. ClaySurface uses pixel dimensions for 1:1 mapping.
 *
 * The Clay SDL3 renderer is configured with a font file before module
 * registration. Text rendering requires a TTF font on disk.
 */

CEL_Compose(SDL3App) {
    SDL3Context(.video = true) {
        SDL3Window(.title = "cels-clay example", .width = 1280, .height = 720) {
            /* ClaySurface uses pixel dimensions for SDL3.
             * Match window size for 1:1 pixel mapping. */
            ClaySurface(.width = 1280.0f, .height = 720.0f) {
                build_ui();
            }
        }
    }
}

/* ============================================================================
 * Application Entry Point
 * ============================================================================
 *
 * Configures the SDL3 renderer with a font path, then registers three modules:
 *   1. SDL3_Engine -- SDL3 subsystem, window management, event loop
 *   2. Clay_Engine -- Clay arena, layout system, render bridge
 *   3. Clay_SDL3   -- text measurement + graphical render provider
 *
 * Then enters the session loop with the SDL3App root composition.
 * SDL3 handles quit via window close (SDL_QUIT event).
 */

cels_main() {
    /* Configure SDL3 renderer with a font for text rendering.
     * The font path is relative to the working directory. */
    Clay_SDL3_configure(&(ClaySDL3Config){
        .font_path = "assets/font.ttf",
        .font_size = 16
    });

    cels_register(SDL3_Engine, Clay_Engine, Clay_SDL3);

    cels_session(SDL3App) {
        while (cels_running()) {
            cels_step(0);
        }
    }
}
