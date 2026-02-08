/*
 * Clay Layout System - Infrastructure Implementation
 *
 * Implements the foundational infrastructure for the CELS-Clay layout system:
 * - Component registration (ClayUI, ClaySurfaceConfig)
 * - Per-frame bump arena for dynamic string lifetime management
 * - Terminal text measurement function (character-cell based)
 * - Auto-ID generation via Clay__HashNumber(counter, entity_id)
 * - Layout pass state globals (used by tree walk in Plan 02)
 *
 * The layout system tree walk and system registration are stubs here,
 * to be implemented in Plan 02.
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 */

#include "cels-clay/clay_layout.h"
#include "clay.h"
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

static struct ecs_world_t* g_layout_world = NULL;
static cels_entity_t g_layout_current_entity = 0;
static bool g_layout_pass_active = false;

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
 * Stubs for Plan 02
 * ============================================================================ */

void _cel_clay_emit_children(void) {
    /* Stub: tree walk child emission implemented in Plan 02 */
    (void)g_layout_world;
    (void)g_layout_current_entity;
}

void _cel_clay_layout_system_register(void) {
    /* Stub: layout system registration implemented in Plan 02 */
    (void)_cel_clay_frame_arena_reset;
}
