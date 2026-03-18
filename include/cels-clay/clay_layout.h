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
 * Clay Layout System - Public API for CELS-Clay Layout Integration
 *
 * Provides the layout infrastructure for building Clay layout trees from
 * CELS entity hierarchies. The layout system reads property components
 * (ClayContainerConfig, ClayTextConfig, etc. from clay_primitives.h) from
 * entities and generates corresponding CLAY() calls per-frame.
 *
 * Developers compose UI by attaching property components to entities via
 * primitive compositions (ClayRow, ClayColumn, ClayText, etc.), and the
 * layout walker handles all Clay API interaction internally.
 *
 * Usage:
 *   #include <cels-clay/clay_layout.h>
 *   #include <cels-clay/clay_primitives.h>
 *
 *   // Wrap tree in ClaySurface
 *   ClaySurface(.width = 80, .height = 24) {
 *       ClayColumn(.gap = 1) {
 *           ClayText(.text = "Hello") {}
 *           ClayRow(.gap = 2) {
 *               ClayText(.text = "World") {}
 *           }
 *       }
 *   }
 */

#ifndef CELS_CLAY_LAYOUT_H
#define CELS_CLAY_LAYOUT_H

#include <cels/cels.h>
#include "clay.h"
#include <stdbool.h>

/* ============================================================================
 * ClaySurfaceConfig Component
 * ============================================================================
 *
 * Attached to ClaySurface entities. Stores the layout dimensions used
 * to call Clay_SetLayoutDimensions() before each layout pass.
 */
typedef struct ClaySurfaceConfig {
    float width;
    float height;
} ClaySurfaceConfig;

extern cels_entity_t ClaySurfaceConfig_id;
extern void ClaySurfaceConfig_register(void);

/* ============================================================================
 * ClaySurface Composition (built-in)
 * ============================================================================
 *
 * ClaySurface owns the Clay layout pass boundary. Wrapping compositions
 * inside ClaySurface makes them participate in Clay layout. The layout
 * system (ClayLayoutSystem at PreStore) finds entities with ClaySurfaceConfig
 * and runs BeginLayout -> tree walk -> EndLayout for each.
 *
 * Accepts reactive dimensions (width, height). Wire window size to these
 * props for resize support -- when props change, CELS recomposition updates
 * ClaySurfaceConfig, and the next layout frame picks up new dimensions via
 * Clay_SetLayoutDimensions().
 *
 * Usage:
 *   CEL_Root(App) {
 *       ClaySurface(.width = 80, .height = 24) {
 *           Sidebar() {}
 *           Content() {}
 *       }
 *   }
 */
/* ClaySurface composition -- manually expanded from CEL_Composition to use
 * static linkage, avoiding multiple-definition errors when included by
 * multiple TUs in the INTERFACE library pattern.
 *
 * Naming convention: Name_props, Name_impl, Name_factory -- matches the
 * _cel_compose_impl macro expectations (cel_init -> _cel_compose -> Name_props,
 * Name_factory, Name_impl). */
typedef struct ClaySurface_props {
    cels_lifecycle_def_t* lifecycle;
    const char* id;
    float width;
    float height;
} ClaySurface_props;
static void ClaySurface_impl(ClaySurface_props props);
static void ClaySurface_factory(void* _raw_props) {
    ClaySurface_props _p = {0};
    if (_raw_props) _p = *(ClaySurface_props*)_raw_props;
    ClaySurface_impl(_p);
}
static void ClaySurface_impl(ClaySurface_props props) {
    /* Use cel_has for first-time component attachment, then force-update
     * on recomposition. cel_has skips writes on reused entities (by design,
     * to preserve system mutations). But ClaySurfaceConfig is composition-
     * driven -- dimensions come from reactive props (e.g. terminal size),
     * so we must overwrite on every recomposition. */
    cel_has(ClaySurfaceConfig, .width = props.width, .height = props.height);
    cels_entity_set_component(cels_get_current_entity(), ClaySurfaceConfig_id,
        &(ClaySurfaceConfig){ .width = props.width, .height = props.height },
        sizeof(ClaySurfaceConfig));
}

#define ClaySurface(...) cel_init(ClaySurface, __VA_ARGS__)

/* ============================================================================
 * CEL_Clay(...)
 * ============================================================================
 *
 * Wraps CLAY() with an auto-generated unique Clay_ElementId derived from
 * the current entity ID and the call site counter (__COUNTER__). Used
 * internally by emit functions and available for advanced custom layouts.
 *
 * The trailing block contains CLAY_TEXT, nested CEL_Clay, CEL_Clay_Children,
 * or other Clay element calls.
 *
 * Example:
 *   CEL_Clay(.layout = { .padding = CLAY_PADDING_ALL(1) }) {
 *       CLAY_TEXT(CLAY_STRING("Hello"), CLAY_TEXT_CONFIG({ .fontSize = 16 }));
 *   }
 */
#define CEL_Clay(...) \
    CLAY(_cel_clay_auto_id(__COUNTER__), __VA_ARGS__)

/* ============================================================================
 * CEL_Clay_Children()
 * ============================================================================
 *
 * Emits child entities at this point in the CLAY tree. Used internally
 * by the layout walker's emit functions. Also available for advanced
 * custom layout patterns that need explicit child placement control.
 */
#define CEL_Clay_Children() \
    _cel_clay_emit_children()

/* ============================================================================
 * CEL_Clay_ChildAt(index)
 * ============================================================================
 *
 * Emits a specific child entity by index (0-based). Used by container
 * widgets that render children in separate regions (e.g., split panes).
 * Returns true if child was found and emitted, false otherwise.
 */
#define CEL_Clay_ChildAt(index) \
    _cel_clay_emit_child_at_index(index)

/* ============================================================================
 * CEL_Clay_ChildrenRange(start, count)
 * ============================================================================
 *
 * Emits only a range of child entities (0-based, in sibling order).
 * Used by scrollable containers for virtual rendering -- only visible
 * children get Clay elements, avoiding element overflow for large lists.
 *
 * Example:
 *   // Only lay out children [offset, offset + visible)
 *   CEL_Clay_ChildrenRange(scroll_offset, visible_count);
 */
#define CEL_Clay_ChildrenRange(start, count) \
    _cel_clay_emit_children_range((start), (count))

/* ============================================================================
 * CEL_Clay_Text(buf, len)
 * ============================================================================
 *
 * Creates a Clay_String from a dynamic buffer by copying into the per-frame
 * arena. The copy survives until the renderer reads it after EndLayout.
 * Use with CLAY_TEXT for dynamic (snprintf) strings.
 *
 * Example:
 *   char buf[64];
 *   int len = snprintf(buf, sizeof(buf), "Count: %d", counter->value);
 *   CLAY_TEXT(CEL_Clay_Text(buf, len),
 *             CLAY_TEXT_CONFIG({ .fontSize = 16 }));
 */
#define CEL_Clay_Text(buf, len) \
    _cel_clay_frame_arena_string((buf), (len))

/* ============================================================================
 * Internal Function Declarations
 * ============================================================================
 *
 * Used by macros above. Prefixed with underscore -- not part of public API.
 */
extern Clay_ElementId _cel_clay_auto_id(uint32_t counter);
extern void _cel_clay_emit_children(void);
extern void _cel_clay_emit_children_range(int start, int count);
extern bool _cel_clay_emit_child_at_index(int index);
extern Clay_String _cel_clay_frame_arena_string(const char* buf, int32_t len);
extern bool _cel_clay_layout_active(void);
extern Clay_RenderCommandArray _cel_clay_get_render_commands(void);
extern Clay_Dimensions _cel_clay_get_layout_dimensions(void);

/* ============================================================================
 * Layout Subsystem Lifecycle
 * ============================================================================
 *
 * Called from clay_engine.c to initialize, register systems, and clean up
 * the layout subsystem. Not for direct consumer use.
 */
extern void _cel_clay_layout_init(void);
extern void _cel_clay_layout_cleanup(void);
extern void _cel_clay_layout_system_register(void);

#endif /* CELS_CLAY_LAYOUT_H */
