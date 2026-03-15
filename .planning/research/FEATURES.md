# Feature Landscape

**Domain:** Compose-Inspired Entity-Based UI Framework (CELS + Clay)
**Researched:** 2026-03-15
**Overall confidence:** HIGH (based on direct source analysis of cels-clay v0.5, CELS v0.4 API, Clay layout properties, cels-sdl3/cels-ncurses module patterns)

## Context: v0.5 → v0.6 Paradigm Shift

v0.5 uses **layout functions**: compositions attach a `ClayUI` component with a function pointer. A system walks the entity tree, calling each function to emit raw `CLAY()` macro blocks.

v0.6 replaces this with an **entity/property model**: compositions instantiate Row, Column, Box, Text entities with per-type property components. The layout system reads these components and generates `CLAY()` calls internally. Developers never touch Clay macros directly.

---

## Table Stakes

### 1. Row Composition + RowProps Component

| Aspect | Detail |
|--------|--------|
| What | Horizontal layout container. Children flow left-to-right. |
| Clay mapping | `.layout.layoutDirection = CLAY_LEFT_TO_RIGHT` |
| Complexity | Low |

**RowProps fields:**

| Field | Type | Clay Mapping | Default |
|-------|------|-------------|---------|
| `gap` | `uint16_t` | `.layout.childGap` | `0` |
| `padding` | `Clay_Padding` | `.layout.padding` | `{0}` |
| `width` | `Clay_SizingAxis` | `.layout.sizing.width` | `CLAY_SIZING_GROW(0)` |
| `height` | `Clay_SizingAxis` | `.layout.sizing.height` | `CLAY_SIZING_GROW(0)` |
| `main_align` | `Clay_ChildAlignment_x` | `.layout.childAlignment.x` | `CLAY_ALIGN_X_LEFT` |
| `cross_align` | `Clay_ChildAlignment_y` | `.layout.childAlignment.y` | `CLAY_ALIGN_Y_TOP` |
| `bg` | `Clay_Color` | `.backgroundColor` | `{0,0,0,0}` |
| `clip` | `bool` | `.clip` | `false` |

### 2. Column Composition + ColumnProps Component

| Aspect | Detail |
|--------|--------|
| What | Vertical layout container. Children flow top-to-bottom. |
| Clay mapping | `.layout.layoutDirection = CLAY_TOP_TO_BOTTOM` |
| Complexity | Low |

**ColumnProps fields:** Same structure as RowProps but with main/cross axes swapped:

| Field | Type | Clay Mapping | Default |
|-------|------|-------------|---------|
| `gap` | `uint16_t` | `.layout.childGap` | `0` |
| `padding` | `Clay_Padding` | `.layout.padding` | `{0}` |
| `width` | `Clay_SizingAxis` | `.layout.sizing.width` | `CLAY_SIZING_GROW(0)` |
| `height` | `Clay_SizingAxis` | `.layout.sizing.height` | `CLAY_SIZING_GROW(0)` |
| `main_align` | `Clay_ChildAlignment_y` | `.layout.childAlignment.y` | `CLAY_ALIGN_Y_TOP` |
| `cross_align` | `Clay_ChildAlignment_x` | `.layout.childAlignment.x` | `CLAY_ALIGN_X_LEFT` |
| `bg` | `Clay_Color` | `.backgroundColor` | `{0,0,0,0}` |
| `clip` | `bool` | `.clip` | `false` |

### 3. Box Composition + BoxProps Component

| Aspect | Detail |
|--------|--------|
| What | General-purpose container. Stacks children. Supports borders, background, corner radius. |
| Clay mapping | Container element, `CLAY_TOP_TO_BOTTOM` with children using full bounding box |
| Complexity | Medium |

**BoxProps fields:**

| Field | Type | Clay Mapping | Default |
|-------|------|-------------|---------|
| `padding` | `Clay_Padding` | `.layout.padding` | `{0}` |
| `width` | `Clay_SizingAxis` | `.layout.sizing.width` | `CLAY_SIZING_GROW(0)` |
| `height` | `Clay_SizingAxis` | `.layout.sizing.height` | `CLAY_SIZING_GROW(0)` |
| `align_x` | `Clay_ChildAlignment_x` | `.layout.childAlignment.x` | `CLAY_ALIGN_X_LEFT` |
| `align_y` | `Clay_ChildAlignment_y` | `.layout.childAlignment.y` | `CLAY_ALIGN_Y_TOP` |
| `bg` | `Clay_Color` | `.backgroundColor` | `{0,0,0,0}` |
| `border` | `Clay_Border` | `.border` | `{0}` |
| `corner_radius` | `Clay_CornerRadius` | `.cornerRadius` | `{0}` |
| `clip` | `bool` | `.clip` | `false` |

### 4. Text Composition + TextProps Component

| Aspect | Detail |
|--------|--------|
| What | Leaf node that renders text. No children. |
| Clay mapping | `CLAY_TEXT(Clay_String, CLAY_TEXT_CONFIG(...))` |
| Complexity | Low-Medium |

**TextProps fields:**

| Field | Type | Clay Mapping | Default |
|-------|------|-------------|---------|
| `text` | `const char*` | First arg to `CLAY_TEXT` | `""` |
| `color` | `Clay_Color` | `.textColor` | `{255,255,255,255}` |
| `font_size` | `uint16_t` | `.fontSize` | `16` |
| `font_id` | `uint16_t` | `.fontId` | `0` |
| `letter_spacing` | `uint16_t` | `.letterSpacing` | `0` |
| `line_height` | `uint16_t` | `.lineHeight` | `0` |
| `wrap` | `Clay_TextElementConfigWrapMode` | `.wrapMode` | `CLAY_TEXT_WRAP_WORDS` |

**Design decision:** Text accepts `const char*` — the layout system converts to `Clay_String` via the frame arena (`CEL_Clay_Text`). Always copies to arena for lifetime safety.

### 5. Spacer Composition + SpacerProps Component

| Aspect | Detail |
|--------|--------|
| What | Invisible element that fills available space. No visual output. |
| Clay mapping | `CLAY({ .layout.sizing = {...} })` with no children/background |
| Complexity | Low |

**SpacerProps fields:**

| Field | Type | Clay Mapping | Default |
|-------|------|-------------|---------|
| `width` | `Clay_SizingAxis` | `.layout.sizing.width` | `CLAY_SIZING_GROW(0)` |
| `height` | `Clay_SizingAxis` | `.layout.sizing.height` | `CLAY_SIZING_GROW(0)` |

### 6. Image Composition + ImageProps Component

| Aspect | Detail |
|--------|--------|
| What | Displays an image (SDL3) or colored placeholder (NCurses). Leaf node. |
| Clay mapping | `CLAY({ .image = { .imageData, .sourceDimensions } })` |
| Complexity | Medium |

**ImageProps fields:**

| Field | Type | Clay Mapping | Default |
|-------|------|-------------|---------|
| `source` | `void*` | `.image.imageData` | `NULL` |
| `source_width` | `float` | `.image.sourceDimensions.width` | `0` |
| `source_height` | `float` | `.image.sourceDimensions.height` | `0` |
| `width` | `Clay_SizingAxis` | `.layout.sizing.width` | Fit to source |
| `height` | `Clay_SizingAxis` | `.layout.sizing.height` | Fit to source |
| `bg` | `Clay_Color` | `.backgroundColor` | `{0,0,0,0}` |
| `corner_radius` | `Clay_CornerRadius` | `.cornerRadius` | `{0}` |

### 7. ClaySurface Root Composition

| Aspect | Detail |
|--------|--------|
| What | Root layout boundary. Owns `Clay_BeginLayout` / `Clay_EndLayout`. Already exists in v0.5. |
| Changes for v0.6 | Adapt to modern CELS module pattern. Surface props set layout dimensions. |
| Complexity | Low (evolution of existing) |

### 8. Layout System Rewrite

| Aspect | Detail |
|--------|--------|
| What | Rewrite tree walker to read property components instead of calling layout function pointers. Same depth-first algorithm. |
| Complexity | Medium |

### 9. SDL3 Renderer

| Aspect | Detail |
|--------|--------|
| What | Translates Clay render commands to SDL3 draw calls. Conditionally compiled when cels-sdl3 target exists. |
| Complexity | Medium-High |

### 10. NCurses Renderer Migration

| Aspect | Detail |
|--------|--------|
| What | Existing renderer adapted to new module pattern. Already works — mostly structural migration. |
| Complexity | Low-Medium |

### 11. Renderer Selection via Module Registration

| Aspect | Detail |
|--------|--------|
| What | `cels_register(Clay_NCurses)` or `cels_register(Clay_SDL3)` selects the active renderer. |
| Complexity | Medium |

### 12. Modern CELS Module Pattern

| Aspect | Detail |
|--------|--------|
| What | Convert to CELS v0.4+: `CEL_Module`, `CEL_Lifecycle`, `CEL_System`, state singletons with cross-TU registry. |
| Complexity | Low |

---

## Differentiators

### 1. Transparent Composition Passthrough
Compositions without layout components pass children through to parent Clay scope. Enables logical grouping without visual impact. Already works in v0.5 — must preserve.

### 2. Border Decoration Component
Separate `BorderDecor` component addable to any container for border styling. Separates border from layout props. v0.5 `CelClayBorderDecor` pattern.

### 3. Cell-Aware Sizing Helpers
`CELL_W(n)`, `CELL_H(n)`, `CELL_PAD(l,r,t,b)` convenience macros for terminal developers. Deferred from v0.5 (API-06).

---

## Anti-Features

### 1. Modifier Chain Pattern
Compose's `.modifier(Modifier.padding(8).background(Color.Blue))` requires Kotlin language features. C99 designated initializers are the right idiom.

### 2. Widget Library
Buttons, TextInput, Slider, Toggle — app-level compositions built from primitives.

### 3. ListView / ScrollView
Efficient virtualized lists — deferred to future milestone. Needs scroll tracking, item recycling, focus management.

### 4. Layout Functions
v0.6 is pure entity model. No `ClayUI.layout_fn` escape hatch for this milestone.

### 5. Animation System
Application-level concern. CELS systems with delta time handle interpolation.

### 6. Mouse / Pointer Interaction
Keyboard-only for now. Design for future but do not implement.

### 7. Wrapping Clay Macros
Don't create `CELS_SIZING_GROW` wrappers. Property structs use Clay types directly. Developers use Clay sizing/padding macros as values.

---

## Feature Dependencies

```
    Module Pattern (12)
           |
    +------+------+
    |             |
Components (1-6)  Layout System (8)
    |             |
    +------+------+
           |
    ClaySurface (7)
           |
    +------+------+
    |             |
NCurses (10)  SDL3 (9)
    |             |
    +------+------+
           |
  Renderer Selection (11)
```

**Critical path:** 12 → 1-6 + 8 → 7 → 10 + 9 → 11

---

## DX Comparison: v0.5 vs v0.6

### v0.5 (14 lines)
```c
CEL_Clay_Layout(sidebar_layout) {
    (void)world; (void)self;
    CEL_Clay(
        .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { .width = CLAY_SIZING_PERCENT(0.25f),
                                .height = CLAY_SIZING_GROW(0) },
                    .padding = CLAY_PADDING_ALL(1) },
        .backgroundColor = (Clay_Color){ 25, 28, 38, 255 }
    ) { CEL_Clay_Children(); }
}
CEL_Composition(Sidebar) {
    (void)props;
    cel_has(ClayUI, .layout_fn = sidebar_layout);
}
```

### v0.6 (7 lines)
```c
CEL_Compose(Sidebar) {
    cel_has(ColumnProps,
        .width = CLAY_SIZING_PERCENT(0.25f),
        .height = CLAY_SIZING_GROW(0),
        .padding = CLAY_PADDING_ALL(1),
        .bg = (Clay_Color){ 25, 28, 38, 255 }
    );
}
```

~50% code reduction per composition. No layout functions, no boilerplate.

---

## Sources

- cels-clay v0.5 codebase (direct analysis)
- CELS v0.4 API header (`/home/cachy/workspaces/libs/cels/include/cels/cels.h`)
- Clay layout API (derived from demo usage and clay.h)
- cels-sdl3 module pattern (`/home/cachy/workspaces/libs/cels-sdl3/`)
- cels-ncurses module pattern (`/home/cachy/workspaces/libs/cels-ncurses/`)
- Jetpack Compose design patterns (Row, Column, Box, Spacer, Text)
- Godot Container nodes (HBoxContainer, VBoxContainer, Control)

---
*Researched: 2026-03-15*
