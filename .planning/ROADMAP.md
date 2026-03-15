# Roadmap: cels-clay

## Milestones

- **v0.5 Foundation** - Phases 1-5 (shipped 2026-02-08)
- **v0.6 Entity-Based UI + Dual Renderers** - Phases 6-11 (in progress)

## Phases

<details>
<summary>v0.5 Foundation (Phases 1-5) - SHIPPED 2026-02-08</summary>

### Phase 1: Build System + Clay Initialization
**Goal**: Clay compiles, initializes, and is ready for layout calls -- the foundation everything else builds on
**Plans**: 2 plans

Plans:
- [x] 01-01-PLAN.md -- CMake FetchContent + INTERFACE library setup
- [x] 01-02-PLAN.md -- Clay initialization, arena management, error handler wiring

### Phase 2: Layout System Core
**Goal**: Developers can write CELS compositions that contribute CLAY() blocks to a single per-frame layout tree with correct nesting and ordering
**Plans**: 3 plans

Plans:
- [x] 02-01-PLAN.md -- Public header, types, frame arena, text measurement, auto-ID infrastructure
- [x] 02-02-PLAN.md -- Layout system (PreStore), entity tree walk, BeginLayout/EndLayout, module wiring
- [x] 02-03-PLAN.md -- ClaySurface composition, compile validation, phase success criteria verification

### Phase 3: Render Bridge + Module Definition
**Goal**: Layout results are exposed to renderer backends through the CELS Feature/Provider pattern, and the module has a clean single-call initialization
**Plans**: 2 plans

Plans:
- [x] 03-01-PLAN.md -- Render bridge: ClayRenderable Feature, data component, dispatch system
- [x] 03-02-PLAN.md -- Module facade: pointer-based config API, composable sub-modules

### Phase 4: ncurses Clay Renderer
**Goal**: Clay render commands produce visible terminal output -- rectangles, text, borders, and colors rendered correctly in ncurses
**Plans**: 2 plans

Plans:
- [x] 04-01-PLAN.md -- Complete renderer: theme, provider callback, coordinate mapping, color mapping, text measurement
- [x] 04-02-PLAN.md -- Scroll container keyboard navigation

### Phase 5: Demo App + Integration
**Goal**: A working terminal application proves CELS state + Clay layout + ncurses rendering work together end-to-end
**Plans**: 2 plans

Plans:
- [ ] 05-01-PLAN.md -- Complete demo app: source files + CMake target
- [ ] 05-02-PLAN.md -- Build validation, bug fixes, end-to-end verification

</details>

## v0.6 Entity-Based UI + Dual Renderers

**Milestone Goal:** Rewrite cels-clay from layout-function model to a pure entity/component UI model with Compose-inspired primitives and dual SDL3/NCurses rendering.

**Phase Numbering:**
- Integer phases (6, 7, 8...): Planned milestone work
- Decimal phases (7.1, 7.2): Urgent insertions (marked with INSERTED)

- [ ] **Phase 6: Module Modernization** - Migrate to CELS v0.4+ patterns, retire Feature/Provider
- [ ] **Phase 7: Entity Primitives** - Row, Column, Box, Text, Spacer, Image compositions with per-entity property components
- [ ] **Phase 8: Layout System Rewrite** - Property-driven tree walk replaces function-pointer dispatch
- [ ] **Phase 9: NCurses Renderer Migration + Renderer Infrastructure** - Port NCurses renderer to module pattern, add renderer selection and conditional compilation
- [ ] **Phase 10: SDL3 Renderer** - New SDL3 renderer translating Clay render commands to SDL3 draw calls
- [ ] **Phase 11: Dual-Renderer Example App** - Minimal app proving the same entity tree renders on both backends

## Phase Details

### Phase 6: Module Modernization
**Goal**: cels-clay uses CELS v0.4+ module patterns throughout, with no deprecated Feature/Provider code remaining
**Depends on**: v0.5 foundation (Phases 1-5)
**Requirements**: MOD-01, MOD-02, MOD-03, MOD-04, MOD-05
**Success Criteria** (what must be TRUE):
  1. Clay_Engine module initializes via CEL_Module facade with CEL_Register (no _CEL_DefineModule)
  2. Clay arena init/cleanup runs through CEL_Lifecycle observers (no atexit calls remain)
  3. ClayEngineState singleton is accessible from any translation unit via cross-TU registry
  4. All Feature/Provider types and patterns (ClayRenderable, _CEL_Provides, ClayRenderableData) are removed from the codebase
  5. ClaySurface composition works with the modernized module (layout still runs, NCurses renderer still produces output)
**Plans**: TBD

### Phase 7: Entity Primitives
**Goal**: Developers can compose UI using Row, Column, Box, Text, Spacer, and Image entities with per-entity tailored property components
**Depends on**: Phase 6
**Requirements**: PRIM-01, PRIM-02, PRIM-03, PRIM-04, PRIM-05, PRIM-06
**Success Criteria** (what must be TRUE):
  1. Each primitive (Row, Column, Box, Text, Spacer, Image) is a CELS composition that spawns an entity with its corresponding property component
  2. RowProps and ColumnProps have fields for gap, padding, sizing, and alignment that map to Clay layout config
  3. BoxProps has fields for borders, corner radius, and background color
  4. TextProps has fields for text content, color, font ID, wrap mode, and letter spacing
  5. SpacerProps and ImageProps have sizing fields, and ImageProps holds an opaque source pointer
**Plans**: TBD

### Phase 8: Layout System Rewrite
**Goal**: The layout system reads entity property components to emit CLAY() calls, replacing the function-pointer dispatch model
**Depends on**: Phase 7
**Requirements**: LAYOUT-01, LAYOUT-02, LAYOUT-03, LAYOUT-04
**Success Criteria** (what must be TRUE):
  1. The tree walk reads RowProps/ColumnProps/BoxProps/TextProps/SpacerProps/ImageProps components to generate CLAY() calls (no layout function pointers used)
  2. Entity type is dispatched by component presence -- an entity with RowProps emits a horizontal CLAY layout, ColumnProps emits vertical, etc.
  3. Entities without any layout component pass their children through to the parent scope transparently (logical grouping without visual impact)
  4. Parent-before-child depth-first walk order from v0.5 is preserved (layout nesting is correct)
**Plans**: TBD

### Phase 9: NCurses Renderer Migration + Renderer Infrastructure
**Goal**: The existing NCurses renderer operates as a standalone CEL_Module with renderer selection via module registration and conditional compilation
**Depends on**: Phase 8
**Requirements**: REND-01, REND-08, REND-09
**Success Criteria** (what must be TRUE):
  1. NCurses renderer is a standalone CEL_Module(Clay_NCurses) with its own text measurement callback registration
  2. Developers select their renderer by calling cels_register(Clay_NCurses) or cels_register(Clay_SDL3) -- no explicit config needed
  3. Renderer source files compile only when their backend target exists (CMake TARGET guards for cels-sdl3 and cels-ncurses)
  4. A program using cels_register(Clay_NCurses) produces the same terminal output as the v0.5 renderer
**Plans**: TBD

### Phase 10: SDL3 Renderer
**Goal**: Clay render commands produce correct visual output through SDL3 -- rectangles, text, borders, images, and clipping
**Depends on**: Phase 9
**Requirements**: REND-02, REND-03, REND-04, REND-05, REND-06, REND-07
**Success Criteria** (what must be TRUE):
  1. CEL_Module(Clay_SDL3) translates the full Clay render command array to SDL3 draw calls each frame
  2. Filled rectangles render with correct position, size, and RGBA color via SDL_RenderFillRect
  3. Text renders via SDL_ttf (TTF_RenderText + SDL_RenderTexture) with correct font, color, and positioning
  4. Borders render as per-side lines, and scissor/clipping restricts drawing via SDL_SetRenderClipRect
  5. Image render commands draw an SDL_Texture from the imageData pointer with correct dimensions
**Plans**: TBD

### Phase 11: Dual-Renderer Example App
**Goal**: A single example app proves the same entity tree renders correctly on both NCurses and SDL3 backends
**Depends on**: Phase 10
**Requirements**: DEMO-01, DEMO-02, DEMO-03, DEMO-04
**Success Criteria** (what must be TRUE):
  1. The example app uses Row, Column, Box, Text, and Spacer entities to build a visible UI layout
  2. Running with cels_register(Clay_NCurses) produces correct terminal output (layout, text, colors)
  3. Running with cels_register(Clay_SDL3) produces correct graphical output (layout, text, colors)
  4. The entity tree definition is identical between both backends -- only the module registration line differs
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 6 -> 7 -> 8 -> 9 -> 10 -> 11

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Build System + Clay Init | 2/2 | Complete | 2026-02-08 |
| 2. Layout System Core | 3/3 | Complete | 2026-02-08 |
| 3. Render Bridge + Module Def | 2/2 | Complete | 2026-02-08 |
| 4. ncurses Clay Renderer | 2/2 | Complete | 2026-02-08 |
| 5. Demo App + Integration | 0/2 | Not started | - |
| 6. Module Modernization | 0/TBD | Not started | - |
| 7. Entity Primitives | 0/TBD | Not started | - |
| 8. Layout System Rewrite | 0/TBD | Not started | - |
| 9. NCurses Migration + Infra | 0/TBD | Not started | - |
| 10. SDL3 Renderer | 0/TBD | Not started | - |
| 11. Dual-Renderer Example App | 0/TBD | Not started | - |

---
*Roadmap created: 2026-02-07*
*Last updated: 2026-03-15 after v0.6 milestone roadmap creation*
