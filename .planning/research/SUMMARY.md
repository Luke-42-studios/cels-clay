# Project Research Summary

**Project:** cels-clay v0.6
**Domain:** Compose-Inspired Entity-Based UI Framework (CELS + Clay + Dual Renderers)
**Researched:** 2026-03-15
**Confidence:** HIGH

## Executive Summary

cels-clay v0.6 transforms the module from a "layout function on entity" model to a "entity IS a UI primitive" model, inspired by Jetpack Compose and Godot's scene tree. Instead of writing `CEL_Clay_Layout(fn)` functions that emit raw `CLAY()` macros, developers compose entities like `Row(.gap = 2) { Text(.text = "Hello") {} }` and the framework translates entity properties to Clay calls automatically. This is a new abstraction layer on the existing stack — no new external dependencies.

Simultaneously, the module gains dual rendering: both SDL3 and NCurses renderers live inside cels-clay with conditional compilation. The developer selects their renderer by registering the appropriate module (`cels_register(Clay_NCurses)` or `cels_register(Clay_SDL3)`). The existing NCurses renderer is migrated to the new module pattern; the SDL3 renderer is new work following the same architecture.

The module is also modernized to CELS v0.4+ patterns: `CEL_Module`, `CEL_Lifecycle`, `CEL_System`, state singletons with cross-TU registry — matching how cels-sdl3 and cels-ncurses are structured.

## Key Findings

### Recommended Stack

No new external dependencies. The entire v0.6 transformation is a new abstraction layer on:
- **CELS v0.4+** — CEL_Module, CEL_Component, CEL_Compose patterns (verified from cels.h)
- **Clay v0.14** — Layout engine, already integrated via FetchContent
- **SDL3** — via cels-sdl3 (context init already handled)
- **NCurses** — via cels-ncurses (full drawing API already available)

### Expected Features

**Table Stakes (12 items):**
1. Row composition + RowProps (horizontal layout, gap, padding, sizing, alignment)
2. Column composition + ColumnProps (vertical layout)
3. Box composition + BoxProps (general container, borders, corner radius)
4. Text composition + TextProps (text content, color, font, wrap)
5. Spacer composition + SpacerProps (invisible space filler)
6. Image composition + ImageProps (SDL3 images, NCurses placeholder)
7. ClaySurface root composition (layout boundary)
8. Property-driven layout system rewrite (entity tree walk reads components, not function pointers)
9. SDL3 renderer (Clay render commands → SDL3 draw calls)
10. NCurses renderer migration (existing code → new module pattern)
11. Renderer selection via module registration
12. Modern CELS module pattern (CEL_Module, CEL_Lifecycle, CEL_System)

**Differentiators:**
- Transparent composition passthrough (logical grouping without visual impact)
- Border decoration component (additive styling)
- Cell-aware sizing helpers (CELL_W, CELL_H macros)

**Anti-features (do NOT build):**
- Modifier chain pattern (Kotlin feature, not C)
- Widget library (app-level concern)
- ListView/ScrollView (future milestone)
- Layout functions as escape hatch (pure entity model for v0.6)
- Animation system (app-level concern)
- Mouse interaction (keyboard-only)

### Architecture Approach

**Data flow:**
```
Entity Properties → Layout Walker → CLAY() calls → Clay_RenderCommandArray → Renderer → Screen
```

**Frame pipeline:**
- PreStore: `ClayLayoutSystem` walks entity tree, reads property components, emits CLAY() calls
- OnRender: Active renderer system consumes render commands, draws to backend

**Module structure:**
- `Clay_Engine` — arena init, lifecycle, core layout system
- `Clay_NCurses` — NCurses render system + text measurement callback
- `Clay_SDL3` — SDL3 render system + text measurement callback

**Per-entity property components** (not shared): RowProps, ColumnProps, BoxProps, TextProps, SpacerProps, ImageProps — each with tailored fields mapping directly to Clay API structs.

**Conditional compilation:** CMake `if(TARGET cels-sdl3)` / `if(TARGET cels-ncurses)` guards control which renderer sources are compiled.

### Critical Pitfalls

1. **P-01 (Entity→CLAY generation):** Layout walker must synthesize correct CLAY() nesting from components. Same algorithm as v0.5, different data source.
2. **P-04 (Text measurement conflict):** NCurses and SDL3 measure text differently (cells vs pixels). Active renderer owns the callback.
3. **P-06 (Renderer mutual exclusion):** Only one renderer module should be registered. Document this.
4. **P-08 (SDL3 renderer handle):** cels-sdl3 may need to expose SDL_Renderer via state singleton.
5. **P-13 (Entity ordering):** Preserve v0.5's explicit tree walk — don't rely on ECS query ordering.

## Implications for Roadmap

**Phase ordering derived from dependency chain:**

1. **Module refactor** — Convert to CELS v0.4+ patterns, clean up dead Feature/Provider code
2. **Entity primitives + property components** — Define RowProps, ColumnProps, BoxProps, TextProps, SpacerProps, compositions
3. **Layout system rewrite** — Property-driven tree walk replaces function-pointer dispatch
4. **ClaySurface adaptation** — Wire to new layout system
5. **NCurses renderer migration** — Port existing renderer to new module pattern
6. **SDL3 renderer** — New implementation using NCurses renderer as template
7. **ImageProps** — Needs SDL3 renderer to be meaningful
8. **Renderer selection + integration** — Module registration wiring
9. **Minimal example** — Dual-backend demo app

Row + Column + Text should be the first three primitives (covers 90% of real UI patterns). SDL3 renderer can be developed after NCurses migration establishes the renderer contract.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | No new dependencies — all verified from existing source |
| Per-entity components | HIGH | Verified from CEL_Component/cel_has macro expansions |
| Composition API | HIGH | Identical pattern proven in cels-sdl3 and cels-ncurses |
| Layout walker extension | HIGH | Same algorithm as v0.5, different data source |
| NCurses renderer | HIGH | 720 lines of production code fully verified |
| SDL3 renderer architecture | HIGH | Follows proven NCurses pattern exactly |
| SDL3 renderer specifics | MEDIUM | SDL_Renderer handle access from cels-sdl3 needs verification |

**Overall confidence:** HIGH — all four research files grounded in direct source code analysis.

## Sources

- cels-clay v0.5 codebase (all headers and source files)
- CELS v0.4 API header (`cels.h`, `cels_macros.h`, `cels_runtime.h`, `cels_system_impl.h`)
- cels-sdl3 module (full source analysis)
- cels-ncurses module (full source analysis)
- Clay layout API (clay.h, renderer references)
- Jetpack Compose design patterns (Row, Column, Box, Spacer, Text)
- Godot Container nodes (HBoxContainer, VBoxContainer, Control)

---
*Research completed: 2026-03-15*
*Ready for roadmap: yes*
