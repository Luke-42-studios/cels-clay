/*
 * Clay Render Bridge - Implementation
 *
 * Implements the render bridge subsystem:
 * - ClayRenderable Feature definition at OnStore phase
 * - ClayRenderableData component registration
 * - Singleton ClayRenderTarget entity creation
 * - Render dispatch system that updates the singleton each frame
 * - Public getter API for advanced users
 *
 * Phase ordering:
 *   PreStore  -> ClayLayoutSystem (BeginLayout -> tree walk -> EndLayout)
 *   OnStore   -> ClayRenderDispatch (copies commands into ClayRenderableData)
 *            -> [Provider system] (backend reads ClayRenderableData, draws)
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 */

#include "cels-clay/clay_render.h"
#include "cels-clay/clay_layout.h"
#include "clay.h"
#include <cels/cels.h>
#include <flecs.h>
#include <stdio.h>
#include <stdbool.h>

/* ============================================================================
 * Feature Definition (file scope)
 * ============================================================================
 *
 * Defines the ClayRenderable feature at OnStore phase. This creates static
 * variables used by _CEL_Feature and _CEL_Provides. Matches the 3-step
 * pattern from cels-ncurses/src/renderer/tui_renderer.c.
 */
_CEL_DefineFeature(ClayRenderable, .phase = CELS_Phase_OnStore, .priority = 0);

/* ============================================================================
 * Component Registration
 * ============================================================================ */

cels_entity_t ClayRenderableDataID = 0;

cels_entity_t ClayRenderableData_ensure(void) {
    if (ClayRenderableDataID == 0) {
        ClayRenderableDataID = cels_component_register("ClayRenderableData",
            sizeof(ClayRenderableData), CELS_ALIGNOF(ClayRenderableData));
    }
    return ClayRenderableDataID;
}

/* ============================================================================
 * Static State
 * ============================================================================ */

static cels_entity_t g_render_target = 0;
static uint32_t g_frame_number = 0;

/* ============================================================================
 * Render Dispatch System
 * ============================================================================
 *
 * Runs each frame at OnStore phase. Reads the most recent render commands
 * and layout dimensions from the layout subsystem, packages them into
 * ClayRenderableData, and updates the singleton entity's component.
 *
 * Registered BEFORE providers are finalized (providers are created lazily
 * on first Engine_Progress), so this system runs first within OnStore.
 */
static void ClayRenderDispatch_callback(ecs_iter_t* it) {
    g_frame_number++;

    ecs_world_t* world = cels_get_world(cels_get_context());
    Clay_RenderCommandArray commands = _cel_clay_get_render_commands();
    Clay_Dimensions dims = _cel_clay_get_layout_dimensions();

    ClayRenderableData data = {
        .render_commands = commands,
        .layout_width = dims.width,
        .layout_height = dims.height,
        .frame_number = g_frame_number,
        .delta_time = it->delta_time,
        .dirty = (commands.length > 0)
    };

    ecs_set_id(world, g_render_target, ClayRenderableDataID,
               sizeof(ClayRenderableData), &data);
}

/* ============================================================================
 * Public Getter API
 * ============================================================================
 *
 * For advanced users who want raw render commands without Feature/Provider.
 * Delegates to the layout subsystem's internal getter.
 */
Clay_RenderCommandArray cel_clay_get_render_commands(void) {
    return _cel_clay_get_render_commands();
}

/* ============================================================================
 * Init (called from clay_engine.c during module init)
 * ============================================================================
 *
 * Creates the singleton render target entity and declares the Feature
 * relationship. Must be called AFTER Clay is initialized and components
 * are available.
 */
void _cel_clay_render_init(void) {
    ClayRenderableData_ensure();

    ecs_world_t* world = cels_get_world(cels_get_context());

    g_render_target = ecs_entity_init(world, &(ecs_entity_desc_t){
        .name = "ClayRenderTarget"
    });

    ClayRenderableData initial = {0};
    ecs_set_id(world, g_render_target, ClayRenderableDataID,
               sizeof(ClayRenderableData), &initial);

    _CEL_Feature(ClayRenderableData, ClayRenderable);
}

/* ============================================================================
 * System Registration (called from clay_engine.c BEFORE providers finalize)
 * ============================================================================
 *
 * Registers ClayRenderDispatch at OnStore phase using direct ecs_system_init.
 * This matches the pattern in clay_layout.c for standalone systems with zero
 * component query terms.
 */
void _cel_clay_render_system_register(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());

    ecs_system_desc_t sys_desc = {0};
    ecs_entity_desc_t entity_desc = {0};
    entity_desc.name = "ClayRenderDispatch";

    ecs_id_t phase_ids[3] = {
        ecs_pair(EcsDependsOn, EcsOnStore),
        EcsOnStore,
        0
    };
    entity_desc.add = phase_ids;

    sys_desc.entity = ecs_entity_init(world, &entity_desc);
    sys_desc.callback = ClayRenderDispatch_callback;

    ecs_system_init(world, &sys_desc);
}
