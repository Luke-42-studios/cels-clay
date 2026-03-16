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
 * Clay SDL3 Renderer - Implementation
 *
 * Translates Clay_RenderCommandArray into graphical output using SDL3
 * rendering API and SDL3_ttf for text. Registers as a render backend
 * via cels_system_declare().
 *
 * Rendering approach:
 *   - SDL_Renderer created lazily from SDL3_WindowComponent.window
 *     (window may not exist at module init time)
 *   - TTF_CreateRendererTextEngine for SDL3_ttf text engine API
 *   - Scissor stack for nested clip region support
 *   - Corner radius: skipped for v1 (filled rects only)
 *
 * Anti-patterns avoided:
 *   - No font per text element (single font, size set per element)
 *   - No surface->texture pipeline for text (uses TTF text engine)
 *   - No manual color management (SDL handles RGBA natively)
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 *       It is conditionally included when the cels-sdl3 target exists.
 */

#include "cels-clay/clay_sdl3_renderer.h"
#include "cels-clay/clay_render.h"
#include "clay.h"
#include <cels/cels.h>
#include <cels_sdl3.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Static State
 * ============================================================================ */

static ClaySDL3Config g_sdl3_config = {0};
static SDL_Renderer* g_renderer = NULL;
static TTF_Font* g_font = NULL;
static TTF_TextEngine* g_text_engine = NULL;

/* ============================================================================
 * Scissor Stack
 * ============================================================================
 *
 * Maintains a stack of clip rectangles for nested SCISSOR_START/END pairs.
 * On push: intersect new rect with current, apply to renderer.
 * On pop: restore previous rect (or NULL if stack empty).
 */

#define MAX_SCISSOR_DEPTH 16
static SDL_Rect g_scissor_stack[MAX_SCISSOR_DEPTH];
static int g_scissor_depth = 0;

static void scissor_reset(void) {
    g_scissor_depth = 0;
    if (g_renderer) {
        SDL_SetRenderClipRect(g_renderer, NULL);
    }
}

static void scissor_push(SDL_Rect new_rect) {
    if (g_scissor_depth > 0 && g_scissor_depth <= MAX_SCISSOR_DEPTH) {
        /* Intersect with current top of stack */
        SDL_Rect current = g_scissor_stack[g_scissor_depth - 1];
        int x1 = SDL_max(new_rect.x, current.x);
        int y1 = SDL_max(new_rect.y, current.y);
        int x2 = SDL_min(new_rect.x + new_rect.w, current.x + current.w);
        int y2 = SDL_min(new_rect.y + new_rect.h, current.y + current.h);
        new_rect.x = x1;
        new_rect.y = y1;
        new_rect.w = SDL_max(0, x2 - x1);
        new_rect.h = SDL_max(0, y2 - y1);
    }

    if (g_scissor_depth < MAX_SCISSOR_DEPTH) {
        g_scissor_stack[g_scissor_depth] = new_rect;
        g_scissor_depth++;
    }

    SDL_SetRenderClipRect(g_renderer, &new_rect);
}

static void scissor_pop(void) {
    if (g_scissor_depth > 0) {
        g_scissor_depth--;
    }

    if (g_scissor_depth > 0) {
        SDL_SetRenderClipRect(g_renderer, &g_scissor_stack[g_scissor_depth - 1]);
    } else {
        SDL_SetRenderClipRect(g_renderer, NULL);
    }
}

/* ============================================================================
 * Lazy Renderer Initialization
 * ============================================================================
 *
 * SDL_CreateRenderer needs an SDL_Window* which may not exist at module
 * init time (depends on entity creation order). The render callback
 * checks if g_renderer is NULL and attempts initialization on first call.
 */

static bool ensure_renderer_initialized(void) {
    if (g_renderer) return true;

    /* The app must pass the SDL_Window* via Clay_SDL3_configure.
     * If not set, try to find one from the config's window pointer. */
    SDL_Window* window = g_sdl3_config.window;
    if (!window) return false;  /* No window available yet */

    g_renderer = SDL_CreateRenderer(window, NULL);
    if (!g_renderer) {
        SDL_Log("Clay_SDL3: SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    /* Enable alpha blending by default */
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    /* Create text engine for TTF rendering */
    g_text_engine = TTF_CreateRendererTextEngine(g_renderer);
    if (!g_text_engine) {
        SDL_Log("Clay_SDL3: TTF_CreateRendererTextEngine failed: %s",
                SDL_GetError());
    }

    /* Load TTF font */
    if (g_sdl3_config.font_path) {
        int size = g_sdl3_config.font_size > 0 ? g_sdl3_config.font_size : 16;
        g_font = TTF_OpenFont(g_sdl3_config.font_path, (float)size);
        if (!g_font) {
            SDL_Log("Clay_SDL3: TTF_OpenFont('%s') failed: %s",
                    g_sdl3_config.font_path, SDL_GetError());
        }
    } else {
        SDL_Log("Clay_SDL3: No font_path configured. "
                "Text rendering will be disabled.");
    }

    return true;
}

/* ============================================================================
 * Text Measurement Callback (pixel-based)
 * ============================================================================
 *
 * Provides pixel-accurate text dimensions for Clay_SetMeasureTextFunction.
 * Uses TTF_GetStringSize for width/height measurement in pixels.
 *
 * Unlike the NCurses renderer (which measures in cell columns), this
 * returns pixel dimensions directly. No aspect ratio compensation needed.
 */

static Clay_Dimensions clay_sdl3_measure_text(
    Clay_StringSlice text,
    Clay_TextElementConfig* config,
    void* userData)
{
    (void)userData;

    if (!g_font || text.length <= 0 || !text.chars) {
        return (Clay_Dimensions){0, 0};
    }

    /* Set font size to match the config (Clay may use different sizes) */
    if (config && config->fontSize > 0) {
        TTF_SetFontSize(g_font, (float)config->fontSize);
    }

    /* TTF_GetStringSize accepts length parameter, no null-termination needed */
    int w = 0, h = 0;
    TTF_GetStringSize(g_font, text.chars, (size_t)text.length, &w, &h);

    return (Clay_Dimensions){ .width = (float)w, .height = (float)h };
}

/* ============================================================================
 * Text Rendering Helper
 * ============================================================================
 *
 * Uses the SDL3_ttf text engine API (TTF_CreateText + TTF_DrawRendererText)
 * following the Clay reference renderer pattern. This is more efficient than
 * the surface->texture pipeline as the text engine caches glyphs internally.
 */

static void render_sdl3_text(Clay_RenderCommand* cmd) {
    Clay_TextRenderData* td = &cmd->renderData.text;
    Clay_StringSlice text = td->stringContents;
    if (text.length <= 0 || !text.chars || !g_font || !g_text_engine) return;

    /* Set font size for this text element */
    if (td->fontSize > 0) {
        TTF_SetFontSize(g_font, (float)td->fontSize);
    }

    /* Create text object via text engine */
    TTF_Text* ttf_text = TTF_CreateText(g_text_engine, g_font,
                                         text.chars, (size_t)text.length);
    if (!ttf_text) return;

    /* Set text color */
    Clay_Color c = td->textColor;
    TTF_SetTextColor(ttf_text,
                     (uint8_t)c.r, (uint8_t)c.g, (uint8_t)c.b, (uint8_t)c.a);

    /* Draw at bounding box position */
    TTF_DrawRendererText(ttf_text,
                         cmd->boundingBox.x, cmd->boundingBox.y);

    TTF_DestroyText(ttf_text);
}

/* ============================================================================
 * Border Rendering Helper
 * ============================================================================
 *
 * Draws borders as filled rectangles per side, following the Clay reference
 * renderer pattern. Each border side is a thin filled rect along the edge
 * of the bounding box with thickness from Clay_BorderRenderData.width.
 *
 * Corner radius rendering is skipped for v1. Rounded border corners
 * would require SDL_RenderGeometry arc segments (deferred to v2).
 */

static void render_sdl3_border(Clay_RenderCommand* cmd) {
    Clay_BorderRenderData* bd = &cmd->renderData.border;
    Clay_BoundingBox bb = cmd->boundingBox;
    Clay_Color c = bd->color;

    SDL_SetRenderDrawColor(g_renderer,
        (uint8_t)c.r, (uint8_t)c.g, (uint8_t)c.b, (uint8_t)c.a);

    float x = bb.x, y = bb.y, w = bb.width, h = bb.height;

    /* Draw each side as a filled rectangle (matching Clay reference renderer) */
    if (bd->width.top > 0) {
        SDL_FRect line = { x, y, w, (float)bd->width.top };
        SDL_RenderFillRect(g_renderer, &line);
    }
    if (bd->width.bottom > 0) {
        SDL_FRect line = { x, y + h - (float)bd->width.bottom,
                           w, (float)bd->width.bottom };
        SDL_RenderFillRect(g_renderer, &line);
    }
    if (bd->width.left > 0) {
        SDL_FRect line = { x, y, (float)bd->width.left, h };
        SDL_RenderFillRect(g_renderer, &line);
    }
    if (bd->width.right > 0) {
        SDL_FRect line = { x + w - (float)bd->width.right, y,
                           (float)bd->width.right, h };
        SDL_RenderFillRect(g_renderer, &line);
    }
}

/* ============================================================================
 * Render Callback
 * ============================================================================
 *
 * Main render system callback. Reads ClayRenderableData from the singleton
 * entity and iterates the Clay_RenderCommandArray, dispatching each command
 * to the appropriate SDL3 draw function.
 *
 * Called each frame at OnRender phase after ClayRenderDispatch has updated
 * the ClayRenderableData singleton.
 */

static void clay_sdl3_render(cels_iter_t* it) {
    (void)it;
    /* Read render commands directly from the layout system getter */
    Clay_RenderCommandArray cmds = cel_clay_get_render_commands();
    if (cmds.length <= 0) return;
    if (!ensure_renderer_initialized()) return;

    /* Reset scissor stack at start of each render pass */
    scissor_reset();

    {

        for (int32_t j = 0; j < cmds.length; j++) {
            Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&cmds, j);
            Clay_BoundingBox bb = cmd->boundingBox;
            SDL_FRect rect = {
                .x = bb.x, .y = bb.y,
                .w = bb.width, .h = bb.height
            };

            switch (cmd->commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                    /* Corner radius rendering deferred to v2.
                     * v2 would use SDL_RenderGeometry for smooth
                     * rounded corners via triangle fan tessellation. */
                    Clay_Color c = cmd->renderData.rectangle.backgroundColor;
                    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(g_renderer,
                        (uint8_t)c.r, (uint8_t)c.g,
                        (uint8_t)c.b, (uint8_t)c.a);
                    SDL_RenderFillRect(g_renderer, &rect);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                    render_sdl3_text(cmd);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                    render_sdl3_border(cmd);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                    SDL_Rect clip = {
                        .x = (int)bb.x, .y = (int)bb.y,
                        .w = (int)bb.width, .h = (int)bb.height
                    };
                    scissor_push(clip);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                    scissor_pop();
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                    /* Cast imageData to SDL_Texture* (user provides texture) */
                    if (cmd->renderData.image.imageData) {
                        SDL_Texture* tex =
                            (SDL_Texture*)cmd->renderData.image.imageData;
                        SDL_RenderTexture(g_renderer, tex, NULL, &rect);
                    }
                    break;
                }
                default:
                    break;  /* CUSTOM, NONE -- skip silently */
            }
        }
    }
}

/* ============================================================================
 * Module Definition
 * ============================================================================ */

CEL_Module(Clay_SDL3, init) {
    /* Register text measurement callback (pixel-based via TTF).
     * Font loading is deferred to ensure_renderer_initialized() since
     * the SDL_Window may not exist yet at module init time. However,
     * we attempt font loading here for the common case where SDL3
     * is already initialized. */
    if (g_sdl3_config.font_path) {
        int size = g_sdl3_config.font_size > 0
            ? g_sdl3_config.font_size : 16;
        g_font = TTF_OpenFont(g_sdl3_config.font_path, (float)size);
        if (!g_font) {
            /* Font load deferred to ensure_renderer_initialized */
            SDL_Log("Clay_SDL3: Font load deferred (TTF may not be ready)");
        }
    }

    Clay_SetMeasureTextFunction(clay_sdl3_measure_text, NULL);

    /* Register render system at OnRender phase */
    ClayRenderableData_register();
    cels_entity_t comp_ids[] = { ClayRenderableData_id };
    cels_system_declare("SDL3_ClayRenderable_ClayRenderableData",
                        OnRender, clay_sdl3_render, comp_ids, 1);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void Clay_SDL3_configure(const ClaySDL3Config* config) {
    if (config) g_sdl3_config = *config;
}
