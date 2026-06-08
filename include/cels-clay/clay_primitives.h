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
 * Color resolution (IMPORTANT, see WR-03): theme-aware background resolution
 * via the CELS_DEFAULT_COLOR sentinel + active-theme slot chain is applied ONLY
 * by the text-bearing primitives -- ClayText (and ClayButton in cels-clay-widgets).
 * The container primitives (ClayRow, ClayColumn, ClayBox) and ClayImage pass
 * their `.bg` straight through to_clay_color() with NO sentinel/theme resolution:
 * omitting `.bg` yields the zero color {0,0,0,0} (transparent), NOT the active
 * theme surface. This is intentional -- containers are transparent by default so
 * they inherit the parent/canvas background. Pass an explicit `.bg` (or route it
 * through cels_resolve_color at the call site) if a theme-aware surface is wanted.
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
#include <cels-theme/cels_theme.h>  /* cels_color_t, CELS_DEFAULT_COLOR */
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * Wrapper types — nucleus_*_t convenience aliases.
 * cels_color_t is the canonical color type (owned by cels-theme, ABI-identical
 * to Clay_Color).  nucleus_color_t is a typedef alias in nucleus_types.h for
 * source compatibility; clay_primitives.h uses cels_color_t directly.
 * nucleus_padding_t and nucleus_size_t are standalone structs defined here so
 * clay_primitives.h does not depend on engines/Nucleus/include.
 * ============================================================================ */

/* Padding: ABI-identical to Clay_Padding { uint16_t left, right, top, bottom; }. */
#ifndef NUCLEUS_PADDING_T_DEFINED
#define NUCLEUS_PADDING_T_DEFINED
typedef struct { uint16_t left, right, top, bottom; } nucleus_padding_t;
#endif

/* Sizing: simplified sizing type (NOT ABI-identical to Clay_SizingAxis).
 * kind: 0=FIT, 1=GROW, 2=FIXED, 3=PERCENT. Convert via to_clay_sizing(). */
#ifndef NUCLEUS_SIZE_T_DEFINED
#define NUCLEUS_SIZE_T_DEFINED
typedef struct { uint8_t kind; float value; } nucleus_size_t;
#define NUCLEUS_SIZE_FIT(v)     ((nucleus_size_t){.kind = 0, .value = (float)(v)})
#define NUCLEUS_SIZE_GROW(v)    ((nucleus_size_t){.kind = 1, .value = (float)(v)})
#define NUCLEUS_SIZE_FIXED(v)   ((nucleus_size_t){.kind = 2, .value = (float)(v)})
#define NUCLEUS_SIZE_PERCENT(v) ((nucleus_size_t){.kind = 3, .value = (float)(v)})
#endif

/* ============================================================================
 * ABI-drift guards (file-scope exception: clay_primitives.h already includes
 * clay.h, so _Static_assert guards are permitted here — see PATTERNS.md).
 * Fail the build at compile time if Clay changes its struct layout.
 * No size assert for nucleus_size_t (NOT ABI-identical to Clay_SizingAxis).
 * ============================================================================ */
_Static_assert(sizeof(cels_color_t) == sizeof(Clay_Color),
    "cels_color_t layout drifted from Clay_Color -- update cels_theme.h");
_Static_assert(sizeof(nucleus_padding_t) == sizeof(Clay_Padding),
    "nucleus_padding_t layout drifted from Clay_Padding -- update clay_primitives.h");

/* ============================================================================
 * Type-conversion helpers — nucleus_*_t → Clay_* at the boundary.
 * Must be static inline: this header is included by multiple TUs (INTERFACE
 * library pattern) — multiple-definition errors if not static.
 * ============================================================================ */

/* to_clay_color: reinterpret cels_color_t as Clay_Color via memcpy.
 * ABI-identical — verified by _Static_assert above. */
static inline Clay_Color to_clay_color(cels_color_t c) {
    Clay_Color r;
    __builtin_memcpy(&r, &c, sizeof(r));
    return r;
}

/* to_clay_padding: reinterpret nucleus_padding_t as Clay_Padding via memcpy.
 * ABI-identical — verified by _Static_assert above. */
static inline Clay_Padding to_clay_padding(nucleus_padding_t p) {
    Clay_Padding r;
    __builtin_memcpy(&r, &p, sizeof(r));
    return r;
}

/* to_clay_sizing: construct Clay_SizingAxis from nucleus_size_t.
 * NOT a memcpy — nucleus_size_t is NOT ABI-identical to Clay_SizingAxis.
 *
 * nucleus_size_t kind mapping → Clay__SizingType:
 *   0 (FIT)     → CLAY__SIZING_TYPE_FIT     (wraps content, no value needed)
 *   1 (GROW)    → CLAY__SIZING_TYPE_GROW    (fill available space)
 *   2 (FIXED)   → CLAY__SIZING_TYPE_FIXED   (exact pixel size via minMax)
 *   3 (PERCENT) → CLAY__SIZING_TYPE_PERCENT (fraction of parent via .percent)
 *   other       → CLAY__SIZING_TYPE_FIT     (safe fallback)
 *
 * Clay__SizingType enum values (confirmed from clay.h):
 *   CLAY__SIZING_TYPE_FIT=0, GROW=1, PERCENT=2, FIXED=3
 * nucleus_size_t kind=2 (FIXED) maps to Clay value 3 -- NOT a direct cast.
 */
static inline Clay_SizingAxis to_clay_sizing(nucleus_size_t s) {
    Clay_SizingAxis ax = {0};
    switch (s.kind) {
        case 0: /* FIT */
            ax.type = CLAY__SIZING_TYPE_FIT;
            break;
        case 1: /* GROW */
            ax.type = CLAY__SIZING_TYPE_GROW;
            break;
        case 2: /* FIXED */
            ax.type = CLAY__SIZING_TYPE_FIXED;
            ax.size.minMax.min = s.value;
            ax.size.minMax.max = s.value;
            break;
        case 3: /* PERCENT */
            ax.type = CLAY__SIZING_TYPE_PERCENT;
            ax.size.percent = s.value;
            break;
        default: /* unknown kind — safe fallback to FIT */
            ax.type = CLAY__SIZING_TYPE_FIT;
            break;
    }
    return ax;
}

/* ============================================================================
 * Component Structs
 * ============================================================================ */

/* ClayContainerConfig -- shared by Row, Column, and Box.
 * The direction field determines layout axis:
 *   CLAY_LEFT_TO_RIGHT for Row, CLAY_TOP_TO_BOTTOM for Column/Box.
 *
 * Internal component — stores pre-converted Clay_* types after boundary
 * conversion in *_impl. Callers use ClayRow_props / ClayColumn_props /
 * ClayBox_props (which take nucleus_*_t wrapper types). */
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
    nucleus_padding_t padding;       /* was Clay_Padding — converted at cel_has boundary */
    nucleus_size_t width;            /* was Clay_SizingAxis — converted via to_clay_sizing */
    nucleus_size_t height;           /* was Clay_SizingAxis — converted via to_clay_sizing */
    Clay_ChildAlignment alignment;
    cels_color_t bg;                 /* was Clay_Color — converted via to_clay_color */
    bool clip;
} ClayRow_props;

static void ClayRow_impl(ClayRow_props props);
static void ClayRow_factory(void* _raw_props) {
    ClayRow_props _p = {0};
    if (_raw_props) _p = *(ClayRow_props*)_raw_props;
    ClayRow_impl(_p);
}
static void ClayRow_impl(ClayRow_props props) {
    /* WR-03: bg is NOT sentinel/theme resolved -- omitting .bg gives transparent
     * {0,0,0,0} by design (containers inherit parent bg). Pass explicit .bg. */
    cel_has(ClayContainerConfig,
        .direction = CLAY_LEFT_TO_RIGHT,
        .gap = props.gap,
        .padding = to_clay_padding(props.padding),
        .width   = to_clay_sizing(props.width),
        .height  = to_clay_sizing(props.height),
        .alignment = props.alignment,
        .bg = to_clay_color(props.bg),
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
    nucleus_padding_t padding;       /* was Clay_Padding — converted at cel_has boundary */
    nucleus_size_t width;            /* was Clay_SizingAxis — converted via to_clay_sizing */
    nucleus_size_t height;           /* was Clay_SizingAxis — converted via to_clay_sizing */
    Clay_ChildAlignment alignment;
    cels_color_t bg;                 /* was Clay_Color — converted via to_clay_color */
    bool clip;
} ClayColumn_props;

static void ClayColumn_impl(ClayColumn_props props);
static void ClayColumn_factory(void* _raw_props) {
    ClayColumn_props _p = {0};
    if (_raw_props) _p = *(ClayColumn_props*)_raw_props;
    ClayColumn_impl(_p);
}
static void ClayColumn_impl(ClayColumn_props props) {
    /* WR-03: bg is NOT sentinel/theme resolved -- omitting .bg gives transparent
     * {0,0,0,0} by design (containers inherit parent bg). Pass explicit .bg. */
    cel_has(ClayContainerConfig,
        .direction = CLAY_TOP_TO_BOTTOM,
        .gap = props.gap,
        .padding = to_clay_padding(props.padding),
        .width   = to_clay_sizing(props.width),
        .height  = to_clay_sizing(props.height),
        .alignment = props.alignment,
        .bg = to_clay_color(props.bg),
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
    nucleus_padding_t padding;       /* was Clay_Padding — converted at cel_has boundary */
    nucleus_size_t width;            /* was Clay_SizingAxis — converted via to_clay_sizing */
    nucleus_size_t height;           /* was Clay_SizingAxis — converted via to_clay_sizing */
    Clay_ChildAlignment alignment;
    cels_color_t bg;                 /* was Clay_Color — converted via to_clay_color */
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
    /* WR-03: container bg is NOT sentinel/theme resolved -- omitting .bg gives
     * transparent {0,0,0,0} by design (containers inherit parent bg). Only
     * ClayText / ClayButton are theme-resolving. Pass explicit .bg for a fill. */
    cel_has(ClayContainerConfig,
        .direction = CLAY_TOP_TO_BOTTOM,
        .padding = to_clay_padding(props.padding),
        .width   = to_clay_sizing(props.width),
        .height  = to_clay_sizing(props.height),
        .alignment = props.alignment,
        .bg = to_clay_color(props.bg),
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
 * Color resolution: explicit prop > active theme text slot > white fallback.
 * Pass CELS_DEFAULT_COLOR (or omit the field) to use the active theme color.
 * font_size defaults to 16 if not specified.
 */

typedef struct ClayText_props {
    cels_lifecycle_def_t* lifecycle;
    const char* id;
    const char* text;
    cels_color_t color;              /* was Clay_Color — sentinel+theme resolved in impl */
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
    /* Resolve text color via 2-level sentinel chain (WR-01 fix):
     *   1. props.color            — explicit per-text color (if not sentinel)
     *   2. theme->text            — active theme text slot  (if not sentinel)
     *   3. FB_TEXT                — compiled-in white fallback (last resort)
     *
     * CELS_DEFAULT_COLOR (c.r < 0) is the sentinel: {0,0,0,0} transparent and
     * {0,0,0,255} pure black are now honored as explicit colors (WR-01 resolved).
     */
    const struct CelsTheme_Active* ta = cel_watch(CelsTheme_Active);
    const CelsTheme* theme = ta ? &ta->active : NULL;
    static const cels_color_t FB_TEXT = {255.0f, 255.0f, 255.0f, 255.0f};

    cels_color_t c = cels_resolve_color(props.color,
                     cels_resolve_color(theme ? theme->text : CELS_DEFAULT_COLOR, FB_TEXT));
    cel_has(ClayTextConfig,
        .text = props.text,
        .color = to_clay_color(c),
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
    nucleus_size_t width;            /* was Clay_SizingAxis — converted via to_clay_sizing */
    nucleus_size_t height;           /* was Clay_SizingAxis — converted via to_clay_sizing */
} ClaySpacer_props;

static void ClaySpacer_impl(ClaySpacer_props props);
static void ClaySpacer_factory(void* _raw_props) {
    ClaySpacer_props _p = {0};
    if (_raw_props) _p = *(ClaySpacer_props*)_raw_props;
    ClaySpacer_impl(_p);
}
static void ClaySpacer_impl(ClaySpacer_props props) {
    cel_has(ClaySpacerConfig,
        .width  = to_clay_sizing(props.width),
        .height = to_clay_sizing(props.height)
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
    /* WR-03: bg is NOT sentinel/theme resolved -- omitting .bg gives transparent
     * {0,0,0,0} by design. Only ClayText / ClayButton are theme-resolving. */
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
