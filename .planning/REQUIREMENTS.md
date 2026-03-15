# Requirements: cels-clay v0.6

**Defined:** 2026-03-15
**Core Value:** Declarative UI development where developers build interfaces by composing entities with layout properties — CELS handles state and reactivity, Clay handles layout math, and the renderer is a pluggable detail.

## v1 Requirements

### Module Infrastructure

- [ ] **MOD-01**: CEL_Module(Clay_Engine) facade with CELS v0.4+ patterns (replaces _CEL_DefineModule)
- [ ] **MOD-02**: CEL_Lifecycle for Clay arena init/cleanup (replaces atexit)
- [ ] **MOD-03**: ClayEngineState singleton with cross-TU registry (initialized, layout dimensions, render commands)
- [ ] **MOD-04**: Remove deprecated Feature/Provider pattern (ClayRenderable, _CEL_Provides, ClayRenderableData)
- [ ] **MOD-05**: ClaySurface composition adapted to modern module pattern

### Entity Primitives

- [ ] **PRIM-01**: Row composition + RowProps component (horizontal layout, gap, padding, sizing, alignment, bg, clip)
- [ ] **PRIM-02**: Column composition + ColumnProps component (vertical layout, same fields as Row with axes swapped)
- [ ] **PRIM-03**: Box composition + BoxProps component (general container, borders, corner radius, background)
- [ ] **PRIM-04**: Text composition + TextProps component (text content, color, font, wrap, letter spacing)
- [ ] **PRIM-05**: Spacer composition + SpacerProps component (invisible space filler, width/height sizing)
- [ ] **PRIM-06**: Image composition + ImageProps component (opaque source pointer, dimensions, SDL3 renders image, NCurses renders placeholder)

### Layout System

- [ ] **LAYOUT-01**: Property-driven tree walk reads entity components instead of calling layout function pointers
- [ ] **LAYOUT-02**: Entity type dispatch via component presence (RowProps -> Row CLAY, ColumnProps -> Column CLAY, etc.)
- [ ] **LAYOUT-03**: Transparent passthrough for entities without layout components (children emitted into parent scope)
- [ ] **LAYOUT-04**: Depth-first parent-before-child walk order preserved from v0.5

### Renderers

- [ ] **REND-01**: NCurses renderer migrated to CEL_Module(Clay_NCurses) pattern with own text measurement callback
- [ ] **REND-02**: SDL3 renderer as CEL_Module(Clay_SDL3) translating Clay render commands to SDL3 draw calls
- [ ] **REND-03**: SDL3 rectangle rendering (SDL_RenderFillRect with color)
- [ ] **REND-04**: SDL3 text rendering (TTF_RenderText + SDL_RenderTexture)
- [ ] **REND-05**: SDL3 border rendering (per-side line drawing)
- [ ] **REND-06**: SDL3 scissor/clipping support (SDL_SetRenderClipRect)
- [ ] **REND-07**: SDL3 image rendering (SDL_RenderTexture from imageData pointer)
- [ ] **REND-08**: Renderer selection via module registration (cels_register(Clay_NCurses) or cels_register(Clay_SDL3))
- [ ] **REND-09**: Conditional compilation — renderers compile only when backend target exists

### Example App

- [ ] **DEMO-01**: Minimal example app using Row, Column, Box, Text, Spacer entities
- [ ] **DEMO-02**: Example renders correctly on NCurses backend
- [ ] **DEMO-03**: Example renders correctly on SDL3 backend
- [ ] **DEMO-04**: Same entity tree used for both backends (only module registration differs)

## v2 Requirements

### Advanced Primitives

- **PRIM2-01**: ScrollView composition with scroll state management
- **PRIM2-02**: ListView composition with virtualization and item recycling
- **PRIM2-03**: Floating element support (dropdowns, tooltips, modals)

### Interaction

- **INT-01**: Mouse pointer support (Clay_SetPointerState wired to input events)
- **INT-02**: Clay_Hovered() / Clay_OnHover() interaction model
- **INT-03**: Focus management system for keyboard navigation

### Rendering

- **REND2-01**: 256-color and true color support for terminal renderer
- **REND2-02**: Damage tracking / dirty rectangle optimization
- **REND2-03**: Sub-cell rendering modes for NCurses (half-block, quadrant, braille)

### API

- **API-01**: Cell-aware sizing helpers (CELL_W, CELL_H, CELL_PAD macros)
- **API-02**: Theme/style inheritance component
- **API-03**: Layout function escape hatch for custom/complex layouts
- **API-04**: Debug overlay component (layout bounds visualization)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Widget library (Button, TextInput, Slider) | App-level compositions built from primitives |
| Modifier chain pattern | Requires Kotlin language features not available in C |
| Animation/tweening system | Application-level concern using CELS delta time |
| Text editing/input fields | Widget-level concern requiring cursor/selection state |
| Multi-window / multi-surface | One ClaySurface per app is correct for v0.6 |
| Wrapping Clay macros (CELS_SIZING_GROW) | Property structs use Clay types directly |
| True color NCurses | 8/16-color sufficient; design for future expansion |
| Layout functions in v0.6 | Pure entity model — layout functions deferred to v2 API-03 |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| MOD-01 | Phase 1 | Pending |
| MOD-02 | Phase 1 | Pending |
| MOD-03 | Phase 1 | Pending |
| MOD-04 | Phase 1 | Pending |
| MOD-05 | Phase 1 | Pending |
| PRIM-01 | Phase 2 | Pending |
| PRIM-02 | Phase 2 | Pending |
| PRIM-03 | Phase 2 | Pending |
| PRIM-04 | Phase 2 | Pending |
| PRIM-05 | Phase 2 | Pending |
| PRIM-06 | Phase 2 | Pending |
| LAYOUT-01 | Phase 3 | Pending |
| LAYOUT-02 | Phase 3 | Pending |
| LAYOUT-03 | Phase 3 | Pending |
| LAYOUT-04 | Phase 3 | Pending |
| REND-01 | Phase 4 | Pending |
| REND-02 | Phase 5 | Pending |
| REND-03 | Phase 5 | Pending |
| REND-04 | Phase 5 | Pending |
| REND-05 | Phase 5 | Pending |
| REND-06 | Phase 5 | Pending |
| REND-07 | Phase 5 | Pending |
| REND-08 | Phase 4 | Pending |
| REND-09 | Phase 4 | Pending |
| DEMO-01 | Phase 6 | Pending |
| DEMO-02 | Phase 6 | Pending |
| DEMO-03 | Phase 6 | Pending |
| DEMO-04 | Phase 6 | Pending |

**Coverage:**
- v1 requirements: 28 total
- Mapped to phases: 28
- Unmapped: 0 ✓

---
*Requirements defined: 2026-03-15*
*Last updated: 2026-03-15 after initial definition*
