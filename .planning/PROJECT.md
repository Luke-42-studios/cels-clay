# cels-clay

## What This Is

A CELS module that integrates the Clay UI layout library into the CELS declarative ECS framework. Developers write app structure with CELS (state, reactivity, lifecycle) and drop into Clay's `CLAY()` macro DSL for UI subtrees. cels-clay owns Clay initialization, runs layout each frame, and exposes render commands via the CELS Feature/Provider pattern for backend renderers to consume.

## Core Value

Declarative UI development where CELS handles application state and reactivity while Clay handles layout — developers get the best of both without hand-rolling layout math or managing UI state manually.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] CEL_Clay() macro that opens a Clay scope inside a CELS composition
- [ ] Entity-backed Clay blocks (ECS tracks which compositions have Clay UI)
- [ ] One global Clay tree per frame (all active compositions contribute)
- [ ] Full rebuild each frame (immediate-mode — CELS reactivity controls which compositions run, Clay re-layouts what's declared)
- [ ] Clay initialization and arena management owned by cels-clay module
- [ ] ClayRenderable Feature defined by cels-clay
- [ ] Provider callback pattern for renderer backends (receives Clay_RenderCommandArray)
- [ ] Clay fetched via CMake FetchContent (no manual dependency management)
- [ ] cels-ncurses drawing primitives (rect, text, border, color, scissor) — prerequisite for demo
- [ ] ncurses Clay renderer that translates Clay render commands to cels-ncurses primitives
- [ ] Demo app: sidebar + content area with text and a button, rendered in terminal via Clay layout
- [ ] No cross-module dependencies (cels-clay and cels-ncurses are siblings, app wires them)

### Out of Scope

- SDL renderer — future module, not v1
- Widget library — Clay provides layout, widgets are app-level compositions
- Animation/tweening — application-level concern
- Mouse input — ncurses v1 is keyboard-only
- True color support — design for future, 8/256 color for now
- Image/bitmap rendering — not applicable in terminal
- Text editing/input fields — widget-level, not layout-level
- Clay configuration GUI — this is a code-first API

## Context

- CELS v0.1 is complete (main branch) with full DSL: CEL_Component, CEL_Composition, CEL_Init, CEL_State, CEL_Observe, etc.
- cels-ncurses exists as a module with window lifecycle, input provider, and basic renderer — but its planned graphics API (drawing primitives, layers, frame pipeline) from research docs is not yet built
- Clay is a single-header C library (~4000 lines) with flexbox-style layout, zero deps, arena-based memory, and renderer-agnostic render command output
- Clay already has an SDL3 renderer reference implementation we can study for the ncurses renderer
- The hybrid API model was chosen: CELS for app structure + reactivity, CLAY() for UI subtrees
- Dependency tree: app → {cels, cels-clay, cels-ncurses} where cels-clay and cels-ncurses are sibling modules with no cross-dependencies

## Constraints

- **Language**: C99 public API, C++17 internal implementation (matches CELS convention)
- **Build**: CMake INTERFACE library pattern (same as cels-ncurses)
- **Clay version**: Latest stable from GitHub via FetchContent
- **Terminal**: ncurses backend for v1 demo — float-to-cell coordinate mapping required
- **CELS API**: Must use existing v0.1 macros and patterns (CEL_DefineModule, CEL_Feature, CEL_Provides, etc.)
- **No cross-module deps**: cels-clay must not depend on cels-ncurses and vice versa

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Hybrid API (CELS + CLAY() macros) | Leverages Clay's battle-tested layout DSL directly while CELS handles state/reactivity/lifecycle — avoids re-inventing Clay's API | — Pending |
| One global Clay tree per frame | All compositions contribute to single layout tree — Clay sees whole UI for correct layout math | — Pending |
| Full rebuild each frame | Clay is immediate-mode by design. CELS reactivity controls which compositions are active, not incremental layout updates | — Pending |
| Entity-backed Clay blocks | ECS tracks which compositions have Clay UI — enables query-based rendering order and system integration | — Pending |
| Provider callback for rendering | Follows existing CELS Feature/Provider pattern (same as cels-ncurses). Clean contract between layout and rendering | — Pending |
| Sibling module architecture | cels-clay and cels-ncurses have no cross-deps. App wires them together. Keeps dependency tree clean | — Pending |
| Build ncurses primitives first | Correct layering: Clay renderer uses cels-ncurses abstractions, not raw ncurses calls | — Pending |

---
*Last updated: 2026-02-07 after initialization*
