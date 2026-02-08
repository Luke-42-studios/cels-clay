/*
 * Clay Engine Module - Implementation
 *
 * Initializes the Clay layout engine: arena allocation, error handler,
 * and cleanup. Uses _CEL_DefineModule for idempotent initialization.
 *
 * Usage:
 *   Clay_Engine_use(config) -- configure and initialize Clay
 *
 * The arena is allocated with at least Clay_MinMemorySize() bytes.
 * Consumer can override via Clay_EngineConfig.arena_size.
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 * NOTE: This file does NOT define CLAY_IMPLEMENTATION -- that's in clay_impl.c.
 */

#include "cels-clay/clay_engine.h"
#include "cels-clay/clay_layout.h"
#include "cels-clay/clay_render.h"
#include "clay.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* ============================================================================
 * Static State
 * ============================================================================ */

/* Module-level config (stored by Clay_Engine_use, read by init body) */
static Clay_EngineConfig g_clay_config = {0};

/* Original malloc pointer -- needed for free() because Clay aligns
 * arena.memory to a 64-byte cache-line boundary, so arena.memory != malloc result */
static void* g_clay_arena_memory = NULL;

/* Clay context pointer -- stored for future multi-context support */
static Clay_Context* g_clay_context = NULL;

/* ============================================================================
 * Error Handler
 * ============================================================================ */

static void clay_error_handler(Clay_ErrorData error) {
    const char* type_str = "unknown";
    switch (error.errorType) {
        case CLAY_ERROR_TYPE_TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED:
            type_str = "text measurement function not provided"; break;
        case CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED:
            type_str = "arena capacity exceeded"; break;
        case CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED:
            type_str = "elements capacity exceeded"; break;
        case CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED:
            type_str = "text measurement capacity exceeded"; break;
        case CLAY_ERROR_TYPE_DUPLICATE_ID:
            type_str = "duplicate ID"; break;
        case CLAY_ERROR_TYPE_FLOATING_CONTAINER_PARENT_NOT_FOUND:
            type_str = "floating container parent not found"; break;
        case CLAY_ERROR_TYPE_PERCENTAGE_OVER_1:
            type_str = "percentage over 1"; break;
        case CLAY_ERROR_TYPE_INTERNAL_ERROR:
            type_str = "internal error"; break;
    }
    /* Clay_String is NOT null-terminated -- use %.*s with explicit length */
    fprintf(stderr, "[cels-clay] %s: %.*s\n",
            type_str, error.errorText.length, error.errorText.chars);
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

static void clay_cleanup(void) {
    _cel_clay_layout_cleanup();  /* Free frame arena (before Clay arena) */
    if (g_clay_arena_memory != NULL) {
        free(g_clay_arena_memory);
        g_clay_arena_memory = NULL;
    }
}

/* ============================================================================
 * Module Definition
 * ============================================================================ */

_CEL_DefineModule(Clay_Engine) {
    /* 1. Calculate arena size */
    uint32_t min_memory = Clay_MinMemorySize();
    uint32_t arena_size = min_memory;

    if (g_clay_config.arena_size > 0) {
        if (g_clay_config.arena_size >= min_memory) {
            arena_size = g_clay_config.arena_size;
        } else {
            fprintf(stderr, "[cels-clay] warning: requested arena_size %u "
                    "is less than Clay_MinMemorySize() %u, clamping to minimum\n",
                    (unsigned)g_clay_config.arena_size, (unsigned)min_memory);
            arena_size = min_memory;
        }
    }

    /* 2. Allocate arena memory */
    g_clay_arena_memory = malloc(arena_size);
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(
        arena_size, g_clay_arena_memory);

    /* 3. Initialize Clay */
    g_clay_context = Clay_Initialize(
        arena,
        (Clay_Dimensions){0, 0},
        (Clay_ErrorHandler){ .errorHandlerFunction = clay_error_handler }
    );

    /* 4. Initialize layout subsystem (frame arena, text measurement, components) */
    _cel_clay_layout_init();

    /* 5. Initialize render bridge (singleton entity, feature declaration) */
    _cel_clay_render_init();

    /* 6. Register systems in correct order:
     *    a) Layout at PreStore (runs first each frame)
     *    b) Render dispatch at OnStore (runs after layout, before providers) */
    _cel_clay_layout_system_register();
    _cel_clay_render_system_register();

    /* 7. Register cleanup for process exit */
    atexit(clay_cleanup);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void Clay_Engine_use(Clay_EngineConfig config) {
    g_clay_config = config;
    Clay_Engine_init();  /* idempotent via _CEL_DefineModule guard */
}
