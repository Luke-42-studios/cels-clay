# cels-clay

## What This Is

A CELS module that provides a declarative, entity-based UI framework powered by Clay layout. Developers build UI by composing entities — Row, Column, Box, Text — with per-entity properties, following patterns inspired by Jetpack Compose and Godot's scene tree. cels-clay owns Clay initialization, runs layout each frame, and ships both SDL3 and NCurses renderers that developers select via module registration.

## Core Value

Declarative UI development where developers build interfaces by composing entities with layout properties — CELS handles state and reactivity, Clay handles layout math, and the renderer is a pluggable detail selected at registration time.

## Current Milestone: v0.6 Entity-Based UI + Dual Renderers

**Goal:** Rewrite cels-clay from layout-function model to a pure entity/component UI model with Compose-inspired primitives and dual SDL3/NCurses rendering.

**Target features:**
- Compose-inspired entity primitives: Row, Column, Box, Spacer, Text, Image
- Per-entity tailored property components (RowProps, ColumnProps, etc.)
- Modern CELS patterns: CEL_Module, CEL_Lifecycle, CEL_System, state singletons
- SDL3 renderer (inside cels-clay, compiled when cels-sdl3 available)
- NCurses renderer (inside cels-clay, compiled when cels-ncurses available)
- Renderer selection via module registration order
- Minimal example app that renders on both backends

## Requirements

### Validated

- [x] Clay library fetched via CMake FetchContent
- [x] CMake INTERFACE library pattern
- [x] Clay arena initialization and lifecycle management
- [x] Clay error handler wired to logging
- [x] Entity tree walked depth-first for correct Clay nesting
- [x] One global Clay tree per frame (immediate-mode rebuild)
- [x] Float-to-cell coordinate mapping for terminal rendering
- [x] RGBA-to-ncurses color mapping
- [x] Unicode box-drawing border rendering
- [x] Scissor/clipping support
- [x] Text measurement callback system

### Active

- [ ] Entity-based UI primitives: Row, Column, Box, Spacer, Text, Image
- [ ] Per-entity property components (RowProps, ColumnProps, BoxProps, TextProps, etc.)
- [ ] ClaySurface as root layout boundary composition
- [ ] Modern CELS module pattern (CEL_Module, CEL_Lifecycle, CEL_System)
- [ ] State singletons with cross-TU registry
- [ ] SDL3 Clay renderer (compiled when cels-sdl3 target exists)
- [ ] NCurses Clay renderer (compiled when cels-ncurses target exists)
- [ ] Renderer selection via module registration
- [ ] Minimal dual-renderer example app

### Out of Scope

- Layout functions / function pointers — pure entity model for v0.6
- Widget library (Button, TextInput, Slider) — app-level compositions
- Animation/tweening — application-level concern
- Mouse input — keyboard-only for now
- Text editing/input fields — widget-level concern
- ListView / ScrollView — deferred to future milestone (own phase)
- Clay configuration GUI — code-first API

## Context

- CELS v0.4+ is the target API (CEL_Module, CEL_Register, observer-driven lifecycles, state singletons with cross-TU registry)
- cels-sdl3 v0.1 exists with SDL3 context initialization (SDL_Init, TTF_Init) — follows the CELS module pattern
- cels-ncurses v0.6 exists with full terminal rendering: window lifecycle, input, drawing primitives, surfaces, sub-cell rendering
- Both cels-sdl3 and cels-ncurses follow the same module architecture: CEL_Module facade, observer-driven lifecycle, state singletons
- v0.5 cels-clay built the foundation (Clay integration, layout system, ncurses renderer) but uses outdated CELS patterns (Feature/Provider, layout function pointers)
- Clay is a single-header C library with flexbox-style layout, arena-based memory, renderer-agnostic render command output
- The entity model draws from Jetpack Compose (Row, Column, Box as composables with layout properties) and Godot (Surface → CanvasItem hierarchy)

## Constraints

- **Language**: C99 public API, C++17 internal implementation (matches CELS convention)
- **Build**: CMake INTERFACE library pattern
- **Clay version**: Latest stable from GitHub via FetchContent
- **CELS API**: Must use v0.4+ patterns (CEL_Module, CEL_Register, CEL_Lifecycle, CEL_System)
- **Renderers**: Both live inside cels-clay, conditionally compiled based on available backend targets
- **Dependencies**: cels-sdl3 and cels-ncurses are optional dependencies (renderers compile only when their targets exist)

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| One global Clay tree per frame | All compositions contribute to single layout tree — Clay sees whole UI for correct layout math | ✓ Good |
| Full rebuild each frame | Clay is immediate-mode by design. CELS reactivity controls which compositions are active | ✓ Good |
| Entity-backed Clay layout | Entity tree IS the UI tree — parent-child relationships define nesting | ✓ Good |
| Pure entity model (no layout fns) | Compose-inspired: Row, Column, Box as entities with properties. Simpler DX than function pointers | — Pending |
| Per-entity property components | Each entity type has tailored props (RowProps, ColumnProps) vs shared ClayLayout component | — Pending |
| Renderers inside cels-clay | Both SDL3 and NCurses renderers ship with cels-clay, conditionally compiled | — Pending |
| Renderer via module registration | Developer registers Clay_NCurses or Clay_SDL3 module. No explicit config needed | — Pending |
| Retire Feature/Provider pattern | Replace with modern CELS module pattern (CEL_Module, CEL_System, observers) | — Pending |

---
*Last updated: 2026-03-15 after v0.6 milestone initialization*
