# Project Research Summary

**Project:** cels-clay
**Domain:** ECS + Immediate-Mode Layout Integration (CELS reactive framework + Clay layout engine)
**Researched:** 2026-02-07
**Confidence:** HIGH

## Executive Summary

cels-clay integrates Clay v0.14 (a single-header C99 flexbox layout library) into the CELS declarative ECS framework. The integration is architecturally clean but non-trivial: Clay rebuilds its entire element tree every frame via `BeginLayout`/`EndLayout`, while CELS compositions only re-execute when observed state changes. The solution is a two-system design where CELS reactivity controls *which* compositions are active (updating the entity tree), and a dedicated Clay layout system walks that entity tree every frame emitting `CLAY()` calls between a single `BeginLayout`/`EndLayout` pair. This means compositions have two functions: a composition body (creates ECS entities) and a layout function (emits Clay declarations). The separation is the core architectural insight and must be enforced from day one.

The recommended approach is bottom-up: build the CMake integration and Clay initialization first, then the layout system (entity tree walking + `BeginLayout`/`EndLayout` coordination), then the render bridge (Feature/Provider pattern exposing `Clay_RenderCommandArray`), and finally the ncurses renderer as a separate app-layer concern. The stack is minimal -- Clay via FetchContent, ncurses via existing cels-ncurses, CMake 3.21+. No new dependencies. The 1:1 cell-unit coordinate system (one Clay unit = one terminal cell) eliminates the float-to-cell rounding problems that plague Clay's existing terminal renderers. This is a departure from Clay's examples but is the correct approach for terminal-native UI.

The critical risks are: (1) Clay's `CLAY()` macro uses a shared static latch variable that can collide with wrapper macros -- `CEL_Clay()` must use its own `__COUNTER__`-based loop variable, never the latch; (2) ECS iteration order is non-deterministic, but Clay tree structure depends on declaration order -- the layout system must impose explicit ordering; (3) all `CLAY()` calls must occur within a single `BeginLayout`/`EndLayout` window in one system, never scattered across ECS phases. All three are architectural decisions that must be correct in the first phase.

## Key Findings

### Recommended Stack

The stack is deliberately minimal. Clay is the only new dependency, fetched via CMake FetchContent. Everything else already exists in the CELS ecosystem.

**Core technologies:**
- **Clay v0.14**: Flexbox-style layout engine -- single-header, C99, zero dependencies, arena-based, renderer-agnostic. Pin to v0.14 tag for stability.
- **ncurses (wide)**: Terminal rendering -- already available via cels-ncurses module. No new terminal library needed.
- **CMake FetchContent**: Dependency management -- same pattern as flecs/yyjson in CELS root. Must use `FetchContent_Populate` (not `MakeAvailable`) because Clay's CMakeLists.txt builds examples that would pull unwanted deps.

**Build decision:** cels-clay is a CMake INTERFACE library (same as cels-ncurses). `CLAY_IMPLEMENTATION` is defined in exactly one `.c` file (`clay_engine.c`). Clay's include path is provided via `target_include_directories`.

**Coordinate system decision:** Use 1:1 cell units (one Clay unit = one terminal cell). This eliminates the pixel-ratio scaling used by Clay's terminal examples and removes all rounding complexity. Clay's layout engine is unit-agnostic -- it does not assume pixels.

### Expected Features

**Must have (table stakes -- 10 items, all required for a functioning module):**
- Clay initialization + arena ownership (module manages Clay lifecycle)
- `CEL_Clay()` scope macro (bridge between CELS compositions and Clay trees)
- `ClayRenderable` Feature + Provider contract (rendering pipeline)
- Float-to-cell coordinate mapping (Clay floats to terminal integers)
- Text measurement callback (terminal-correct, handles UTF-8 wide chars)
- RGBA-to-terminal color mapping (Clay colors to ncurses color pairs)
- Layout dimension sync from window state (resize handling)
- `CLAY()` macro passthrough (no re-wrapping -- use Clay DSL directly)
- Module definition via `_CEL_DefineModule` (consistent CELS pattern)
- CMake FetchContent for Clay (automated dependency)

**Should have (differentiators -- build after table stakes):**
- Unicode box-drawing border renderer (significant visual improvement)
- Error handler integration (Clay errors surfaced through CELS logging)
- Cell-aware sizing helpers (`CLAY_CELL_WIDTH(n)` macros)
- Floating element support (dropdowns, overlays -- rendering works for free)
- `betweenChildren` border support (dividers between list items)

**Defer (v2+):**
- Scroll container keyboard navigation (requires focus tracking system)
- Auto Clay ID generation from CELS context (CLAY_AUTO_ID exists as workaround)
- 256-color and true color support (8-color LUT is sufficient for v1)
- Mouse/pointer interaction (keyboard-only for terminal v1)
- Widget library (app-level compositions, not layout concerns)
- Animation/tweening (application-level state changes)
- Image/bitmap rendering (out of scope for terminal)

### Architecture Approach

The architecture is a two-system design within the CELS ECS pipeline. `Clay_LayoutSystem` runs at PreStore (after state is settled, before rendering) and coordinates a single `BeginLayout`/`EndLayout` pass by walking the entity tree depth-first. `Clay_RenderSystem` runs at OnRender and dispatches the resulting `Clay_RenderCommandArray` to whatever renderer backend is registered via the Feature/Provider pattern. Compositions pair an ECS body (creates entities with `ClayUI` component) with a layout function (emits `CLAY()` calls). The entity tree IS the UI tree -- flecs parent-child relationships define nesting.

**Major components:**
1. **Clay_Engine** (module facade) -- arena lifecycle, module bundling, window resize observation
2. **Clay_Layout** (layout system) -- `BeginLayout`/`EndLayout` coordination, entity tree walking, `ClayUI` component
3. **Clay_Render** (render provider) -- `ClayRenderable` Feature definition, render command dispatch, text measurement hook
4. **Clay_TextMeasure** (text callback) -- pluggable measurement function, ships default cell-based implementation
5. **Clay_Arena** (arena manager) -- one-time allocation, persists for application lifetime

**File structure:** 3 headers (`clay_engine.h`, `clay_layout.h`, `clay_render.h`) + 3 sources (`clay_engine.c`, `clay_layout.c`, `clay_render.c`).

### Critical Pitfalls

The top 5 pitfalls that must be addressed during implementation:

1. **CLAY() macro latch collision (P-01)** -- Clay's `CLAY()` macro uses a file-scope `static uint8_t` latch. If `CEL_Clay()` also expands to a for-loop, it must use its own `__COUNTER__`-based variable, never touch the latch. Test with 3-level nesting.

2. **Non-deterministic ECS iteration order (P-02)** -- Flecs does not guarantee entity iteration order, but Clay tree structure depends on declaration order. The layout system must walk entities in explicit order (parent-child hierarchy with creation-order sorting), not rely on arbitrary query ordering.

3. **BeginLayout/EndLayout bracketing (P-03)** -- ALL `CLAY()` calls must occur within a single `BeginLayout`/`EndLayout` window, in one system, at one pipeline phase. Compositions register layout callbacks; the layout system invokes them sequentially. Never scatter Clay calls across ECS phases.

4. **Render command array is ephemeral (P-10)** -- `Clay_RenderCommandArray` points into Clay's arena, which is reset on next `BeginLayout`. The renderer must consume all commands within the same frame. Never store pointers across frames.

5. **CLAY_IMPLEMENTATION ODR violation (P-05)** -- Must be defined in exactly one translation unit. In the INTERFACE library pattern, `clay_engine.c` owns this. Document that consumers must NOT define it themselves.

## Implications for Roadmap

Based on combined research, the dependency chain is clear: build system -> Clay init -> layout system -> render bridge -> ncurses renderer -> demo app. The critical path from FEATURES.md confirms this: `10 -> 1 -> 5,7 -> 2 -> 4,8 -> 6 -> 3 -> 9`.

### Phase 1: Build System + Clay Foundation
**Rationale:** Nothing works until Clay compiles and initializes. This is pure infrastructure with zero design ambiguity.
**Delivers:** CMakeLists.txt with FetchContent, INTERFACE library target, `CLAY_IMPLEMENTATION` in one TU, Clay arena allocation, `Clay_Initialize`, error handler wired to stderr.
**Addresses:** Table stakes 10 (FetchContent), 1 (Clay init + arena)
**Avoids:** P-05 (CLAY_IMPLEMENTATION ODR), P-11 (no Clay INTERFACE target), P-08 (arena capacity -- set generous defaults)

### Phase 2: Layout System Core
**Rationale:** The layout system is the architectural spine. It resolves the three most critical pitfalls (macro latch, tree ordering, BeginLayout bracketing) and must be designed correctly before any rendering exists.
**Delivers:** `ClayUI` component definition, `CEL_Clay()` scope macro, `clay_emit_children()` helper, `Clay_LayoutSystem` at PreStore phase, entity tree walking in deterministic order, text measurement callback registration, layout dimension sync.
**Addresses:** Table stakes 2 (CEL_Clay macro), 5 (text measurement), 7 (layout dim sync), 8 (CLAY passthrough)
**Avoids:** P-01 (latch collision), P-02 (tree ordering), P-03 (BeginLayout bracketing), P-04 (Clay ID strategy), P-14 (single-threaded constraint)

### Phase 3: Render Bridge + Module Definition
**Rationale:** With layout producing a `Clay_RenderCommandArray`, the render bridge exposes it to backends via the Feature/Provider pattern. Module definition bundles everything into a clean init story.
**Delivers:** `ClayRenderable` Feature definition, render command accessor API, Provider registration framework, `CEL_DefineModule(Clay_Engine)` with `Clay_Engine_use()` entry point.
**Addresses:** Table stakes 3 (ClayRenderable Feature), 9 (module definition)
**Avoids:** P-09 (phase ordering -- layout at PreStore, render at OnRender), P-10 (ephemeral pointers -- document "consume immediately" contract), P-16 (sibling module boundary -- renderer is NOT in cels-clay)

### Phase 4: ncurses Clay Renderer
**Rationale:** The renderer translates Clay render commands to terminal output. It lives in the app layer (or a bridge file), NOT inside cels-clay. This phase requires understanding the termbox2 renderer patterns.
**Delivers:** Rectangle rendering (colored spaces), text rendering (`mvaddnstr`), border rendering (box-drawing chars), scissor/clipping, 8-color nearest-match LUT, float-to-cell coordinate conversion.
**Addresses:** Table stakes 4 (coordinate mapping), 6 (color mapping), differentiators 4 (Unicode borders), 6 (error handler)
**Avoids:** P-06 (quantization -- use `round(x+w) - round(x)` pattern), P-07 (wide char measurement), P-13 (border rounding -- min 1 cell), P-15 (scissor implementation), P-12 (dynamic text strings)

### Phase 5: Demo App + Integration Validation
**Rationale:** End-to-end proof that CELS + Clay + ncurses work together. Validates all architectural decisions.
**Delivers:** Demo app with sidebar + content area + text + button. CELS compositions with Clay layout. State changes trigger recomposition and layout updates. Terminal resize handling.
**Addresses:** Validates all table stakes. Tests differentiators 5 (betweenChildren), 7 (floating elements).
**Avoids:** All pitfalls validated through integration testing.

### Phase Ordering Rationale

- **Phase 1 before 2:** Clay must compile and initialize before any macros or layout systems can be tested.
- **Phase 2 before 3:** The layout system produces the `Clay_RenderCommandArray` that the render bridge exposes. Cannot define the render contract without understanding what layout produces.
- **Phase 3 before 4:** The renderer is a Provider for `ClayRenderable`. The Feature must exist before Providers can register.
- **Phase 4 before 5:** The demo app needs visible output. Without the ncurses renderer, there is nothing to see.
- **Phases 2-3 are the architectural core.** If the layout system and render bridge are designed correctly, phases 4-5 are mechanical implementation.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 2 (Layout System Core):** The composition-to-layout-function pairing is novel. No existing reference implementation. The entity tree walking algorithm, `CEL_Clay()` macro design, and Clay ID strategy all need careful plan-level design. Recommend `/gsd:research-phase`.
- **Phase 4 (ncurses Renderer):** The termbox2 renderer is a strong reference, but ncurses-specific concerns (color pair management, `wnoutrefresh`/`doupdate` pattern, wide character handling with `ncursesw`) need validation. Recommend `/gsd:research-phase`.

Phases with standard patterns (skip research-phase):
- **Phase 1 (Build System):** Well-documented FetchContent + INTERFACE library pattern. Verified from cels-ncurses reference. Standard CMake.
- **Phase 3 (Render Bridge):** Direct application of CELS Feature/Provider pattern already used by cels-ncurses. Module definition follows `CEL_DefineModule` convention.
- **Phase 5 (Demo App):** Integration phase. No new patterns -- just wiring existing components together.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All technologies verified from local source checkouts. Clay v0.14 header, ncurses via cels-ncurses, CMake patterns from CELS root. No external docs needed. |
| Features | HIGH | Table stakes derived from Clay API analysis + CELS module patterns. Dependency chain verified. Anti-features grounded in PROJECT.md scope constraints. |
| Architecture | HIGH | Two-system design (layout + render) derived from Clay's `BeginLayout`/`EndLayout` contract and CELS pipeline phases. Entity tree walking follows flecs parent-child semantics. All verified from source. |
| Pitfalls | HIGH | All 16 pitfalls verified by direct Clay source code inspection with specific line numbers. Prevention strategies reference working implementations (termbox2 renderer). |

**Overall confidence:** HIGH

All four research files are grounded in direct source code analysis of Clay v0.14 (`clay.h`), CELS v0.1 (`cels.h`, `cels.cpp`), cels-ncurses (full module), and Clay's terminal renderers (ANSI + termbox2). No findings rely on external documentation, community posts, or inference.

### Gaps to Address

- **Composition-to-layout-function pairing DX:** The two-function pattern (composition body + layout function) is architecturally sound but may feel verbose. Need to validate during Phase 2 whether a single-macro approach is feasible or whether the separation is the right trade-off.
- **Layout function access to CELS state:** Layout functions need to read component data to branch on state. The mechanism for accessing component data inside a layout callback (via entity ID queries? via props struct?) needs concrete API design.
- **cels-ncurses drawing primitives gap:** PROJECT.md notes that cels-ncurses's planned graphics API (drawing primitives, layers) is "not yet built." Phase 4 may need to build ncurses drawing helpers or operate directly on stdscr. Validate what cels-ncurses currently provides.
- **Clay v0.14 vs v0.14+52 local checkout:** Research used the v0.14 tag. The local checkout is 52 commits ahead. If any post-v0.14 API changes affect the integration, they will surface during Phase 1. Pin to v0.14 tag.

## Sources

### Primary (HIGH confidence -- direct source analysis)
- Clay v0.14 header: `/home/cachy/workspaces/libs/clay/clay.h`
- Clay termbox2 renderer: `/home/cachy/workspaces/libs/clay/renderers/termbox2/clay_renderer_termbox2.c`
- Clay ANSI terminal renderer: `/home/cachy/workspaces/libs/clay/renderers/terminal/clay_renderer_terminal_ansi.c`
- Clay terminal example: `/home/cachy/workspaces/libs/clay/examples/terminal-example/main.c`
- Clay termbox2 demo: `/home/cachy/workspaces/libs/clay/examples/termbox2-demo/main.c`
- Clay CMakeLists.txt: `/home/cachy/workspaces/libs/clay/CMakeLists.txt`
- CELS API header: `/home/cachy/workspaces/libs/cels/include/cels/cels.h`
- CELS runtime: `/home/cachy/workspaces/libs/cels/src/cels.cpp`
- cels-ncurses full module: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/`
- CELS demo app: `/home/cachy/workspaces/libs/cels/examples/app.c`

### Secondary (HIGH confidence -- project documentation)
- cels-clay PROJECT.md: `/home/cachy/workspaces/libs/cels/modules/cels-clay/.planning/PROJECT.md`
- cels-ncurses architecture docs: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/.planning/codebase/`

---
*Research completed: 2026-02-07*
*Ready for roadmap: yes*
