/*
 * Clay Render Bridge - CELS Feature/Provider Interface for Clay Render Commands
 *
 * Bridges the Clay layout system output (Clay_RenderCommandArray) to renderer
 * backends via the CELS Feature/Provider pattern. The render dispatch system
 * runs at OnStore phase (after layout at PreStore), updating a singleton
 * ClayRenderTarget entity with current render commands each frame.
 *
 * Backend registration:
 *   _CEL_Provides(MyBackend, ClayRenderable, ClayRenderableData, my_renderer);
 *
 * Advanced users (custom systems):
 *   Clay_RenderCommandArray cmds = cel_clay_get_render_commands();
 */

#ifndef CELS_CLAY_RENDER_H
#define CELS_CLAY_RENDER_H

#include <cels/cels.h>
#include "clay.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * ClayRenderableData Component
 * ============================================================================
 *
 * Attached to the singleton ClayRenderTarget entity. Updated each frame by
 * the render dispatch system with current render commands, layout dimensions,
 * frame metadata, and dirty flag. Backends query this via _CEL_Provides.
 */
typedef struct ClayRenderableData {
    Clay_RenderCommandArray render_commands;
    float layout_width;
    float layout_height;
    uint32_t frame_number;
    float delta_time;
    bool dirty;
} ClayRenderableData;

extern cels_entity_t ClayRenderableDataID;
extern void ClayRenderableData_register(void);

/* ============================================================================
 * Public Getter API
 * ============================================================================
 *
 * For advanced users who want raw render commands without the Feature/Provider
 * pattern. Returns the most recent Clay_RenderCommandArray from the layout pass.
 */
extern Clay_RenderCommandArray cel_clay_get_render_commands(void);

/* ============================================================================
 * Render Bridge Lifecycle
 * ============================================================================
 *
 * Called from clay_engine.c during module initialization. Not for direct
 * consumer use.
 *
 * _cel_clay_render_init: Creates singleton entity, registers component, declares feature.
 * _cel_clay_render_system_register: Registers dispatch system at OnStore phase.
 */
extern void _cel_clay_render_init(void);
extern void _cel_clay_render_system_register(void);

/* ClayRenderable feature register function (non-static for cross-TU visibility
 * in INTERFACE library). Used by clay_ncurses_renderer.c via _CEL_Provides. */
extern void ClayRenderable_register(void);

/* ============================================================================
 * CelClayBorderDecor - Renderer-drawn border decoration
 * ============================================================================
 *
 * Passed via Clay element .userData to request the renderer draw a TUI
 * border (box-drawing characters) at the RECTANGLE edges, plus an optional
 * title-in-border on the top line.
 *
 * This bypasses Clay's own border system (which uses uint16_t widths that
 * get AR-scaled to 2+ cells horizontally). The renderer draws 1-cell-wide
 * border characters directly using tui_draw_border, matching the draw_test
 * pattern for tight, correct TUI borders.
 *
 * Usage in layout functions:
 *   static CelClayBorderDecor decor = { .title = "Panel Title", ... };
 *   CEL_Clay(.userData = &decor, ...) { ... }
 */
typedef struct CelClayBorderDecor {
    const char* title;          /* Title text for top border line (NULL = none) */
    const char* right_text;     /* Right-aligned text on top border (e.g., "[X]") */
    Clay_Color border_color;    /* Border line fg color */
    Clay_Color title_color;     /* Title text fg color */
    Clay_Color right_color;     /* Right-aligned text fg color */
    Clay_Color bg_color;        /* Background color (border bg + title bg) */
    uint8_t border_style;       /* 0=rounded, 1=single, 2=double */
    uintptr_t title_text_attr;  /* Packed CEL_TextAttr for title (0 = normal) */
} CelClayBorderDecor;

#endif /* CELS_CLAY_RENDER_H */
