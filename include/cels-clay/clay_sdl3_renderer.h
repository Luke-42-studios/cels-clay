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
 * Clay SDL3 Renderer Module - Graphical renderer for Clay render commands
 *
 * Translates Clay_RenderCommandArray into graphical output using the
 * SDL3 rendering API and SDL3_ttf for text. Registered as a CEL_Module
 * for automatic initialization via cels_register().
 *
 * The renderer handles 6 Clay command types:
 *   RECTANGLE     -> SDL_RenderFillRect (filled background, no corner radius v1)
 *   TEXT          -> TTF_CreateText + TTF_DrawRendererText (font rendering)
 *   BORDER        -> SDL_RenderFillRect per side (filled border edges)
 *   SCISSOR_START -> SDL_SetRenderClipRect push (nested clip regions)
 *   SCISSOR_END   -> SDL_SetRenderClipRect pop (restore parent clip)
 *   IMAGE         -> SDL_RenderTexture (user-provided SDL_Texture*)
 *
 * Corner radius rendering is deferred to v2 (requires SDL_RenderGeometry
 * or custom shader for smooth rounded corners).
 *
 * Usage:
 *   #include <cels-clay/clay_sdl3_renderer.h>
 *
 *   Clay_SDL3_configure(&(ClaySDL3Config){
 *       .font_path = "assets/font.ttf",
 *       .font_size = 16
 *   });
 *   cels_register(SDL3_Engine, Clay_Engine, Clay_SDL3);
 *
 * NOTE: This header does NOT include SDL3 headers. The config struct
 * uses only C types. SDL types are used internally in the renderer source.
 * Consumers include both cels-clay and cels-sdl3 modules independently.
 */

#ifndef CELS_CLAY_SDL3_RENDERER_H
#define CELS_CLAY_SDL3_RENDERER_H

#include <stdbool.h>
#include <cels/cels.h>
#include <SDL3/SDL.h>

/* ============================================================================
 * Module Declaration
 * ============================================================================ */

CEL_Module(Clay_SDL3);

/* ============================================================================
 * ClaySDL3Config - Renderer configuration
 * ============================================================================
 *
 * Controls font loading for text rendering. The font_path points to a .ttf
 * file on disk. SDL3_ttf loads it at module initialization time.
 */

/* Configuration for the SDL3 Clay renderer.
 * window:    SDL_Window* to create the renderer for. Required.
 * font_path: Path to a .ttf font file for text rendering.
 *            Required -- text rendering fails without a font.
 * font_size: Font size in points. Default: 16 if 0. */
typedef struct ClaySDL3Config {
    SDL_Window* window;
    const char* font_path;
    int font_size;
} ClaySDL3Config;

/* ============================================================================
 * Renderer API
 * ============================================================================ */

/* Configure SDL3 Clay renderer before module registration.
 * Call before cels_register(Clay_SDL3).
 * font_path is required for text rendering. */
extern void Clay_SDL3_configure(const ClaySDL3Config* config);

#endif /* CELS_CLAY_SDL3_RENDERER_H */
