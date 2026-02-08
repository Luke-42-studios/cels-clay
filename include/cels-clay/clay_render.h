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
extern cels_entity_t ClayRenderableData_ensure(void);

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

/* ClayRenderable feature ensure function (non-static for cross-TU visibility
 * in INTERFACE library). Used by clay_ncurses_renderer.c via _CEL_Provides. */
extern cels_entity_t ClayRenderable_ensure(void);

#endif /* CELS_CLAY_RENDER_H */
