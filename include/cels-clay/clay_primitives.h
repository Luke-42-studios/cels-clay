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
 * Clay Entity Primitives - Component Structs and Static Compositions
 *
 * Defines the building blocks developers use to construct UI via entity
 * composition. Instead of writing layout functions with raw CLAY() macros,
 * developers compose entities like:
 *
 *   ClayRow(.gap = 2) {
 *       ClayText(.text = "Hello") {}
 *   }
 *
 * Component structs carry per-entity properties that Phase 8's layout
 * walker reads to generate CLAY() calls.
 *
 * Components (5):
 *   - ClayContainerConfig: shared by Row, Column, Box (direction, gap, etc.)
 *   - ClayTextConfig: text content and styling
 *   - ClaySpacerConfig: fixed or flexible spacing
 *   - ClayImageConfig: image source and sizing
 *   - ClayBorderStyle: additive border decoration (can be added to any container)
 *
 * Compositions (6):
 *   - ClayRow: horizontal layout (LEFT_TO_RIGHT)
 *   - ClayColumn: vertical layout (TOP_TO_BOTTOM)
 *   - ClayBox: vertical layout with optional border
 *   - ClayText: text element
 *   - ClaySpacer: spacing element
 *   - ClayImage: image element
 *
 * All compositions use static linkage to avoid multiple-definition errors
 * when included by multiple TUs in the INTERFACE library pattern.
 *
 * Naming convention: Name_props, Name_impl, Name_factory -- matches the
 * _cel_compose_impl macro expectations (cel_init -> _cel_compose).
 */

#ifndef CELS_CLAY_PRIMITIVES_H
#define CELS_CLAY_PRIMITIVES_H

#include <cels/cels.h>
#include "clay.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * Component Structs
 * ============================================================================ */

/* ClayContainerConfig -- shared by Row, Column, and Box.
 * The direction field determines layout axis:
 *   CLAY_LEFT_TO_RIGHT for Row, CLAY_TOP_TO_BOTTOM for Column/Box. */
typedef struct ClayContainerConfig {
    Clay_LayoutDirection direction;
    uint16_t gap;
    Clay_Padding padding;
    Clay_SizingAxis width;
    Clay_SizingAxis height;
    Clay_ChildAlignment alignment;
    Clay_Color bg;
    bool clip;
} ClayContainerConfig;

/* ClayTextConfig -- text content and styling properties. */
typedef struct ClayTextConfig {
    const char* text;
    Clay_Color color;
    uint16_t font_size;
    uint16_t font_id;
    uint16_t letter_spacing;
    uint16_t line_height;
    Clay_TextElementConfigWrapMode wrap;
} ClayTextConfig;

/* ClaySpacerConfig -- fixed or flexible spacing between elements. */
typedef struct ClaySpacerConfig {
    Clay_SizingAxis width;
    Clay_SizingAxis height;
} ClaySpacerConfig;

/* ClayImageConfig -- image source, dimensions, and styling.
 * source_width/source_height are the natural image dimensions for
 * aspect-ratio-aware sizing in the layout walker. */
typedef struct ClayImageConfig {
    void* source;
    float source_width;
    float source_height;
    Clay_SizingAxis width;
    Clay_SizingAxis height;
    Clay_Color bg;
    Clay_CornerRadius corner_radius;
} ClayImageConfig;

/* ClayBorderStyle -- additive component for border decoration.
 * Can be attached to any container entity alongside ClayContainerConfig.
 * Uses Clay_BorderElementConfig (color + widths) and corner radius. */
typedef struct ClayBorderStyle {
    Clay_BorderElementConfig border;
    Clay_CornerRadius corner_radius;
} ClayBorderStyle;

/* ============================================================================
 * Component ID Extern Declarations
 * ============================================================================
 *
 * Defined in clay_primitives.c. Same pattern as ClaySurfaceConfig_id in clay_layout.h.
 */

extern cels_entity_t ClayContainerConfig_id;
extern void ClayContainerConfig_register(void);

extern cels_entity_t ClayTextConfig_id;
extern void ClayTextConfig_register(void);

extern cels_entity_t ClaySpacerConfig_id;
extern void ClaySpacerConfig_register(void);

extern cels_entity_t ClayImageConfig_id;
extern void ClayImageConfig_register(void);

extern cels_entity_t ClayBorderStyle_id;
extern void ClayBorderStyle_register(void);

/* ============================================================================
 * Static-Linkage Compositions
 * ============================================================================
 *
 * Each composition follows the manual static expansion pattern for INTERFACE
 * library compatibility:
 *   1. Name_props typedef (with lifecycle + id fields for cel_init compat)
 *   2. Name_impl static function (calls cel_has to attach component)
 *   3. Name_factory static wrapper (void* -> typed props)
 *   4. #define Name(...) cel_init(Name, __VA_ARGS__) call macro
 */

/* --------------------------------------------------------------------------
 * ClayRow -- horizontal layout (LEFT_TO_RIGHT)
 * -------------------------------------------------------------------------- */

typedef struct ClayRow_props {
    cels_lifecycle_def_t* lifecycle;
    const char* id;
    uint16_t gap;
    Clay_Padding padding;
    Clay_SizingAxis width;
    Clay_SizingAxis height;
    Clay_ChildAlignment alignment;
    Clay_Color bg;
    bool clip;
} ClayRow_props;

static void ClayRow_impl(ClayRow_props props);
static void ClayRow_factory(void* _raw_props) {
    ClayRow_props _p = {0};
    if (_raw_props) _p = *(ClayRow_props*)_raw_props;
    ClayRow_impl(_p);
}
static void ClayRow_impl(ClayRow_props props) {
    cel_has(ClayContainerConfig,
        .direction = CLAY_LEFT_TO_RIGHT,
        .gap = props.gap,
        .padding = props.padding,
        .width = props.width,
        .height = props.height,
        .alignment = props.alignment,
        .bg = props.bg,
        .clip = props.clip
    );
}

#define ClayRow(...) cel_init(ClayRow, __VA_ARGS__)

/* --------------------------------------------------------------------------
 * ClayColumn -- vertical layout (TOP_TO_BOTTOM)
 * -------------------------------------------------------------------------- */

typedef struct ClayColumn_props {
    cels_lifecycle_def_t* lifecycle;
    const char* id;
    uint16_t gap;
    Clay_Padding padding;
    Clay_SizingAxis width;
    Clay_SizingAxis height;
    Clay_ChildAlignment alignment;
    Clay_Color bg;
    bool clip;
} ClayColumn_props;

static void ClayColumn_impl(ClayColumn_props props);
static void ClayColumn_factory(void* _raw_props) {
    ClayColumn_props _p = {0};
    if (_raw_props) _p = *(ClayColumn_props*)_raw_props;
    ClayColumn_impl(_p);
}
static void ClayColumn_impl(ClayColumn_props props) {
    cel_has(ClayContainerConfig,
        .direction = CLAY_TOP_TO_BOTTOM,
        .gap = props.gap,
        .padding = props.padding,
        .width = props.width,
        .height = props.height,
        .alignment = props.alignment,
        .bg = props.bg,
        .clip = props.clip
    );
}

#define ClayColumn(...) cel_init(ClayColumn, __VA_ARGS__)

/* --------------------------------------------------------------------------
 * ClayBox -- vertical layout with optional border
 * --------------------------------------------------------------------------
 * ClayBox is like ClayColumn but additionally attaches a ClayBorderStyle
 * component when border widths are non-zero. Useful for panels, cards, etc.
 */

typedef struct ClayBox_props {
    cels_lifecycle_def_t* lifecycle;
    const char* id;
    Clay_Padding padding;
    Clay_SizingAxis width;
    Clay_SizingAxis height;
    Clay_ChildAlignment alignment;
    Clay_Color bg;
    Clay_BorderElementConfig border;
    Clay_CornerRadius corner_radius;
    bool clip;
} ClayBox_props;

static void ClayBox_impl(ClayBox_props props);
static void ClayBox_factory(void* _raw_props) {
    ClayBox_props _p = {0};
    if (_raw_props) _p = *(ClayBox_props*)_raw_props;
    ClayBox_impl(_p);
}
static void ClayBox_impl(ClayBox_props props) {
    cel_has(ClayContainerConfig,
        .direction = CLAY_TOP_TO_BOTTOM,
        .padding = props.padding,
        .width = props.width,
        .height = props.height,
        .alignment = props.alignment,
        .bg = props.bg,
        .clip = props.clip
    );
    /* Attach border style only if any border width is non-zero */
    if (props.border.width.top || props.border.width.right ||
        props.border.width.bottom || props.border.width.left) {
        cel_has(ClayBorderStyle,
            .border = props.border,
            .corner_radius = props.corner_radius
        );
    }
}

#define ClayBox(...) cel_init(ClayBox, __VA_ARGS__)

/* --------------------------------------------------------------------------
 * ClayText -- text element
 * --------------------------------------------------------------------------
 * Defaults: color white (255,255,255,255), font_size 16 if not specified.
 */

typedef struct ClayText_props {
    cels_lifecycle_def_t* lifecycle;
    const char* id;
    const char* text;
    Clay_Color color;
    uint16_t font_size;
    uint16_t font_id;
    uint16_t letter_spacing;
    uint16_t line_height;
    Clay_TextElementConfigWrapMode wrap;
} ClayText_props;

static void ClayText_impl(ClayText_props props);
static void ClayText_factory(void* _raw_props) {
    ClayText_props _p = {0};
    if (_raw_props) _p = *(ClayText_props*)_raw_props;
    ClayText_impl(_p);
}
static void ClayText_impl(ClayText_props props) {
    cel_has(ClayTextConfig,
        .text = props.text,
        .color = (props.color.r || props.color.g || props.color.b || props.color.a)
            ? props.color : (Clay_Color){255, 255, 255, 255},
        .font_size = props.font_size ? props.font_size : 16,
        .font_id = props.font_id,
        .letter_spacing = props.letter_spacing,
        .line_height = props.line_height,
        .wrap = props.wrap
    );
}

#define ClayText(...) cel_init(ClayText, __VA_ARGS__)

/* --------------------------------------------------------------------------
 * ClaySpacer -- spacing element
 * --------------------------------------------------------------------------
 * If width and height are both zero-initialized, the layout walker
 * (Phase 8) should treat them as CLAY_SIZING_GROW(0).
 */

typedef struct ClaySpacer_props {
    cels_lifecycle_def_t* lifecycle;
    const char* id;
    Clay_SizingAxis width;
    Clay_SizingAxis height;
} ClaySpacer_props;

static void ClaySpacer_impl(ClaySpacer_props props);
static void ClaySpacer_factory(void* _raw_props) {
    ClaySpacer_props _p = {0};
    if (_raw_props) _p = *(ClaySpacer_props*)_raw_props;
    ClaySpacer_impl(_p);
}
static void ClaySpacer_impl(ClaySpacer_props props) {
    cel_has(ClaySpacerConfig,
        .width = props.width,
        .height = props.height
    );
}

#define ClaySpacer(...) cel_init(ClaySpacer, __VA_ARGS__)

/* --------------------------------------------------------------------------
 * ClayImage -- image element
 * -------------------------------------------------------------------------- */

typedef struct ClayImage_props {
    cels_lifecycle_def_t* lifecycle;
    const char* id;
    void* source;
    float source_width;
    float source_height;
    Clay_SizingAxis width;
    Clay_SizingAxis height;
    Clay_Color bg;
    Clay_CornerRadius corner_radius;
} ClayImage_props;

static void ClayImage_impl(ClayImage_props props);
static void ClayImage_factory(void* _raw_props) {
    ClayImage_props _p = {0};
    if (_raw_props) _p = *(ClayImage_props*)_raw_props;
    ClayImage_impl(_p);
}
static void ClayImage_impl(ClayImage_props props) {
    cel_has(ClayImageConfig,
        .source = props.source,
        .source_width = props.source_width,
        .source_height = props.source_height,
        .width = props.width,
        .height = props.height,
        .bg = props.bg,
        .corner_radius = props.corner_radius
    );
}

#define ClayImage(...) cel_init(ClayImage, __VA_ARGS__)

#endif /* CELS_CLAY_PRIMITIVES_H */
