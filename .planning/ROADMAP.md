# Roadmap: cels-clay

## Overview

cels-clay integrates the Clay layout engine into the CELS declarative ECS framework, delivering a module where developers write app structure with CELS (state, reactivity, lifecycle) and use Clay's CLAY() macro DSL for UI subtrees. The roadmap follows the natural dependency chain: build system, then Clay initialization, then the layout system that walks entity trees and coordinates Clay's BeginLayout/EndLayout, then the render bridge that exposes results via Feature/Provider, then the ncurses renderer that translates render commands to terminal output, and finally a demo app proving end-to-end integration.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Build System + Clay Initialization** - CMake integration, Clay fetch, arena setup, error handler
- [x] **Phase 2: Layout System Core** - CEL_Clay() macro, entity tree walking, layout pipeline, developer API
- [x] **Phase 3: Render Bridge + Module Definition** - ClayRenderable Feature, Provider dispatch, module facade
- [x] **Phase 4: ncurses Clay Renderer** - Terminal render commands: rectangles, text, borders, clipping, color
- [ ] **Phase 5: Demo App + Integration** - Sidebar + content layout, end-to-end CELS + Clay + ncurses

## Phase Details

### Phase 1: Build System + Clay Initialization
**Goal**: Clay compiles, initializes, and is ready for layout calls -- the foundation everything else builds on
**Depends on**: Nothing (first phase)
**Requirements**: BUILD-01, BUILD-02, BUILD-03, BUILD-04, CORE-01, CORE-03
**Success Criteria** (what must be TRUE):
  1. A consumer project can `find_package` or `add_subdirectory` cels-clay and get Clay headers available automatically
  2. The project compiles with Clay included in exactly one translation unit (no duplicate symbol errors)
  3. Clay arena is allocated and Clay_Initialize succeeds during module startup
  4. Clay errors (capacity exceeded, duplicate IDs) print visible messages to stderr
**Plans**: 2 plans

Plans:
- [x] 01-01-PLAN.md -- CMake FetchContent + INTERFACE library setup (wave 1)
- [x] 01-02-PLAN.md -- Clay initialization, arena management, error handler wiring (wave 2)

### Phase 2: Layout System Core
**Goal**: Developers can write CELS compositions that contribute CLAY() blocks to a single per-frame layout tree with correct nesting and ordering
**Depends on**: Phase 1
**Requirements**: CORE-04, API-01, API-02, API-03, API-04, API-05, API-07, API-08, PIPE-03, PIPE-05
**Success Criteria** (what must be TRUE):
  1. A CEL_Clay() scope inside a composition opens an entity-backed CLAY() block and all standard Clay macros (CLAY_TEXT, CLAY_ID, sizing helpers) work inside it
  2. Parent-child entity relationships produce correctly nested CLAY() output in deterministic depth-first order
  3. The layout system runs at PreStore phase each frame, coordinating a single BeginLayout/EndLayout pass across all active compositions
  4. Terminal resize updates propagate to Clay_SetLayoutDimensions so layout adapts to window size changes
  5. Text measurement callback is registered and available for renderer backends to hook into
**Plans**: 3 plans

Plans:
- [x] 02-01-PLAN.md -- Public header, types, frame arena, text measurement, auto-ID infrastructure (wave 1)
- [x] 02-02-PLAN.md -- Layout system (PreStore), entity tree walk, BeginLayout/EndLayout, module wiring (wave 2)
- [x] 02-03-PLAN.md -- ClaySurface composition, compile validation, phase success criteria verification (wave 3)

### Phase 3: Render Bridge + Module Definition
**Goal**: Layout results are exposed to renderer backends through the CELS Feature/Provider pattern, and the module has a clean single-call initialization
**Depends on**: Phase 2
**Requirements**: CORE-02, PIPE-01, PIPE-02, PIPE-04
**Success Criteria** (what must be TRUE):
  1. ClayRenderable Feature is defined and a Provider callback receives the Clay_RenderCommandArray each frame
  2. Render dispatch runs at OnStore phase (after layout is complete) and backends can iterate all render commands
  3. CEL_DefineModule(Clay_Engine) bundles layout + render systems and Clay_Engine_use() initializes the full module in one call
**Plans**: 2 plans

Plans:
- [x] 03-01-PLAN.md -- Render bridge: ClayRenderable Feature, ClayRenderableData component, singleton entity, dispatch system at OnStore (wave 1)
- [x] 03-02-PLAN.md -- Module facade: pointer-based config API, composable sub-modules, CMake integration (wave 2)

### Phase 4: ncurses Clay Renderer
**Goal**: Clay render commands produce visible terminal output -- rectangles, text, borders, and colors rendered correctly in ncurses
**Depends on**: Phase 3
**Requirements**: REND-01, REND-02, REND-03, REND-04, REND-05, REND-06, REND-07, REND-08
**Success Criteria** (what must be TRUE):
  1. Clay rectangle commands render as filled cells with correct background color in the terminal
  2. Clay text commands render via mvaddnstr with correct positioning, handling non-null-terminated StringSlice
  3. Borders render with Unicode box-drawing characters (single-line with corners and edges)
  4. Scissor/clipping restricts rendering to the scissor rect (content outside bounds is not drawn)
  5. RGBA colors from Clay map to ncurses 8/16 color pairs via nearest-match lookup
**Plans**: 2 plans

Plans:
- [x] 04-01-PLAN.md -- Complete renderer: theme struct, provider callback (rectangles, text, borders, scissor), coordinate mapping with aspect ratio, color mapping, text measurement callback, CMake wiring (wave 1)
- [x] 04-02-PLAN.md -- Scroll container keyboard navigation: Vim-style j/k/Ctrl-U/Ctrl-D/G/gg bindings, Page Up/Down, arrow keys (wave 2)

### Phase 5: Demo App + Integration
**Goal**: A working terminal application proves CELS state + Clay layout + ncurses rendering work together end-to-end
**Depends on**: Phase 4
**Requirements**: DEMO-01, DEMO-02, DEMO-03, DEMO-04
**Success Criteria** (what must be TRUE):
  1. A sidebar and content area are visible in the terminal, laid out by Clay with correct proportions
  2. Static and dynamic text content renders correctly, with dynamic text updating when CELS state changes
  3. A button element shows visual feedback (highlight) when selected via keyboard navigation
  4. The full pipeline works: CELS compositions define structure, Clay computes layout, ncurses renders output, and state changes trigger visible UI updates
**Plans**: TBD

Plans:
- [ ] 05-01: Demo app composition structure (sidebar + content + button)
- [ ] 05-02: End-to-end wiring and integration validation

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Build System + Clay Initialization | 2/2 | Complete | 2026-02-08 |
| 2. Layout System Core | 3/3 | Complete | 2026-02-08 |
| 3. Render Bridge + Module Definition | 2/2 | Complete | 2026-02-08 |
| 4. ncurses Clay Renderer | 2/2 | Complete | 2026-02-08 |
| 5. Demo App + Integration | 0/2 | Not started | - |

---
*Roadmap created: 2026-02-07*
*Last updated: 2026-02-08 after Phase 4 execution*
