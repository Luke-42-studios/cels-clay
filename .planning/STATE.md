# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-15)

**Core value:** Declarative UI development where developers build interfaces by composing entities with layout properties -- CELS handles state and reactivity, Clay handles layout math, and the renderer is a pluggable detail.
**Current focus:** v0.6 milestone -- Phase 8 (Layout System Rewrite)

## Current Position

Phase: 8 of 11 (Layout System Rewrite)
Plan: 1 of 1 in current phase
Status: Phase complete
Last activity: 2026-03-16 -- Completed 08-01-PLAN.md

Progress: [#####.....] 50% (v0.6 phases 6-11, 3/6 phases complete)

## Performance Metrics

**Velocity:**
- Total plans completed: 13 (v0.5 phases 1-4 + v0.6 phases 6-8)
- Average duration: 6.6 min
- Total execution time: 86 min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 2 | 4 min | 2 min |
| 2 | 3 | 9 min | 3 min |
| 3 | 2 | 5 min | 2.5 min |
| 4 | 2 | 5 min | 2.5 min |
| 5 | 1 | 12 min | 12 min |
| 6 | 1 | 11 min | 11 min |
| 7 | 1 | 27 min | 27 min |
| 8 | 1 | 13 min | 13 min |

**Recent Trend:**
- Last 5 plans: 05-01 (12 min), 06-01 (11 min), 07-01 (27 min), 08-01 (13 min)
- Trend: 08-01 faster than 07-01; straightforward rewrite with Clay API already understood from Phase 7

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [v0.6 Roadmap]: 6-phase structure (phases 6-11) following module refactor -> primitives -> layout rewrite -> NCurses migration -> SDL3 renderer -> demo app
- [v0.6 Roadmap]: Entity primitives defined before layout rewrite so tree walk has components to read
- [v0.6 Roadmap]: NCurses renderer migration before SDL3 renderer -- establishes renderer contract that SDL3 follows
- [v0.6 Roadmap]: Renderer selection (REND-08) and conditional compilation (REND-09) grouped with NCurses migration (infrastructure phase)
- [06-01]: Arena init in CEL_Module body (not lifecycle observer) -- simpler for single-instance Clay
- [06-01]: Clay_Engine_configure() replaces Clay_Engine_use() -- config before cels_register(Clay_Engine)
- [06-01]: CEL_Lifecycle on_destroy for cleanup replaces atexit()
- [06-01]: Retired clay_layout_use() and clay_render_use() sub-module API
- [07-01]: Used Clay_BorderElementConfig (not Clay_Border) -- Clay v0.14 actual type
- [07-01]: Static-linkage composition naming: Name_props, Name_impl, Name_factory (matches _cel_compose_impl macro)
- [07-01]: Fixed ClaySurface naming from ClaySurfaceProps/ClaySurface_Impl to ClaySurface_props/ClaySurface_impl
- [08-01]: Fixed auto-ID discriminators (0-3) per emit function -- entity ID seed provides uniqueness
- [08-01]: Clay_ClipElementConfig uses .horizontal/.vertical bools (not enum) -- Clay v0.14 API
- [08-01]: Clay_ImageElementConfig has only .imageData -- no sourceDimensions in Clay v0.14
- [08-01]: Zero-initialized Clay_SizingAxis treated as GROW(0) via _sizing_or_grow() helper

### Pending Todos

None.

### Blockers/Concerns

- v0.5 Phase 5 (Demo App) is incomplete (plans 05-01, 05-02 not finished) -- v0.6 work will supersede this
- SDL3 renderer handle access from cels-sdl3 needs verification (MEDIUM confidence from research)
- Text measurement conflict between NCurses (cells) and SDL3 (pixels) -- active renderer must own the callback

## Session Continuity

Last session: 2026-03-16
Stopped at: Completed 08-01-PLAN.md (Layout System Rewrite). Phase 8 complete.
Resume file: None
