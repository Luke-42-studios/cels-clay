# Technology Stack

**Project:** cels-clay v0.6 -- Entity-based UI with Compose-inspired primitives and dual renderers
**Researched:** 2026-03-15
**Overall confidence:** HIGH (all recommendations based on verified source code from cels v0.4+, cels-clay, cels-sdl3, cels-ncurses, and Clay v0.14)

## Executive Summary

The v0.6 transformation converts cels-clay from a "layout function attached to entity" model to a "entity IS a UI primitive" model. Today, a developer writes `CEL_Clay_Layout(my_layout)` at file scope, manually calls `CLAY()` macros inside it, and attaches it via `cel_has(ClayUI, .layout_fn = my_layout)`. After v0.6, a developer writes `Row(.gap = 4) { Text(.content = "hello") {} }` and the framework translates entity components into Clay layout calls automatically.

This is not a new stack -- it is a new **layer** on the existing stack. The technologies are already validated and in production. What changes is the abstraction boundary: entity primitives become the public API, and the Clay-level layout functions become generated internal code. Simultaneously, an SDL3 renderer is added alongside the existing NCurses renderer, both living inside cels-clay with conditional compilation.

---

## Recommended Stack

### Core Framework (No Changes)

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| CELS | v0.4+ | ECS framework, reactivity, lifecycle | Already integrated; CEL_Module, CEL_Component, CEL_Compose, CEL_System patterns are the foundation |
| Clay | v0.14 | Layout computation engine | Already integrated via FetchContent; single-header, proven layout math, renderer-agnostic |
| Flecs | (via CELS) | Underlying ECS runtime | Provides entity hierarchy, component storage, system scheduling; accessed through CELS wrappers |
| CMake | 3.21+ | Build system | Already used; conditional compilation via `if(TARGET ...)` for optional renderers |

**Confidence: HIGH** -- all verified from current source code in the repository.

### Renderer Dependencies (Existing, Now Compiled Into cels-clay)

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| SDL3 | latest (via cels-sdl3) | GPU-accelerated rendering | cels-sdl3 already handles SDL3 initialization; renderer translates Clay render commands to SDL3_Renderer calls |
| NCurses | 6.x (via cels-ncurses) | Terminal rendering | Already fully integrated; renderer translates Clay render commands to TUI draw calls |
| SDL3_ttf | (via cels-sdl3) | Font rendering for SDL3 | Required for SDL3 text measurement; Clay needs accurate text dimensions per font |

**Confidence: HIGH** -- NCurses renderer verified working (clay_ncurses_renderer.c is 720 lines of production code). SDL3 renderer to be built following the same pattern.

### New Components (To Build, Not To Install)

| Component | Type | Purpose | Notes |
|-----------|------|---------|-------|
| UI Primitive Compositions | CELS compositions | Row, Column, Box, Spacer, Text, Image | Per-entity components with tailored properties |
| Per-primitive Components | CELS components | RowProps, ColumnProps, BoxProps, TextProps, etc. | Each primitive gets its OWN component type |
| Layout Walker Extension | Internal code | Dispatches on primitive component presence | Extends existing `clay_walk_entity()` in clay_layout.c |
| SDL3 Clay Renderer | CELS system | Translates ClayRenderableData to SDL3 draw calls | Mirrors clay_ncurses_renderer.c pattern |
| Clay_UI Module | CEL_Module | Registers primitives, orchestrates initialization | Single entry point for the new entity-based API |

**No new external dependencies are required.**

---

## Technology Decisions with Rationale

### Decision 1: Per-Primitive Component Types (Not a Shared LayoutComponent)

**Use:** Separate `RowProps`, `ColumnProps`, `BoxProps`, `TextProps`, `SpacerProps`, `ImageProps` component types.

**Do NOT use:** A single generic `UILayoutConfig` component shared across all primitives.

**Rationale verified from source:** The CELS `CEL_Component` macro (cels.h:330-336) creates a unique `Name_id` per component type, and `cel_has()` (cels.h:926-930) uses `cels_ensure_component()` which registers with `sizeof(Name)`. This means each component type is a distinct archetype in Flecs. Using per-primitive components:

- Enables targeted dispatch in the layout walker (`ecs_get_id(world, entity, RowProps_id)`)
- Avoids wasted memory (TextProps has `.content`, `.font_id`; RowProps has `.gap`, `.main_axis_alignment` -- zero overlap)
- Matches the Compose pattern where `Row` and `Column` are distinct types, not "Container with direction enum"
- An entity has exactly ONE primitive component -- the layout walker checks in priority order and takes the first match

**Example component definitions:**

```c
CEL_Component(RowProps) {
    float gap;
    uint8_t main_align;   // CLAY_ALIGN_X_LEFT / CENTER / RIGHT
    uint8_t cross_align;  // CLAY_ALIGN_Y_TOP / CENTER / BOTTOM
    Clay_Padding padding;
    Clay_Sizing width;
    Clay_Sizing height;
};

CEL_Component(ColumnProps) {
    float gap;
    uint8_t main_align;   // CLAY_ALIGN_Y_TOP / CENTER / BOTTOM
    uint8_t cross_align;  // CLAY_ALIGN_X_LEFT / CENTER / RIGHT
    Clay_Padding padding;
    Clay_Sizing width;
    Clay_Sizing height;
};

CEL_Component(TextProps) {
    const char* content;
    uint16_t font_id;
    uint16_t font_size;
    Clay_Color color;
    uint8_t wrap_mode;    // CLAY_TEXT_WRAP_WORDS / NONE / NEWLINES
};
```

**Confidence: HIGH** -- verified from CEL_Component and cel_has macro expansions in cels.h.

### Decision 2: Layout Walker Dispatches on Component Presence

**Use:** The existing `clay_walk_entity()` pattern (clay_layout.c:254-275) extended to check for primitive components instead of (or in addition to) `ClayUI.layout_fn`.

**Do NOT use:** A vtable/function pointer per entity, or a tag enum + switch.

**Rationale verified from source:** The current code in `clay_walk_entity()` already does:

```c
const ClayUI* layout = (const ClayUI*)ecs_get_id(world, entity, ClayUI_id);
if (layout && layout->layout_fn) {
    layout->layout_fn(world, entity);
} else {
    clay_walk_children(world, entity);  // pass-through
}
```

The v0.6 change extends this with a priority chain before the ClayUI check:

```c
static void clay_walk_entity(ecs_world_t* world, ecs_entity_t entity) {
    ecs_entity_t prev_entity = g_layout_current_entity;
    g_layout_current_entity = entity;

    // NEW: Check primitive components first
    const RowProps* row = (const RowProps*)ecs_get_id(world, entity, RowProps_id);
    if (row) { emit_row(world, entity, row); goto done; }

    const ColumnProps* col = (const ColumnProps*)ecs_get_id(world, entity, ColumnProps_id);
    if (col) { emit_column(world, entity, col); goto done; }

    const BoxProps* box = (const BoxProps*)ecs_get_id(world, entity, BoxProps_id);
    if (box) { emit_box(world, entity, box); goto done; }

    const TextProps* text = (const TextProps*)ecs_get_id(world, entity, TextProps_id);
    if (text) { emit_text(world, entity, text); goto done; }

    const SpacerProps* spacer = (const SpacerProps*)ecs_get_id(world, entity, SpacerProps_id);
    if (spacer) { emit_spacer(world, entity, spacer); goto done; }

    // BACKWARD COMPAT: old-style layout_fn still works
    const ClayUI* layout = (const ClayUI*)ecs_get_id(world, entity, ClayUI_id);
    if (layout && layout->layout_fn) {
        layout->layout_fn(world, entity);
        goto done;
    }

    // Pass-through: walk children directly
    clay_walk_children(world, entity);

done:
    g_layout_current_entity = prev_entity;
}
```

This preserves full backward compatibility. The `emit_*` functions translate component data into `CLAY()` macro calls with the correct Clay configuration, then call `clay_walk_children()` to recurse.

**Confidence: HIGH** -- verified from existing clay_walk_entity implementation and ecs_get_id usage patterns.

### Decision 3: Compositions Wrap Component Attachment (Compose-Style API)

**Use:** CELS compositions (`CEL_Compose` / `CEL_Define_Composition`) to provide the `Row(.gap = 4) { ... }` syntax.

**Rationale verified from source:** The cels-ncurses and cels-sdl3 modules already export compositions with this exact pattern:

```c
// From cels_ncurses.h:
CEL_Define_Composition(NCursesWindow, const char* title; int fps; int color_mode;);
#define NCursesWindow(...) cel_init(NCursesWindow, __VA_ARGS__)
// Usage: NCursesWindow(.title = "My App", .fps = 60) {}
```

```c
// From cels_sdl3.h:
CEL_Define_Composition(SDL3Context, bool video;);
#define SDL3Context(...) cel_init(SDL3Context, __VA_ARGS__)
// Usage: SDL3Context(.video = true) {}
```

The UI primitives follow the same established pattern:

```c
// In cels-clay public header:
CEL_Define_Composition(Row, float gap; uint8_t main_align; uint8_t cross_align;
    Clay_Padding padding; Clay_Sizing width; Clay_Sizing height;);
#define Row(...) cel_init(Row, __VA_ARGS__)

// In cels-clay source:
CEL_Composition(Row) {
    cel_has(RowProps, .gap = cel.gap, .main_align = cel.main_align, /* ... */);
}
```

The `cel_init` macro (cels.h:951, expanding via `_cel_compose` in cels_macros.h:54-80) handles entity creation, parent-child hierarchy (children go inside the `{}`), sibling ordering, composition storage for reconciliation, and props memoization. This is exactly the Compose-inspired "composable tree" pattern.

**Confidence: HIGH** -- verified from CEL_Define_Composition and cel_init expansion in two production modules.

### Decision 4: CEL_Module for the Clay UI Module

**Use:** `CEL_Module(Clay_UI, init)` in the source, `CEL_Module(Clay_UI);` in the header.

**Rationale verified from source:** The CELS v0.4+ `CEL_Module` macro (cels.h:601-621) provides:
- Idempotent init via guard (`if (Name != 0) return;`)
- Module entity registration (`cels_module_register`)
- Dependency declaration (`cel_module_requires`)
- Capability declaration (`cel_module_provides`)
- Init-done signal (`cels_module_init_done()`)

Both cels-sdl3 and cels-ncurses use this pattern:
```c
// cels_sdl3.h: CEL_Module(SDL3_Engine);
// cels_ncurses.h: CEL_Module(NCurses);
```

The current cels-clay uses the older `cel_module()` lowercase pattern (clay_engine.c:101). The v0.6 Clay_UI module should use the v0.4 `CEL_Module` pattern:

```c
CEL_Module(Clay_UI, init) {
    cel_module_requires(Clay_Engine);  // Ensures Clay arena initialized
    // Register all primitive components and compositions
}
```

Note: `Clay_Engine` (the existing module in clay_engine.c) uses the even-older `cel_module(Clay_Engine)` internal helper. It should also be migrated to `CEL_Module(Clay_Engine, init)` for consistency, but this is a separate refactor that can happen before or during v0.6.

**Confidence: HIGH** -- verified from CEL_Module macro expansion and production usage in cels-sdl3 and cels-ncurses.

### Decision 5: Dual Renderer via Conditional Compilation (Extending Existing Pattern)

**Use:** CMake `if(TARGET cels-sdl3)` / `if(TARGET cels-ncurses)` guards, mirroring the existing pattern in CMakeLists.txt.

**Do NOT use:** Runtime renderer switching via function pointers or a renderer vtable interface.

**Rationale verified from source:** The existing CMakeLists.txt (lines 81-88) already does conditional compilation for NCurses:

```cmake
if(TARGET cels-ncurses)
    target_sources(cels-clay INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_ncurses_renderer.c
    )
    target_link_libraries(cels-clay INTERFACE cels-ncurses)
endif()
```

The SDL3 renderer adds an identical block:

```cmake
if(TARGET cels-sdl3)
    target_sources(cels-clay INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_sdl3_renderer.c
    )
    target_link_libraries(cels-clay INTERFACE cels-sdl3)
endif()
```

Both renderers register themselves as systems on the `OnRender` phase via `cels_system_declare()`. The consumer chooses which backend by which modules they register. `ClayRenderableData` is the shared contract -- both renderers query it identically.

**Critical detail from ncurses renderer source (clay_ncurses_renderer.c:631-635):**
```c
void clay_ncurses_renderer_init(const ClayNcursesTheme* theme) {
    Clay_SetMeasureTextFunction(clay_ncurses_measure_text, NULL);
    ClayRenderableData_register();
    cels_entity_t comp_ids[] = { ClayRenderableData_id };
    cels_system_declare("TUI_ClayRenderable_ClayRenderableData",
                        CELS_Phase_OnRender, clay_ncurses_render, comp_ids, 1);
}
```

The SDL3 renderer will follow this exact same init pattern.

**Confidence: HIGH** -- verified from CMakeLists.txt and clay_ncurses_renderer.c.

### Decision 6: Text Measurement Is Renderer-Owned (Not Module-Owned)

**Use:** Each renderer calls `Clay_SetMeasureTextFunction()` during its init.

**Do NOT have the Clay_UI module set a text measurement function.**

**Rationale verified from source:**
- NCurses measurement (clay_ncurses_renderer.c:542-609): Returns width in cell columns via `wcwidth()`, divided by aspect ratio for Clay coordinate space.
- SDL3 measurement: Will return width in pixels via TTF text measurement APIs.
- The generic char-counting measurement in clay_layout.c (lines 140-165) is a fallback only used when no renderer overrides it.

These are fundamentally different units. The measurement function MUST match the renderer. Since only one renderer is active at a time, the last `Clay_SetMeasureTextFunction()` call wins, which is the active renderer's init.

**Implication for architecture:** The Clay_UI module registers components and compositions but does NOT touch text measurement. That responsibility belongs exclusively to the renderer.

**Confidence: HIGH** -- verified from both measurement function implementations.

### Decision 7: Backward Compatibility for ClayUI Layout Functions

**Use:** Keep `ClayUI` component and `CEL_Clay_Layout()` macro fully functional alongside new primitives.

**Do NOT remove or deprecate ClayUI in v0.6.**

**Rationale:** The existing `ClayUI.layout_fn` pattern (clay_layout.h:58-62) is the escape hatch for complex, custom layouts that don't fit neatly into Row/Column/Box primitives. The layout walker checks primitive components first, then falls back to `ClayUI.layout_fn`. This means:

1. Existing code continues to work without changes
2. Developers can mix primitives and custom layout functions in the same tree
3. Complex widgets (scroll containers, split panes, tab bars) can use ClayUI internally while exposing a primitive-based composition externally

**Example of mixing:**

```c
// Custom layout for complex widget
CEL_Clay_Layout(split_pane_layout) {
    CEL_Clay(.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT }) {
        CEL_Clay_ChildAt(0);  // left pane
        CEL_Clay_ChildAt(1);  // right pane
    }
}

// Composition uses ClayUI for the complex part
CEL_Compose(SplitPane) {
    cel_has(ClayUI, .layout_fn = split_pane_layout);
}

// But children can be primitives
Row() {
    SplitPane() {
        Column() { Text(.content = "Left") {} }
        Column() { Text(.content = "Right") {} }
    }
}
```

**Confidence: HIGH** -- verified from the existing dispatch pattern in clay_walk_entity.

---

## How CELS v0.4+ API Specifically Applies

### CEL_Module Pattern

```c
// cels_clay_ui.h (public header)
CEL_Module(Clay_UI);

// cels_clay_ui.c (source)
#include "cels_clay_ui.h"
CEL_Module(Clay_UI, init) {
    cel_module_requires(Clay_Engine);    // Clay arena must be initialized first
    cel_module_provides(UIFramework);    // Declares capability for validation

    // Register all primitive components
    cels_register(RowProps, ColumnProps, BoxProps, TextProps, SpacerProps, ImageProps);

    // Register primitive compositions
    cels_register(Row, Column, Box, Text, Spacer, Image);

    // Layout walker is already registered by Clay_Engine at PreStore
    // Primitive dispatch is added to the existing walker, not a new system
}
```

### CEL_Component Pattern

```c
// Each primitive has its own component with tailored fields
CEL_Component(RowProps) {
    float gap;
    uint8_t main_align;
    uint8_t cross_align;
    Clay_Padding padding;
    Clay_Sizing width;
    Clay_Sizing height;
};

CEL_Component(BoxProps) {
    Clay_Padding padding;
    Clay_Sizing width;
    Clay_Sizing height;
    Clay_Color bg_color;
    Clay_CornerRadius corner_radius;
    CelClayBorderDecor* border;  // NULL = no border
};

CEL_Component(TextProps) {
    const char* content;
    uint16_t font_id;
    uint16_t font_size;
    Clay_Color color;
    uint8_t wrap_mode;
    float letter_spacing;
    float line_height;
    void* text_attr;  // Packed CEL_TextAttr for NCurses bold/dim/etc
};
```

### CEL_Compose + CEL_Define_Composition Pattern

```c
// Header (cels_clay_primitives.h):
CEL_Define_Composition(Row, float gap; uint8_t main_align; uint8_t cross_align;
    Clay_Padding padding; Clay_Sizing width; Clay_Sizing height;);
#define Row(...) cel_init(Row, __VA_ARGS__)

CEL_Define_Composition(Column, float gap; uint8_t main_align; uint8_t cross_align;
    Clay_Padding padding; Clay_Sizing width; Clay_Sizing height;);
#define Column(...) cel_init(Column, __VA_ARGS__)

// Source (cels_clay_primitives.c):
CEL_Composition(Row) {
    cel_has(RowProps,
        .gap = cel.gap,
        .main_align = cel.main_align,
        .cross_align = cel.cross_align,
        .padding = cel.padding,
        .width = cel.width,
        .height = cel.height
    );
}

CEL_Composition(Column) {
    cel_has(ColumnProps,
        .gap = cel.gap,
        .main_align = cel.main_align,
        .cross_align = cel.cross_align,
        .padding = cel.padding,
        .width = cel.width,
        .height = cel.height
    );
}
```

### System Registration Pattern (Renderer)

```c
// clay_sdl3_renderer.c -- follows clay_ncurses_renderer.c pattern exactly
static void clay_sdl3_render(cels_iter_t* it) {
    int count = cels_iter_count(it);
    ClayRenderableData* data = (ClayRenderableData*)cels_iter_column(
        it, ClayRenderableData_id, sizeof(ClayRenderableData));
    for (int i = 0; i < count; i++) {
        if (!data->dirty) continue;
        // Get SDL3 renderer from state
        // Iterate data->render_commands
        // Switch on commandType, draw with SDL3 APIs
    }
}

void clay_sdl3_renderer_init(/* config */) {
    Clay_SetMeasureTextFunction(clay_sdl3_measure_text, NULL);
    ClayRenderableData_register();
    cels_entity_t comp_ids[] = { ClayRenderableData_id };
    cels_system_declare("SDL3_ClayRenderable_ClayRenderableData",
                        CELS_Phase_OnRender, clay_sdl3_render, comp_ids, 1);
}
```

### cel_module_requires Dependency Chain

```
Consumer App
  -> Clay_UI (provides UIFramework)
       -> Clay_Engine (provides LayoutEngine)
            -> cels (core)
  -> NCurses (provides TerminalBackend)    [if terminal app]
       -> cels (core)
  -> SDL3_Engine (provides GraphicsBackend) [if graphical app]
       -> cels (core)
```

### CEL_State: Not Needed for Primitives

UI primitive components are per-entity data, not global state. `CEL_State` (cels.h:375-381) is for singletons like `NCurses_WindowState` or `SDL3_ContextState`. The primitive components use `CEL_Component` (per-entity storage) not `CEL_State` (global singleton).

---

## Clay Property Mapping (Component Fields -> Clay Config)

This is the critical translation layer. Each `emit_*` function in the layout walker must translate component fields to Clay API calls.

### RowProps -> CLAY() with LEFT_TO_RIGHT

| RowProps field | Clay equivalent | Notes |
|----------------|-----------------|-------|
| `.gap` | `Clay_LayoutConfig.childGap` | |
| `.main_align` | `Clay_LayoutConfig.childAlignment.x` | Main axis = horizontal for Row |
| `.cross_align` | `Clay_LayoutConfig.childAlignment.y` | Cross axis = vertical for Row |
| `.padding` | `Clay_LayoutConfig.padding` | Uses Clay_Padding struct directly |
| `.width` | `Clay_LayoutConfig.sizing.width` | Uses Clay_Sizing struct directly |
| `.height` | `Clay_LayoutConfig.sizing.height` | Uses Clay_Sizing struct directly |
| (implicit) | `.layoutDirection = CLAY_LEFT_TO_RIGHT` | Row always LEFT_TO_RIGHT |

**emit_row implementation sketch:**

```c
static void emit_row(ecs_world_t* world, ecs_entity_t entity, const RowProps* p) {
    CLAY({
        .id = _cel_clay_auto_id(__COUNTER__),
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .childGap = p->gap,
            .childAlignment = { .x = p->main_align, .y = p->cross_align },
            .padding = p->padding,
            .sizing = { .width = p->width, .height = p->height },
        }
    }) {
        clay_walk_children(world, entity);
    }
}
```

### ColumnProps -> CLAY() with TOP_TO_BOTTOM

| ColumnProps field | Clay equivalent | Notes |
|-------------------|-----------------|-------|
| `.gap` | `Clay_LayoutConfig.childGap` | |
| `.main_align` | `Clay_LayoutConfig.childAlignment.y` | Main axis = vertical for Column |
| `.cross_align` | `Clay_LayoutConfig.childAlignment.x` | Cross axis = horizontal for Column |
| `.padding` | `Clay_LayoutConfig.padding` | |
| `.width` | `Clay_LayoutConfig.sizing.width` | |
| `.height` | `Clay_LayoutConfig.sizing.height` | |
| (implicit) | `.layoutDirection = CLAY_TOP_TO_BOTTOM` | Column always TOP_TO_BOTTOM |

### BoxProps -> CLAY() + CLAY_RECTANGLE

| BoxProps field | Clay equivalent | Notes |
|----------------|-----------------|-------|
| `.padding` | `Clay_LayoutConfig.padding` | |
| `.width` | `Clay_LayoutConfig.sizing.width` | |
| `.height` | `Clay_LayoutConfig.sizing.height` | |
| `.bg_color` | `Clay_RectangleConfig.color` | Background fill |
| `.corner_radius` | `Clay_RectangleConfig.cornerRadius` | SDL3 renders rounded corners; NCurses ignores |
| `.border` | `.userData = border_decor_ptr` | For NCurses TUI borders via CelClayBorderDecor |

### TextProps -> CLAY_TEXT()

| TextProps field | Clay equivalent | Notes |
|-----------------|-----------------|-------|
| `.content` | `CLAY_STRING()` or `CEL_Clay_Text()` | Static vs dynamic strings |
| `.font_id` | `Clay_TextElementConfig.fontId` | |
| `.font_size` | `Clay_TextElementConfig.fontSize` | Pixel-based for SDL3; ignored for NCurses (1 cell = 1 char) |
| `.color` | `Clay_TextElementConfig.textColor` | |
| `.wrap_mode` | `Clay_TextElementConfig.wrapMode` | |
| `.letter_spacing` | `Clay_TextElementConfig.letterSpacing` | SDL3 only |
| `.line_height` | `Clay_TextElementConfig.lineHeight` | SDL3 only |
| `.text_attr` | `.userData` | Packed CEL_TextAttr for NCurses bold/dim/underline/etc |

### SpacerProps -> CLAY() with no children

| SpacerProps field | Clay equivalent | Notes |
|-------------------|-----------------|-------|
| `.width` | `Clay_LayoutConfig.sizing.width` | Typically `CLAY_SIZING_GROW(0)` |
| `.height` | `Clay_LayoutConfig.sizing.height` | Typically `CLAY_SIZING_GROW(0)` |

### ImageProps -> CLAY_IMAGE()

| ImageProps field | Clay equivalent | Notes |
|------------------|-----------------|-------|
| `.source` | `Clay_ImageElementConfig.imageData` | void* to renderer-specific texture handle |
| `.source_width` | `Clay_ImageElementConfig.sourceDimensions.width` | Original image dimensions |
| `.source_height` | `Clay_ImageElementConfig.sourceDimensions.height` | |

---

## SDL3 Renderer Implementation Notes

The SDL3 Clay renderer must handle the same command types as the NCurses renderer (clay_ncurses_renderer.c:479-527) but in pixel space:

| Command Type | SDL3 Implementation | NCurses Equivalent |
|--------------|---------------------|--------------------|
| RECTANGLE | `SDL_SetRenderDrawColor()` + `SDL_RenderFillRect()` | `tui_draw_fill_rect()` |
| TEXT | TTF text rendering to texture + `SDL_RenderTexture()` | `tui_draw_text()` |
| BORDER | `SDL_RenderRect()` or per-side line drawing | `tui_draw_border()` |
| SCISSOR_START | `SDL_SetRenderClipRect()` with push stack | `tui_push_scissor()` |
| SCISSOR_END | Pop from stack, `SDL_SetRenderClipRect()` to previous | `tui_pop_scissor()` |
| IMAGE | `SDL_RenderTexture()` with texture from `imageData` | Not supported in TUI |

**Key differences from NCurses renderer:**

1. **No aspect ratio compensation.** SDL3 renders in pixels where 1 Clay unit = 1 pixel. No `cell_aspect_ratio` scaling needed. The NCurses renderer scales horizontal values by `g_theme->cell_aspect_ratio` (default 2.0) because terminal cells are roughly 2x taller than wide.

2. **No parent_bg scan.** The NCurses renderer scans backward through render commands to find the nearest parent rectangle's background color (find_parent_bg, clay_ncurses_renderer.c:206-226) because ncurses needs explicit background color for text. SDL3 has alpha blending and does not need this.

3. **Corner radius support.** SDL3 can render rounded rectangles natively. NCurses maps `cornerRadius > 0` to `TUI_BORDER_ROUNDED` (rounded box-drawing characters).

4. **Image support.** SDL3 renders images via `SDL_RenderTexture()`. NCurses skips image commands entirely.

**Key similarity to NCurses renderer:**

Both renderers:
1. Query `ClayRenderableData` from the singleton entity
2. Check the `dirty` flag
3. Iterate `render_commands` array in order
4. Switch on `commandType`
5. Use their respective drawing primitives

The system callback signature is identical:
```c
static void clay_sdl3_render(cels_iter_t* it);
```

**How to access the SDL3 renderer handle:** Read `SDL3_ContextState` via `cel_read(SDL3_ContextState)` to check initialization status, then access the SDL_Renderer through the SDL3 context. The exact mechanism depends on what cels-sdl3 exposes -- if it does not currently expose the SDL_Renderer pointer, a new state field or accessor function will be needed. This is the one area requiring investigation during implementation.

**Confidence: MEDIUM for SDL3 specifics** -- The NCurses renderer pattern is fully verified. The SDL3 renderer follows the same pattern architecturally, but the specific SDL3 drawing API calls (particularly SDL3_ttf text rendering and rounded rectangle support) need verification against SDL3 headers. Also, cels-sdl3 may need a small extension to expose the SDL_Renderer handle.

---

## What NOT to Use

| Technology/Approach | Why Not |
|---------------------|---------|
| Runtime renderer interface (vtable) | Over-engineering. `ClayRenderableData` is already the interface contract. Conditional compilation is simpler, faster, and proven in the existing CMakeLists.txt. |
| Generic UIComponent with type enum | Loses ECS archetype benefits. Per-primitive components enable targeted queries, smaller memory per entity, and dispatch by component presence without enum overhead. |
| Separate cels-clay-sdl3 / cels-clay-ncurses libraries | Fragmenting. The renderers are 500-700 lines each. Conditional compilation in one library is simpler than managing 3 CMake targets with diamond dependencies. |
| Clay's built-in border system for TUI | Verified broken for terminal (clay_ncurses_renderer.c:330-336): Clay border widths are uint16_t values that get aspect-ratio-scaled to 2+ cells. The `CelClayBorderDecor` via `userData` pattern is proven. Keep it. |
| imgui or another immediate-mode UI library | Philosophical mismatch. CELS is entity-based with reactivity. Immediate-mode UI has no entity identity, no lifecycle, no persistence. The point of v0.6 is making UI entities first-class. |
| Yoga or another layout engine | Clay is already integrated, single-header, C99, handles flex/scroll/float/text-wrap. Switching layout engines would be a full rewrite for zero proven benefit. |
| Removing ClayUI/CEL_Clay_Layout in v0.6 | Breaking backward compatibility unnecessarily. The old pattern is the escape hatch for complex custom layouts. Keep both paths; let developers migrate at their own pace. |

---

## Build Configuration

No new external dependencies. Everything is already available or conditionally compiled:

```cmake
# cels-clay CMakeLists.txt structure for v0.6:

# Core sources (always compiled)
target_sources(cels-clay INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_layout.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_render.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_primitives.c      # NEW: primitive compositions + emit_* functions
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_ui_module.c        # NEW: Clay_UI CEL_Module
)

# Conditional NCurses renderer (existing)
if(TARGET cels-ncurses)
    target_sources(cels-clay INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_ncurses_renderer.c
    )
    target_link_libraries(cels-clay INTERFACE cels-ncurses)
endif()

# Conditional SDL3 renderer (NEW)
if(TARGET cels-sdl3)
    target_sources(cels-clay INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_sdl3_renderer.c
    )
    target_link_libraries(cels-clay INTERFACE cels-sdl3)
endif()
```

### Consumer CMakeLists.txt

```cmake
# For a terminal app:
target_link_libraries(my_tui_app PRIVATE cels cels-ncurses cels::clay)

# For an SDL3 app:
target_link_libraries(my_sdl_app PRIVATE cels cels-sdl3 cels::clay)

# For an app supporting both (e.g., testing):
target_link_libraries(my_app PRIVATE cels cels-ncurses cels-sdl3 cels::clay)
```

---

## Version Compatibility

| Dependency | Minimum Version | Verified In |
|------------|----------------|-------------|
| CELS | v0.4+ | cels.h: CEL_Module, CEL_Component, CEL_Compose, cels_system_declare all present and verified |
| Clay | v0.14 | CMakeLists.txt line 39: `GIT_TAG v0.14` |
| CMake | 3.21+ | CMakeLists.txt line 17 |
| C standard | C99 | Clay and CELS both require C99 designated initializers |
| SDL3 | any (via cels-sdl3) | cels-sdl3 handles SDL3 version pinning |
| NCurses | 6.x wide (via cels-ncurses) | cels-ncurses handles NCurses version pinning |

---

## Sources

All recommendations verified from direct source code inspection:

| Source | What Was Verified |
|--------|-------------------|
| `cels/cels.h` (1097 lines) | CEL_Module, CEL_Component, CEL_Compose, CEL_System, CEL_Define_Composition, cel_has, cel_init, cels_register, cel_module_requires, cel_module_provides |
| `cels/private/cels_macros.h` (181 lines) | cel_init/cel_compose expansion, composition factory pattern, _cel_compose_impl |
| `cels/private/cels_runtime.h` (463 lines) | cels_system_declare, cels_component_register, cels_module_register, cels_iter_column, cels_iter_count |
| `cels/private/cels_system_impl.h` (614 lines) | CEL_System two-pass pattern, cel_query, cel_each, cel_update, phase aliases |
| `cels-clay/src/clay_layout.c` (574 lines) | Entity tree walker, clay_walk_entity, clay_walk_children, auto-ID, ClaySurfaceConfig, frame arena |
| `cels-clay/src/clay_render.c` (173 lines) | ClayRenderableData, dispatch system, singleton entity, render bridge |
| `cels-clay/src/clay_engine.c` (182 lines) | Clay_Engine module init, arena management, system registration order |
| `cels-clay/src/clay_ncurses_renderer.c` (720 lines) | Renderer system via cels_system_declare, command iteration, text measurement, coordinate mapping, border decor, parent_bg scan |
| `cels-clay/include/cels-clay/clay_layout.h` (255 lines) | ClayUI component, ClaySurface composition, CEL_Clay macros, CEL_Clay_Children |
| `cels-clay/include/cels-clay/clay_render.h` (115 lines) | ClayRenderableData struct, CelClayBorderDecor, feature register |
| `cels-clay/include/cels-clay/clay_ncurses_renderer.h` (168 lines) | ClayNcursesTheme, renderer init API, scroll input handler |
| `cels-clay/CMakeLists.txt` (88 lines) | Conditional compilation pattern for optional renderers |
| `cels-sdl3/include/cels_sdl3.h` (75 lines) | SDL3_Engine module pattern, SDL3_ContextConfig, SDL3_ContextState, SDL3Context composition |
| `cels-ncurses/include/cels_ncurses.h` (203 lines) | NCurses module pattern, TUI_Renderable, TUI_SurfaceConfig, NCursesWindow + TUISurface compositions |
