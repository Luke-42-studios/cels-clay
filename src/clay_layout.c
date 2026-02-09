/*
 * Clay Layout System - Complete Implementation
 *
 * Implements the CELS-Clay layout system:
 * - Component registration (ClayUI, ClaySurfaceConfig)
 * - Per-frame bump arena for dynamic string lifetime management
 * - Terminal text measurement function (character-cell based)
 * - Auto-ID generation via Clay__HashNumber(counter, entity_id)
 * - Depth-first entity tree walk with transparent pass-through
 * - CEL_Clay_Children child emission at call site
 * - PreStore layout system: SetDimensions -> arena reset -> BeginLayout -> walk -> EndLayout
 * - Render command storage for Phase 3 render bridge
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 */

#include "cels-clay/clay_layout.h"
#include "clay.h"
#include <flecs.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================================
 * Forward declaration for Clay internal
 * ============================================================================
 *
 * Clay__HashNumber is defined in the CLAY_IMPLEMENTATION section of clay.h
 * (compiled via clay_impl.c) but lacks a declaration in the header section.
 * The linker symbol exists -- we just need the declaration.
 */
extern Clay_ElementId Clay__HashNumber(const uint32_t offset, const uint32_t seed);

/* ============================================================================
 * Component Registration
 * ============================================================================
 *
 * Uses cels_component_register() (NOT direct flecs API) to match the
 * pattern from cels.h CEL_Define. Components use extern linkage for
 * cross-file access within the module.
 */

cels_entity_t ClayUIID = 0;
cels_entity_t ClaySurfaceConfigID = 0;

cels_entity_t ClayUI_ensure(void) {
    if (ClayUIID == 0) {
        ClayUIID = cels_component_register("ClayUI",
            sizeof(ClayUI), CELS_ALIGNOF(ClayUI));
    }
    return ClayUIID;
}

cels_entity_t ClaySurfaceConfig_ensure(void) {
    if (ClaySurfaceConfigID == 0) {
        ClaySurfaceConfigID = cels_component_register("ClaySurfaceConfig",
            sizeof(ClaySurfaceConfig), CELS_ALIGNOF(ClaySurfaceConfig));
    }
    return ClaySurfaceConfigID;
}

/* ============================================================================
 * Frame Arena (per-frame bump allocator for dynamic strings)
 * ============================================================================
 *
 * Dynamic strings (snprintf results, concatenated text) are stack-local in
 * layout functions. Clay_String stores only a pointer -- not a copy. The
 * frame arena copies dynamic strings into a persistent buffer that survives
 * until the renderer reads them. Reset at the start of each frame.
 */

typedef struct {
    char* memory;
    size_t capacity;
    size_t offset;
} _CelClayFrameArena;

static _CelClayFrameArena g_frame_arena = {0};
#define CEL_CLAY_FRAME_ARENA_SIZE (16 * 1024)  /* 16KB */

Clay_String _cel_clay_frame_arena_string(const char* buf, int32_t len) {
    Clay_String empty = { .isStaticallyAllocated = false, .length = 0, .chars = "" };

    if (len <= 0 || buf == NULL) {
        return empty;
    }

    if (g_frame_arena.memory == NULL) {
        fprintf(stderr, "[cels-clay] frame arena not initialized\n");
        return empty;
    }

    if (g_frame_arena.offset + (size_t)len > g_frame_arena.capacity) {
        fprintf(stderr, "[cels-clay] frame arena overflow: need %d bytes, "
                "%zu/%zu used\n", len, g_frame_arena.offset, g_frame_arena.capacity);
        return empty;
    }

    char* dest = g_frame_arena.memory + g_frame_arena.offset;
    memcpy(dest, buf, (size_t)len);
    g_frame_arena.offset += (size_t)len;

    return (Clay_String){
        .isStaticallyAllocated = false,
        .length = len,
        .chars = dest
    };
}

static void _cel_clay_frame_arena_reset(void) {
    g_frame_arena.offset = 0;
}

/* ============================================================================
 * Text Measurement (terminal character-cell based)
 * ============================================================================
 *
 * Each character is 1 unit wide, newlines increment height. Matches Clay's
 * terminal renderer example pattern. Registered via Clay_SetMeasureTextFunction
 * during layout init.
 *
 * Callback signature uses Clay_StringSlice (verified for Clay v0.14).
 */

static Clay_Dimensions _cel_clay_measure_text(
    Clay_StringSlice text,
    Clay_TextElementConfig* config,
    void* userData)
{
    (void)config;
    (void)userData;

    float max_width = 0;
    float line_width = 0;
    float height = 1;

    for (int32_t i = 0; i < text.length; i++) {
        if (text.chars[i] == '\n') {
            if (line_width > max_width) max_width = line_width;
            line_width = 0;
            height++;
        } else {
            line_width++;
        }
    }

    if (line_width > max_width) max_width = line_width;

    return (Clay_Dimensions){ .width = max_width, .height = height };
}

/* ============================================================================
 * Layout Pass State
 * ============================================================================
 *
 * Globals used during the entity tree walk (Plan 02). The layout system
 * sets g_layout_world and g_layout_current_entity before calling each
 * entity's layout function. CEL_Clay_Children() reads these to recurse.
 */

static ecs_world_t* g_layout_world = NULL;
static cels_entity_t g_layout_current_entity = 0;
static bool g_layout_pass_active = false;
static Clay_RenderCommandArray g_last_render_commands = {0};
static Clay_Dimensions g_last_layout_dimensions = {0, 0};

/* Forward declarations for tree walk (mutually recursive) */
static void clay_walk_entity(ecs_world_t* world, ecs_entity_t entity);
static void clay_walk_children(ecs_world_t* world, ecs_entity_t parent);

/* ============================================================================
 * Auto-ID Generation
 * ============================================================================
 *
 * Uses __COUNTER__ value (passed from CEL_Clay macro) combined with the
 * current entity ID as seed. Entity IDs are unique in flecs, __COUNTER__
 * is unique per macro expansion site within a translation unit.
 */

Clay_ElementId _cel_clay_auto_id(uint32_t counter) {
    uint32_t seed = (uint32_t)g_layout_current_entity;
    /* Scramble entity ID with golden ratio hash to avoid Clay__HashNumber
     * weakness where small sequential (counter, seed) pairs collide */
    seed *= 2654435761u;
    return Clay__HashNumber(counter, seed);
}

bool _cel_clay_layout_active(void) {
    return g_layout_pass_active;
}

/* ============================================================================
 * Init and Cleanup
 * ============================================================================ */

void _cel_clay_layout_init(void) {
    /* Allocate frame arena */
    g_frame_arena.memory = (char*)malloc(CEL_CLAY_FRAME_ARENA_SIZE);
    if (g_frame_arena.memory == NULL) {
        fprintf(stderr, "[cels-clay] failed to allocate frame arena (%d bytes)\n",
                CEL_CLAY_FRAME_ARENA_SIZE);
        return;
    }
    g_frame_arena.capacity = CEL_CLAY_FRAME_ARENA_SIZE;
    g_frame_arena.offset = 0;

    /* Register text measurement function */
    Clay_SetMeasureTextFunction(_cel_clay_measure_text, NULL);

    /* Ensure components are registered */
    ClayUI_ensure();
    ClaySurfaceConfig_ensure();
}

void _cel_clay_layout_cleanup(void) {
    if (g_frame_arena.memory != NULL) {
        free(g_frame_arena.memory);
        g_frame_arena.memory = NULL;
    }
    g_frame_arena.capacity = 0;
    g_frame_arena.offset = 0;

    g_layout_world = NULL;
    g_layout_current_entity = 0;
    g_layout_pass_active = false;
}

/* ============================================================================
 * Entity Tree Walk (depth-first, recursive)
 * ============================================================================
 *
 * Walks the CELS entity hierarchy and calls layout functions for entities
 * with ClayUI components. Non-ClayUI entities are transparent pass-throughs:
 * their children still participate in the layout tree.
 *
 * Current entity is saved/restored for nested CEL_Clay_Children calls.
 */

static void clay_walk_entity(ecs_world_t* world, ecs_entity_t entity) {
    const ClayUI* layout = (const ClayUI*)ecs_get_id(
        world, entity, ClayUIID);

    /* Save/restore current entity for nested CEL_Clay_Children calls */
    ecs_entity_t prev_entity = g_layout_current_entity;
    g_layout_current_entity = entity;

    if (layout && layout->layout_fn) {
        /* Entity has a layout function -- call it.
         * The function body contains CEL_Clay() and CEL_Clay_Children() calls.
         * Developer MUST call CEL_Clay_Children() to emit children --
         * children are NOT auto-appended (they'd be outside the CLAY scope). */
        layout->layout_fn(world, entity);
    } else {
        /* No ClayUI component -- transparent pass-through.
         * Walk children directly so Clay children still participate. */
        clay_walk_children(world, entity);
    }

    g_layout_current_entity = prev_entity;
}

static void clay_walk_children(ecs_world_t* world, ecs_entity_t parent) {
    ecs_iter_t it = ecs_children(world, parent);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            clay_walk_entity(world, it.entities[i]);
        }
    }
}

/* ============================================================================
 * CEL_Clay_Children Implementation
 * ============================================================================
 *
 * Called from within layout functions via the CEL_Clay_Children() macro.
 * Emits child entities at the current point in the CLAY tree, giving
 * the developer control over WHERE children appear in the layout.
 */

void _cel_clay_emit_children(void) {
    if (!g_layout_pass_active || g_layout_world == NULL) {
        fprintf(stderr, "[cels-clay] CEL_Clay_Children() called outside layout pass\n");
        return;
    }
    clay_walk_children(g_layout_world, g_layout_current_entity);
}

/* ============================================================================
 * Render Command Storage
 * ============================================================================
 *
 * After Clay_EndLayout(), render commands are stored for the render bridge
 * (Phase 3) to consume. Accessible via _cel_clay_get_render_commands().
 */

Clay_RenderCommandArray _cel_clay_get_render_commands(void) {
    return g_last_render_commands;
}

Clay_Dimensions _cel_clay_get_layout_dimensions(void) {
    return g_last_layout_dimensions;
}

/* ============================================================================
 * Layout System (PreStore phase)
 * ============================================================================
 *
 * Runs each frame at PreStore phase. For each ClaySurface entity:
 * 1. Set Clay layout dimensions from ClaySurfaceConfig
 * 2. Reset frame arena for dynamic strings
 * 3. Clay_BeginLayout()
 * 4. Walk children of the surface entity (depth-first tree walk)
 * 5. Clay_EndLayout() -> store render commands
 *
 * Uses ecs_iter_t* callback (direct flecs system, matching cels-ncurses pattern).
 */

static void ClayLayoutSystem_callback(ecs_iter_t* it) {
    (void)it;

    ecs_world_t* world = cels_get_world(cels_get_context());

    /* Find ClaySurface entities by querying for ClaySurfaceConfig */
    ecs_iter_t surface_it = ecs_each_id(world, ClaySurfaceConfigID);
    while (ecs_each_next(&surface_it)) {
        for (int i = 0; i < surface_it.count; i++) {
            ecs_entity_t surface = surface_it.entities[i];
            const ClaySurfaceConfig* config = (const ClaySurfaceConfig*)
                ecs_get_id(world, surface, ClaySurfaceConfigID);
            if (!config) continue;

            /* Skip layout if dimensions are too small */
            if (config->width < 2.0f || config->height < 2.0f) continue;

            /* 1. Set layout dimensions (reset text cache on resize) */
            {
                static float prev_w = 0, prev_h = 0;
                if (config->width != prev_w || config->height != prev_h) {
                    fprintf(stderr, "[clay-layout] dims: %.0f x %.0f -> %.0f x %.0f\n",
                            prev_w, prev_h, config->width, config->height);
                    /* Reset text cache on actual resize (not initial 0→real) */
                    if (prev_w > 0 && prev_h > 0) {
                        Clay_ResetMeasureTextCache();
                    }
                    prev_w = config->width;
                    prev_h = config->height;
                }
            }
            Clay_SetLayoutDimensions((Clay_Dimensions){
                .width = config->width,
                .height = config->height
            });
            g_last_layout_dimensions = (Clay_Dimensions){
                .width = config->width,
                .height = config->height
            };

            /* 2. Reset frame arena for this pass */
            _cel_clay_frame_arena_reset();

            /* 3. Begin layout pass */
            Clay_BeginLayout();
            g_layout_world = world;
            g_layout_pass_active = true;

            /* 4. Walk children of the ClaySurface entity */
            {
                static cels_entity_t prev_surface = 0;
                if (surface != prev_surface) {
                    fprintf(stderr, "[clay-layout] surface entity: %llu\n",
                            (unsigned long long)surface);
                    prev_surface = surface;
                }
                /* Count children for debug */
                int child_count = 0;
                ecs_iter_t dbg_it = ecs_children(world, surface);
                while (ecs_children_next(&dbg_it)) child_count += dbg_it.count;
                static int prev_cc = -1;
                if (child_count != prev_cc) {
                    fprintf(stderr, "[clay-layout] surface children: %d\n", child_count);
                    prev_cc = child_count;
                }
            }
            clay_walk_children(world, surface);

            /* 5. End layout pass */
            g_layout_pass_active = false;
            g_layout_world = NULL;
            g_layout_current_entity = 0;

            g_last_render_commands = Clay_EndLayout();
        }
    }
}

/* ============================================================================
 * System Registration
 * ============================================================================
 *
 * Registers ClayLayoutSystem at PreStore phase using direct ecs_system_init.
 * This matches the cels-ncurses pattern (tui_input.c, tui_frame.c) for
 * standalone systems with zero component query terms.
 */

void _cel_clay_layout_system_register(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());

    ecs_system_desc_t sys_desc = {0};
    ecs_entity_desc_t entity_desc = {0};
    entity_desc.name = "ClayLayoutSystem";

    /* Register at PreStore phase using flecs phase pairs */
    ecs_id_t phase_ids[3] = {
        ecs_pair(EcsDependsOn, EcsPreStore),
        EcsPreStore,
        0
    };
    entity_desc.add = phase_ids;

    sys_desc.entity = ecs_entity_init(world, &entity_desc);
    sys_desc.callback = ClayLayoutSystem_callback;

    ecs_system_init(world, &sys_desc);
}
