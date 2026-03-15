# Architecture: cels-clay v0.6 Entity-Based UI

**Domain:** Compose-Inspired Entity-Based UI Framework (CELS + Clay)
**Researched:** 2026-03-15
**Confidence:** HIGH (derived from CELS v0.4 API, Clay API, cels-sdl3/cels-ncurses module patterns, v0.5 implementation)

## Overview

v0.6 replaces the layout-function model with a pure entity/component model. The architecture has four layers:

1. **Entity Layer** — Compositions (Row, Column, Box, Text, Spacer, Image) create entities with per-type property components
2. **Layout Layer** — A system walks the entity tree, reads property components, and emits `CLAY()` calls to build the layout tree
3. **Render Bridge** — Clay produces a `Clay_RenderCommandArray`; the bridge makes it available to the active renderer
4. **Renderer Layer** — SDL3 or NCurses renderer system consumes render commands and draws

## Component Model

### Per-Entity Property Components

Each entity type has a tailored component struct:

```c
CEL_Component(RowProps) {
    uint16_t gap;
    Clay_Padding padding;
    Clay_SizingAxis width, height;
    Clay_ChildAlignment_x main_align;
    Clay_ChildAlignment_y cross_align;
    Clay_Color bg;
    bool clip;
};

CEL_Component(ColumnProps) { /* same fields, axes swapped */ };
CEL_Component(BoxProps)    { /* + border, corner_radius */ };
CEL_Component(TextProps)   { /* text, color, font_size, wrap, etc. */ };
CEL_Component(SpacerProps) { /* width, height only */ };
CEL_Component(ImageProps)  { /* source, dimensions, bg */ };
```

### Entity Type Discovery

The layout walker checks component presence to determine entity type:

```c
if (cel_get(RowProps))         → emit CLAY({LEFT_TO_RIGHT, ...}) { children }
else if (cel_get(ColumnProps)) → emit CLAY({TOP_TO_BOTTOM, ...}) { children }
else if (cel_get(BoxProps))    → emit CLAY({...}) { children }
else if (cel_get(TextProps))   → emit CLAY_TEXT(...)
else if (cel_get(SpacerProps)) → emit CLAY({sizing}) {}
else if (cel_get(ImageProps))  → emit CLAY({.image = ...}) {}
else                           → transparent passthrough (emit children directly)
```

### Compositions

Each primitive is a CELS composition following the proven pattern:

```c
CEL_Define_Composition(Row, /* fields = RowProps fields */);

CEL_Composition(Row) {
    cel_has(RowProps,
        .gap = props.gap,
        .padding = props.padding,
        .width = props.width,
        .height = props.height,
        .main_align = props.main_align,
        .cross_align = props.cross_align,
        .bg = props.bg,
        .clip = props.clip
    );
}

#define Row(...) cel_init(Row, __VA_ARGS__)
```

Developer usage:
```c
Row(.gap = 2, .padding = CLAY_PADDING_ALL(1)) {
    Text(.text = "Hello", .color = (Clay_Color){255,255,255,255}) {}
    Spacer() {}
    Text(.text = "World") {}
}
```

## Frame Pipeline

### System Phases

| Phase | System | Responsibility |
|-------|--------|---------------|
| OnLoad | (app systems) | Read input, update state |
| (reactive) | CELS recomposition | Dirty compositions re-execute, entity tree updated |
| OnUpdate | (app systems) | Process input, mutate state via `cel_update()` |
| PreStore | `ClayLayoutSystem` | `Clay_BeginLayout` → walk entity tree → `Clay_EndLayout` |
| OnRender | `Clay_NCursesRenderSystem` OR `Clay_SDL3RenderSystem` | Consume render commands, draw to backend |

### Layout System (PreStore)

```
Clay_SetLayoutDimensions(surface_width, surface_height)
Clay_BeginLayout()
    for each root entity with ClaySurface:
        walk_entity_tree(entity):
            check component type (Row/Column/Box/Text/Spacer/Image)
            emit corresponding CLAY() call
            recursively walk children
Clay_EndLayout() → returns Clay_RenderCommandArray
store render commands for OnRender consumption
```

The tree walk is the same depth-first parent-before-child algorithm from v0.5. The only change is reading property components instead of calling layout function pointers.

### Render System (OnRender)

One of two systems runs based on which renderer module was registered:

**NCurses path:**
```
Clay_NCursesRenderSystem:
    get Clay_RenderCommandArray
    for each command:
        switch(command.type):
            RECTANGLE → tui_draw_fill_rect()
            TEXT → tui_draw_text()
            BORDER → tui_draw_border()
            SCISSOR → tui_push_scissor() / tui_pop_scissor()
```

**SDL3 path:**
```
Clay_SDL3RenderSystem:
    get Clay_RenderCommandArray
    for each command:
        switch(command.type):
            RECTANGLE → SDL_RenderFillRect()
            TEXT → TTF_RenderText() + SDL_RenderTexture()
            BORDER → SDL_RenderRect() (per side)
            SCISSOR → SDL_SetRenderClipRect()
            IMAGE → SDL_RenderTexture()
```

## Module Architecture

### Registration Pattern

Following cels-sdl3/cels-ncurses module pattern:

```c
// Core module (always registered)
CEL_Module(Clay_Engine) {
    cels_register(ClayLayoutSystem);
    cels_register(ClaySurface);
    cels_register(RowProps, ColumnProps, BoxProps, TextProps, SpacerProps, ImageProps);
    // State, lifecycle for arena management
}

// Renderer modules (developer picks one)
CEL_Module(Clay_NCurses) {
    cels_register(Clay_NCursesRenderSystem);
    cels_register(Clay_NCursesTextMeasure);
}

CEL_Module(Clay_SDL3) {
    cels_register(Clay_SDL3RenderSystem);
    cels_register(Clay_SDL3TextMeasure);
}
```

Developer app:
```c
CEL_Build {
    cels_register(NCurses);       // terminal backend
    cels_register(Clay_Engine);   // core layout
    cels_register(Clay_NCurses);  // terminal renderer for Clay

    NCursesWindow(.title = "App", .fps = 30) {
        ClaySurface(.width = 80, .height = 24) {
            // UI compositions here
        }
    }
}
```

### State Singletons

```c
CEL_Define_State(ClayEngineState) {
    bool initialized;
    float layout_width, layout_height;
    Clay_RenderCommandArray render_commands;
};
```

Registered via cross-TU pointer registry (same pattern as `SDL3_ContextState`, `NCurses_WindowState`).

### Lifecycle

```c
CEL_Lifecycle(ClayEngineLC);
CEL_Observe(ClayEngineLC, on_create) {
    // Initialize Clay arena, set error handler
    clay_arena_init(config);
    Clay_Initialize(arena, config, error_handler);
}
CEL_Observe(ClayEngineLC, on_destroy) {
    // Free Clay arena
    clay_arena_free();
}
```

## Data Flow

```
Developer writes:
    Row(.gap = 2) {
        Text(.text = "Hello") {}
        Column() {
            Text(.text = "A") {}
            Text(.text = "B") {}
        }
    }

CELS creates:
    Entity 1: RowProps{gap=2}
        Entity 2: TextProps{text="Hello"}
        Entity 3: ColumnProps{}
            Entity 4: TextProps{text="A"}
            Entity 5: TextProps{text="B"}

Layout system emits:
    CLAY({LEFT_TO_RIGHT, gap=2}) {
        CLAY_TEXT("Hello", config)
        CLAY({TOP_TO_BOTTOM}) {
            CLAY_TEXT("A", config)
            CLAY_TEXT("B", config)
        }
    }

Clay produces:
    RenderCommand[0]: RECTANGLE (row background)
    RenderCommand[1]: TEXT "Hello" at (x1,y1)
    RenderCommand[2]: RECTANGLE (column background)
    RenderCommand[3]: TEXT "A" at (x2,y2)
    RenderCommand[4]: TEXT "B" at (x3,y3)

Renderer draws:
    NCurses: tui_draw_fill_rect(), tui_draw_text(), ...
    SDL3:    SDL_RenderFillRect(), TTF_RenderText(), ...
```

## Renderer Coexistence

### Conditional Compilation

```cmake
# In cels-clay CMakeLists.txt
if(TARGET cels-ncurses)
    target_sources(cels-clay INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/clay_ncurses_renderer.c
    )
endif()

if(TARGET cels-sdl3)
    target_sources(cels-clay INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/clay_sdl3_renderer.c
    )
endif()
```

### Text Measurement Ownership

Critical: Clay has ONE global text measurement callback. The renderer that is registered sets it:

- `Clay_NCurses` sets cell-based measurement (1 char = 1 unit width)
- `Clay_SDL3` sets TTF-based measurement (actual pixel widths from font metrics)

The text measurement callback must be set BEFORE the layout pass (PreStore). The renderer module's init observer handles this.

### Mutual Exclusion

Only one renderer module should be registered. If both are registered, the last one wins (its text measurement callback overwrites the previous). This is acceptable — document that registering both is undefined behavior.

## File Structure (Proposed)

```
cels-clay/
├── CMakeLists.txt
├── include/cels-clay/
│   ├── clay_engine.h          # CEL_Module(Clay_Engine), state, lifecycle
│   ├── clay_primitives.h      # Row, Column, Box, Text, Spacer, Image compositions + props
│   ├── clay_surface.h         # ClaySurface composition
│   ├── clay_ncurses.h         # CEL_Module(Clay_NCurses) — renderer module
│   └── clay_sdl3.h            # CEL_Module(Clay_SDL3) — renderer module
├── src/
│   ├── clay_impl.c            # CLAY_IMPLEMENTATION (unchanged)
│   ├── clay_engine.c          # Module init, arena, state singleton
│   ├── clay_layout.c          # Layout system — property-driven tree walk
│   ├── clay_primitives.c      # Composition definitions for Row, Column, etc.
│   └── renderers/
│       ├── clay_ncurses_renderer.c  # NCurses render system
│       └── clay_sdl3_renderer.c     # SDL3 render system
└── examples/
    └── minimal/
        ├── main_ncurses.c     # NCurses example
        └── main_sdl3.c        # SDL3 example
```

## Build Order (Phase Dependencies)

1. **Module refactor** — CEL_Module, lifecycle, state singletons (foundation)
2. **Property components** — RowProps, ColumnProps, BoxProps, TextProps, SpacerProps (no render needed)
3. **Layout system rewrite** — Property-driven tree walk (can test with existing ncurses renderer)
4. **ClaySurface adaptation** — Wire to new layout system
5. **NCurses renderer migration** — Port existing code to new module pattern
6. **SDL3 renderer** — New implementation using Clay's SDL3 renderer as reference
7. **ImageProps** — Needs SDL3 renderer to be meaningful
8. **Renderer selection** — Module registration wiring
9. **Minimal example** — Dual-backend demo app

## Key Risks

1. **Entity type dispatch overhead** — 6 component checks per entity per frame. Mitigated: ECS archetype queries are O(1) component lookups.
2. **Text measurement conflict** — Two renderers, one callback slot. Mitigated: last-registered wins, document single-renderer-only.
3. **SDL3 renderer state access** — Need SDL_Renderer/TTF_Font from cels-sdl3. May need cels-sdl3 to expose these via state singleton.
4. **INTERFACE library multi-TU** — Same issues as v0.5 with static/extern linkage. Apply proven solutions from v0.5 experience.

---
*Researched: 2026-03-15*
