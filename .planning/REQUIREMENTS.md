# Requirements: cels-clay

**Defined:** 2026-02-07
**Core Value:** Declarative UI development where CELS handles application state and reactivity while Clay handles layout

## v1 Requirements

### Build & Integration

- [ ] **BUILD-01**: Clay library fetched via CMake FetchContent (Populate pattern, not MakeAvailable)
- [ ] **BUILD-02**: cels-clay is a CMake INTERFACE library matching cels-ncurses pattern
- [ ] **BUILD-03**: CLAY_IMPLEMENTATION defined in exactly one module .c file
- [ ] **BUILD-04**: Clay include path available to consumers via target_include_directories

### Module Core

- [ ] **CORE-01**: Clay arena initialized and managed through CELS lifecycle system
- [ ] **CORE-02**: CEL_DefineModule(Clay_Engine) facade bundles layout + render systems
- [ ] **CORE-03**: Clay error handler wired to CELS logging (capacity exceeded, duplicate IDs surfaced visibly)
- [ ] **CORE-04**: Terminal dimensions synced to Clay_SetLayoutDimensions each frame from window state

### Developer API

- [ ] **API-01**: CEL_Clay() scope macro opens entity-backed CLAY() scope inside compositions
- [ ] **API-02**: ClayUI component with layout function pointer attached to entities
- [ ] **API-03**: clay_emit_children() helper recursively walks child entities emitting their layout functions
- [ ] **API-04**: Text measurement hook point for renderer backends (Clay_SetMeasureTextFunction bridge)
- [ ] **API-05**: All Clay macros (CLAY, CLAY_TEXT, CLAY_ID, CLAY_IDI, CLAY_AUTO_ID, sizing/padding helpers) work as-is inside CEL_Clay blocks
- [ ] **API-06**: Cell-aware sizing helpers: CLAY_CELL_WIDTH(n), CLAY_CELL_HEIGHT(n), CLAY_CELL_PADDING(l,r,t,b)
- [ ] **API-07**: Dynamic string helper macro for non-literal text from CELS state
- [ ] **API-08**: Auto Clay ID generation from CELS composition context (prevent manual ID boilerplate)

### Rendering Pipeline

- [ ] **PIPE-01**: ClayRenderable Feature defined by cels-clay module
- [ ] **PIPE-02**: Provider callback pattern dispatches Clay_RenderCommandArray to registered backends
- [ ] **PIPE-03**: Layout system runs at PreStore phase (after recomposition, before rendering)
- [ ] **PIPE-04**: Render dispatch runs at OnRender phase (after layout complete)
- [ ] **PIPE-05**: Entity tree walked in parent-before-child depth-first order for correct CLAY() nesting

### ncurses Renderer

- [ ] **REND-01**: Rectangle rendering (filled cells with background color via ncurses attributes)
- [ ] **REND-02**: Text rendering via mvaddnstr (handles Clay's non-null-terminated StringSlice)
- [ ] **REND-03**: Border rendering with Unicode box-drawing characters (single-line, corners, edges)
- [ ] **REND-04**: Scissor/clipping support (scissor rect stack, bounds-check every cell write)
- [ ] **REND-05**: Float-to-cell coordinate mapping using 1:1 cell-unit system (Clay units = terminal cells)
- [ ] **REND-06**: Color mapping RGBA to ncurses color pairs (8/16 color nearest-match)
- [ ] **REND-07**: Wide character text measurement (wcwidth/wcswidth for CJK/emoji correctness)
- [ ] **REND-08**: Scroll container keyboard navigation (Page Up/Down, arrows mapped to Clay_UpdateScrollContainers)

### Demo App

- [ ] **DEMO-01**: Sidebar + content area layout rendered in terminal via Clay
- [ ] **DEMO-02**: Text elements displaying static and dynamic content
- [ ] **DEMO-03**: A button element with visual feedback (highlight on selection)
- [ ] **DEMO-04**: Working end-to-end: CELS state + Clay layout + ncurses rendering in terminal

## v2 Requirements

### Rendering Backends

- **REND2-01**: SDL3 renderer backend module (cels-sdl or similar)
- **REND2-02**: 256-color and true color support for terminal renderer
- **REND2-03**: Damage tracking / dirty rectangle optimization

### Interaction

- **INT-01**: Mouse pointer support (Clay_SetPointerState wired to ncurses mouse events)
- **INT-02**: Clay_Hovered() / Clay_OnHover() interaction model for keyboard focus

### Advanced Features

- **ADV-01**: Floating element support (dropdowns, tooltips, modals with z-index)
- **ADV-02**: Aspect ratio element support
- **ADV-03**: betweenChildren border separator support
- **ADV-04**: Clay debug mode toggle via CELS state

## Out of Scope

| Feature | Reason |
|---------|--------|
| Widget library (Button, TextInput, Slider) | Widgets are app-level compositions, not layout module concerns |
| Re-wrapping CLAY() macro | Clay's DSL is battle-tested; wrapping it creates maintenance burden for zero benefit |
| Animation/tweening system | Application-level concern; CELS state + delta time handles this |
| Image/bitmap rendering | Not applicable in terminal v1; future backend-specific |
| Text editing/input fields | Widget-level concern requiring cursor/selection state |
| Multi-context Clay (multiple trees) | One global tree per frame is correct for v1 |
| True color in v1 | 8/16 color sufficient; design for future expansion |
| Cross-module dependency (cels-clay depending on cels-ncurses) | Sibling architecture constraint; app wires modules together |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| BUILD-01 | Phase 1 | Pending |
| BUILD-02 | Phase 1 | Pending |
| BUILD-03 | Phase 1 | Pending |
| BUILD-04 | Phase 1 | Pending |
| CORE-01 | Phase 1 | Pending |
| CORE-02 | Phase 3 | Pending |
| CORE-03 | Phase 1 | Pending |
| CORE-04 | Phase 2 | Pending |
| API-01 | Phase 2 | Pending |
| API-02 | Phase 2 | Pending |
| API-03 | Phase 2 | Pending |
| API-04 | Phase 2 | Pending |
| API-05 | Phase 2 | Pending |
| API-06 | Phase 2 | Pending |
| API-07 | Phase 2 | Pending |
| API-08 | Phase 2 | Pending |
| PIPE-01 | Phase 3 | Pending |
| PIPE-02 | Phase 3 | Pending |
| PIPE-03 | Phase 2 | Pending |
| PIPE-04 | Phase 3 | Pending |
| PIPE-05 | Phase 2 | Pending |
| REND-01 | Phase 4 | Pending |
| REND-02 | Phase 4 | Pending |
| REND-03 | Phase 4 | Pending |
| REND-04 | Phase 4 | Pending |
| REND-05 | Phase 4 | Pending |
| REND-06 | Phase 4 | Pending |
| REND-07 | Phase 4 | Pending |
| REND-08 | Phase 4 | Pending |
| DEMO-01 | Phase 5 | Pending |
| DEMO-02 | Phase 5 | Pending |
| DEMO-03 | Phase 5 | Pending |
| DEMO-04 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 33 total
- Mapped to phases: 33
- Unmapped: 0

---
*Requirements defined: 2026-02-07*
*Last updated: 2026-02-07 after roadmap creation*
