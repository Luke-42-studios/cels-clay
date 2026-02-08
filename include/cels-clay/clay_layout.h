/*
 * Clay Layout System - Public API for CELS-Clay Layout Integration
 *
 * Provides the developer-facing API for building Clay layout trees from
 * CELS entity hierarchies. Layout functions are defined at file scope
 * with CEL_Clay_Layout(name), attached to entities via ClayUI component,
 * and called per-frame by the layout system between BeginLayout/EndLayout.
 *
 * Usage:
 *   #include <cels-clay/clay_layout.h>
 *
 *   // 1. Define layout function at file scope
 *   CEL_Clay_Layout(my_layout) {
 *       CEL_Clay(.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM }) {
 *           CEL_Clay_Children();
 *       }
 *   }
 *
 *   // 2. Attach to entity inside a composition
 *   CEL_Has(ClayUI, .layout_fn = my_layout);
 *
 *   // 3. Wrap tree in ClaySurface
 *   ClaySurface(.width = 80, .height = 24) {
 *       MyApp() {}
 *   }
 */

#ifndef CELS_CLAY_LAYOUT_H
#define CELS_CLAY_LAYOUT_H

#include <cels/cels.h>
#include "clay.h"
#include <stdbool.h>

/* ============================================================================
 * ClayUI Component
 * ============================================================================
 *
 * Entities with ClayUI participate in the Clay layout tree. The layout
 * system queries these and calls .layout_fn during the per-frame tree walk.
 */
typedef void (*CelClayLayoutFn)(struct ecs_world_t* world, cels_entity_t self);

typedef struct ClayUI {
    CelClayLayoutFn layout_fn;
} ClayUI;

extern cels_entity_t ClayUIID;
extern cels_entity_t ClayUI_ensure(void);

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

extern cels_entity_t ClaySurfaceConfigID;
extern cels_entity_t ClaySurfaceConfig_ensure(void);

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
 *       Clay_Engine_use((Clay_EngineConfig){0});
 *       ClaySurface(.width = 80, .height = 24) {
 *           Sidebar() {}
 *           Content() {}
 *       }
 *   }
 */
CEL_Composition(ClaySurface, float width; float height;) {
    CEL_Has(ClaySurfaceConfig, .width = props.width, .height = props.height);
}

#define ClaySurface(...) CEL_Init(ClaySurface, __VA_ARGS__)

/* ============================================================================
 * CEL_Clay_Layout(name)
 * ============================================================================
 *
 * Defines a layout function signature at file scope. Use inside layouts
 * to emit CLAY() elements. Reference the function name in CEL_Has(ClayUI).
 *
 * Example:
 *   CEL_Clay_Layout(sidebar_layout) {
 *       CEL_Clay(.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM }) {
 *           CLAY_TEXT(CLAY_STRING("Header"), text_config);
 *           CEL_Clay_Children();
 *       }
 *   }
 *
 *   // In composition:
 *   CEL_Has(ClayUI, .layout_fn = sidebar_layout);
 */
#define CEL_Clay_Layout(name) \
    static void name(struct ecs_world_t* world, cels_entity_t self)

/* ============================================================================
 * CEL_Clay(...)
 * ============================================================================
 *
 * Wraps CLAY() with an auto-generated unique Clay_ElementId derived from
 * the current entity ID and the call site counter (__COUNTER__). Use
 * inside layout functions to emit Clay elements.
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
 * Emits child entities at this point in the CLAY tree. Controls WHERE
 * in the layout tree children appear -- not always at end of scope.
 *
 * Example:
 *   CEL_Clay_Layout(panel_layout) {
 *       CEL_Clay(.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM }) {
 *           CLAY_TEXT(CLAY_STRING("Title"), title_config);
 *           CEL_Clay_Children();   // children render HERE, between title and footer
 *           CLAY_TEXT(CLAY_STRING("Footer"), footer_config);
 *       }
 *   }
 */
#define CEL_Clay_Children() \
    _cel_clay_emit_children()

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
extern Clay_String _cel_clay_frame_arena_string(const char* buf, int32_t len);
extern bool _cel_clay_layout_active(void);

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
