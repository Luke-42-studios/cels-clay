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
 * Clay Layout System - Property-Driven Tree Walk
 *
 * Implements the CELS-Clay layout system:
 * - Component registration (ClaySurfaceConfig)
 * - Per-frame bump arena for dynamic string lifetime management
 * - Terminal text measurement function (character-cell based)
 * - Auto-ID generation via Clay__HashNumber(counter, entity_id)
 * - Depth-first entity tree walk with component-presence dispatch
 * - Property-driven emit: ClayContainerConfig, ClayTextConfig,
 *   ClaySpacerConfig, ClayImageConfig -> CLAY()/CLAY_TEXT() calls
 * - CEL_Clay_Children child emission at call site
 * - PreStore layout system: SetDimensions -> arena reset -> BeginLayout -> walk -> EndLayout
 * - Render command storage for render bridge
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 */

#include "cels-clay/clay_layout.h"
#include "cels-clay/clay_primitives.h"
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

cels_entity_t ClaySurfaceConfig_id = 0;

void ClaySurfaceConfig_register(void) {
    if (ClaySurfaceConfig_id == 0) {
        ClaySurfaceConfig_id = cels_component_register("ClaySurfaceConfig",
            sizeof(ClaySurfaceConfig), CELS_ALIGNOF(ClaySurfaceConfig));
    }
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
 * Globals used during the entity tree walk. The layout system sets
 * g_layout_world and g_layout_current_entity before walking each entity.
 * CEL_Clay_Children() reads these to recurse into children.
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
    ClaySurfaceConfig_register();
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
 * Sizing Helper
 * ============================================================================
 *
 * Zero-initialized Clay_SizingAxis has type CLAY__SIZING_TYPE_FIT with
 * min=0, max=0. This would collapse elements to zero size. Treat
 * zero-initialized axes as CLAY_SIZING_GROW(0) for sensible defaults.
 */

static Clay_SizingAxis _sizing_or_grow(Clay_SizingAxis axis) {
    if (axis.type == 0 && axis.size.minMax.min == 0.0f && axis.size.minMax.max == 0.0f) {
        return (Clay_SizingAxis)CLAY_SIZING_GROW(0);
    }
    return axis;
}

/* ============================================================================
 * Emit Functions (property-driven Clay element generation)
 * ============================================================================
 *
 * Each emit function reads a component struct from an entity and generates
 * the corresponding CLAY() or CLAY_TEXT() call. Called by clay_walk_entity()
 * based on which component is present.
 *
 * Auto-IDs use fixed discriminator values (0-3) combined with entity ID.
 * Since each entity has a unique ID, this produces unique Clay element IDs.
 */

static void emit_container(ecs_world_t* world, ecs_entity_t entity,
                            const ClayContainerConfig* config) {
    /* Check for additive ClayBorderStyle component */
    const ClayBorderStyle* border_style = (const ClayBorderStyle*)
        ecs_get_id(world, entity, ClayBorderStyle_id);

    CLAY(_cel_clay_auto_id(0), {
        .layout = {
            .layoutDirection = config->direction,
            .childGap = config->gap,
            .padding = config->padding,
            .sizing = {
                .width = _sizing_or_grow(config->width),
                .height = _sizing_or_grow(config->height)
            },
            .childAlignment = config->alignment
        },
        .backgroundColor = config->bg,
        .clip = config->clip
            ? (Clay_ClipElementConfig){ .horizontal = true, .vertical = true }
            : (Clay_ClipElementConfig){0},
        .border = border_style ? border_style->border : (Clay_BorderElementConfig){0},
        .cornerRadius = border_style ? border_style->corner_radius : (Clay_CornerRadius){0}
    }) {
        clay_walk_children(world, entity);
    }
}

static void emit_text(ecs_world_t* world, ecs_entity_t entity,
                       const ClayTextConfig* config) {
    (void)world; (void)entity;  /* text is a leaf node */
    if (!config->text) return;

    /* Convert const char* to Clay_String using the frame arena */
    int32_t len = (int32_t)strlen(config->text);
    Clay_String clay_str;
    if (len > 0) {
        clay_str = _cel_clay_frame_arena_string(config->text, len);
    } else {
        clay_str = (Clay_String){ .chars = "", .length = 0 };
    }

    CLAY_TEXT(clay_str, CLAY_TEXT_CONFIG({
        .textColor = config->color,
        .fontSize = config->font_size,
        .fontId = config->font_id,
        .letterSpacing = config->letter_spacing,
        .lineHeight = config->line_height,
        .wrapMode = config->wrap
    }));
}

static void emit_spacer(ecs_world_t* world, ecs_entity_t entity,
                          const ClaySpacerConfig* config) {
    (void)world; (void)entity;  /* spacer is a leaf node */
    CLAY(_cel_clay_auto_id(1), {
        .layout = {
            .sizing = {
                .width = _sizing_or_grow(config->width),
                .height = _sizing_or_grow(config->height)
            }
        }
    }) {}
}

static void emit_image(ecs_world_t* world, ecs_entity_t entity,
                         const ClayImageConfig* config) {
    (void)world; (void)entity;  /* image is a leaf node */
    CLAY(_cel_clay_auto_id(2), {
        .layout = {
            .sizing = {
                .width = _sizing_or_grow(config->width),
                .height = _sizing_or_grow(config->height)
            }
        },
        .image = {
            .imageData = config->source
        },
        .backgroundColor = config->bg,
        .cornerRadius = config->corner_radius
    }) {}
}

/* ============================================================================
 * Entity Tree Walk (depth-first, recursive, component-presence dispatch)
 * ============================================================================
 *
 * Walks the CELS entity hierarchy and dispatches by component presence:
 * - ClayContainerConfig -> emit CLAY() container with direction/props/children
 * - ClayTextConfig -> emit CLAY_TEXT() leaf
 * - ClaySpacerConfig -> emit CLAY() spacing leaf
 * - ClayImageConfig -> emit CLAY() image leaf
 * - No layout component -> transparent passthrough (walk children directly)
 *
 * Current entity is saved/restored for nested CEL_Clay_Children calls.
 */

static void clay_walk_entity(ecs_world_t* world, ecs_entity_t entity) {
    ecs_entity_t prev_entity = g_layout_current_entity;
    g_layout_current_entity = entity;

    /* Check component presence in priority order */
    const ClayContainerConfig* container = (const ClayContainerConfig*)
        ecs_get_id(world, entity, ClayContainerConfig_id);
    const ClayTextConfig* text = (const ClayTextConfig*)
        ecs_get_id(world, entity, ClayTextConfig_id);
    const ClaySpacerConfig* spacer = (const ClaySpacerConfig*)
        ecs_get_id(world, entity, ClaySpacerConfig_id);
    const ClayImageConfig* image = (const ClayImageConfig*)
        ecs_get_id(world, entity, ClayImageConfig_id);

    if (container) {
        emit_container(world, entity, container);
    } else if (text) {
        emit_text(world, entity, text);
    } else if (spacer) {
        emit_spacer(world, entity, spacer);
    } else if (image) {
        emit_image(world, entity, image);
    } else {
        /* No layout component -- transparent passthrough.
         * Walk children directly so they still participate in the Clay tree. */
        clay_walk_children(world, entity);
    }

    g_layout_current_entity = prev_entity;
}

/* Max children for stack-local sort buffer. Overflow falls back to unsorted. */
#define CEL_CLAY_MAX_SORTED_CHILDREN 128

typedef struct {
    ecs_entity_t entity;
    uint32_t order;
} _CelSortedChild;

static void clay_walk_children(ecs_world_t* world, ecs_entity_t parent) {
    _CelSortedChild buf[CEL_CLAY_MAX_SORTED_CHILDREN];
    int count = 0;

    /* Collect children with their sibling order */
    ecs_entity_t so_id = (ecs_entity_t)CELS_SIBLING_ORDER;
    ecs_iter_t it = ecs_children(world, parent);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            if (count >= CEL_CLAY_MAX_SORTED_CHILDREN) {
                /* Overflow: walk remaining unsorted */
                clay_walk_entity(world, it.entities[i]);
                continue;
            }
            const cels_sibling_order_t* so = (const cels_sibling_order_t*)
                ecs_get_id(world, it.entities[i], so_id);
            buf[count].entity = it.entities[i];
            buf[count].order = so ? so->order : (uint32_t)count;
            count++;
        }
    }

    /* Insertion sort by order (children count is typically < 20) */
    for (int i = 1; i < count; i++) {
        _CelSortedChild tmp = buf[i];
        int j = i - 1;
        while (j >= 0 && buf[j].order > tmp.order) {
            buf[j + 1] = buf[j];
            j--;
        }
        buf[j + 1] = tmp;
    }

    /* Walk in sorted order */
    for (int i = 0; i < count; i++) {
        clay_walk_entity(world, buf[i].entity);
    }
}

/* ============================================================================
 * CEL_Clay_Children Implementation
 * ============================================================================
 *
 * Called from within emit functions or advanced layout functions via the
 * CEL_Clay_Children() macro. Emits child entities at the current point
 * in the CLAY tree.
 */

void _cel_clay_emit_children(void) {
    if (!g_layout_pass_active || g_layout_world == NULL) {
        fprintf(stderr, "[cels-clay] CEL_Clay_Children() called outside layout pass\n");
        return;
    }
    clay_walk_children(g_layout_world, g_layout_current_entity);
}

/* Emit a range of children [start, start+count) in sibling order.
 * Used by scrollable containers for virtual rendering -- only visible
 * children get Clay elements created, avoiding element overflow.
 * Uses heap allocation when children exceed the stack sort buffer. */
void _cel_clay_emit_children_range(int start, int count) {
    if (!g_layout_pass_active || g_layout_world == NULL) {
        fprintf(stderr, "[cels-clay] CEL_Clay_ChildrenRange() called outside layout pass\n");
        return;
    }
    if (count <= 0) return;

    /* Collect all children with their sibling order */
    _CelSortedChild stack_buf[CEL_CLAY_MAX_SORTED_CHILDREN];
    _CelSortedChild* buf = stack_buf;
    int total = 0;
    bool heap = false;

    /* First pass: count children */
    ecs_entity_t so_id = (ecs_entity_t)CELS_SIBLING_ORDER;
    ecs_iter_t it = ecs_children(g_layout_world, g_layout_current_entity);
    while (ecs_children_next(&it)) {
        total += it.count;
    }

    /* Allocate heap buffer if needed */
    if (total > CEL_CLAY_MAX_SORTED_CHILDREN) {
        buf = (_CelSortedChild*)malloc(sizeof(_CelSortedChild) * total);
        if (!buf) return;
        heap = true;
    }

    /* Second pass: collect */
    int collected = 0;
    it = ecs_children(g_layout_world, g_layout_current_entity);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count && collected < total; i++) {
            const cels_sibling_order_t* so = (const cels_sibling_order_t*)
                ecs_get_id(g_layout_world, it.entities[i], so_id);
            buf[collected].entity = it.entities[i];
            buf[collected].order = so ? so->order : (uint32_t)collected;
            collected++;
        }
    }

    /* Insertion sort by sibling order */
    for (int i = 1; i < collected; i++) {
        _CelSortedChild tmp = buf[i];
        int j = i - 1;
        while (j >= 0 && buf[j].order > tmp.order) {
            buf[j + 1] = buf[j];
            j--;
        }
        buf[j + 1] = tmp;
    }

    /* Walk only the visible range */
    int end = start + count;
    if (end > collected) end = collected;
    for (int i = start; i < end; i++) {
        clay_walk_entity(g_layout_world, buf[i].entity);
    }

    if (heap) free(buf);
}

/* Emit a specific child entity by index (0-based, in sibling order).
 * Used by container widgets that need to place children in separate regions
 * (e.g., Widget_Split pane 1 = child 0, pane 2 = child 1).
 * Returns true if a child at that index was found and emitted. */
bool _cel_clay_emit_child_at_index(int index) {
    if (!g_layout_pass_active || g_layout_world == NULL) {
        fprintf(stderr, "[cels-clay] _cel_clay_emit_child_at_index() called outside layout pass\n");
        return false;
    }

    _CelSortedChild buf[CEL_CLAY_MAX_SORTED_CHILDREN];
    int count = 0;

    ecs_entity_t so_id = (ecs_entity_t)CELS_SIBLING_ORDER;
    ecs_iter_t it = ecs_children(g_layout_world, g_layout_current_entity);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count && count < CEL_CLAY_MAX_SORTED_CHILDREN; i++) {
            const cels_sibling_order_t* so = (const cels_sibling_order_t*)
                ecs_get_id(g_layout_world, it.entities[i], so_id);
            buf[count].entity = it.entities[i];
            buf[count].order = so ? so->order : (uint32_t)count;
            count++;
        }
    }

    /* Insertion sort by order */
    for (int i = 1; i < count; i++) {
        _CelSortedChild tmp = buf[i];
        int j = i - 1;
        while (j >= 0 && buf[j].order > tmp.order) {
            buf[j + 1] = buf[j];
            j--;
        }
        buf[j + 1] = tmp;
    }

    if (index >= 0 && index < count) {
        clay_walk_entity(g_layout_world, buf[index].entity);
        return true;
    }
    return false;  /* No child at that index */
}

/* ============================================================================
 * Render Command Storage
 * ============================================================================
 *
 * After Clay_EndLayout(), render commands are stored for the render bridge
 * to consume. Accessible via _cel_clay_get_render_commands().
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
    ecs_iter_t surface_it = ecs_each_id(world, ClaySurfaceConfig_id);
    while (ecs_each_next(&surface_it)) {
        for (int i = 0; i < surface_it.count; i++) {
            ecs_entity_t surface = surface_it.entities[i];
            const ClaySurfaceConfig* config = (const ClaySurfaceConfig*)
                ecs_get_id(world, surface, ClaySurfaceConfig_id);
            if (!config) continue;

            /* Skip layout if dimensions are too small */
            if (config->width < 2.0f || config->height < 2.0f) continue;

            /* 1. Set layout dimensions (reset text cache on resize) */
            {
                static float prev_w = 0, prev_h = 0;
                if (config->width != prev_w || config->height != prev_h) {
                    /* Reset text cache on actual resize (not initial 0->real) */
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

            /* 4. Walk children inside a TOP_TO_BOTTOM root container.
             * Clay's implicit root uses LEFT_TO_RIGHT (enum default 0),
             * which would arrange widgets horizontally. */
            CLAY_AUTO_ID({ .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }
            }}) {
                clay_walk_children(world, surface);
            }

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
