# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-15)

**Core value:** Declarative UI development where developers build interfaces by composing entities with layout properties -- CELS handles state and reactivity, Clay handles layout math, and the renderer is a pluggable detail.
**Current focus:** v0.6 milestone complete -- all phases (6-11) delivered

## Current Position

Phase: 11 of 11 (Dual-Renderer Example)
Plan: 1 of 1 in current phase
Status: v0.6 milestone complete
Last activity: 2026-03-15 -- Completed 11-01-PLAN.md

Progress: [##########] 100% (v0.6 phases 6-11, 6/6 phases complete)

## Performance Metrics

**Velocity:**
- Total plans completed: 16 (v0.5 phases 1-4 + v0.6 phases 6-11)
- Average duration: 6.7 min
- Total execution time: 107 min

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
| 9 | 1 | 8 min | 8 min |
| 10 | 1 | 4 min | 4 min |
| 11 | 1 | 9 min | 9 min |

**Recent Trend:**
- Last 5 plans: 08-01 (13 min), 09-01 (8 min), 10-01 (4 min), 11-01 (9 min)
- Trend: v0.6 phases 9-11 consistently fast (4-9 min); well-established patterns from prior phases

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
- [09-01]: g_theme initialized to NULL so module body detects if Clay_NCurses_configure was called
- [09-01]: Renderer module pattern: CEL_Module body registers text measurement + render system
- [10-01]: TTF text engine API (TTF_CreateText + TTF_DrawRendererText) instead of surface->texture pipeline -- follows Clay reference renderer
- [10-01]: Lazy SDL_Renderer creation from SDL3_WindowComponent -- window may not exist at module init time
- [10-01]: Borders as SDL_RenderFillRect per side -- matches Clay reference renderer, proper thickness
- [10-01]: Corner radius deferred to v2 -- requires SDL_RenderGeometry tessellation
- [10-01]: Font size set per text element via TTF_SetFontSize -- supports Clay multi-size text
- [11-01]: Used primitive compositions directly in ui.h (not CEL_Compose wrappers) -- avoids static linkage issues in INTERFACE library headers
- [11-01]: ClaySurface dimensions: 40x24 for NCurses (aspect-ratio-compensated), 1280x720 for SDL3 (pixel-accurate)
- [11-01]: OnUpdate phase shorthand for systems (current v0.4 API convention)

### Pending Todos

None.

### Blockers/Concerns

- v0.5 Phase 5 (Demo App) is incomplete (plans 05-01, 05-02 not finished) -- v0.6 work supersedes this
- Text measurement conflict between NCurses (cells) and SDL3 (pixels) -- resolved: each renderer's CEL_Module registers its own Clay_SetMeasureTextFunction

## Session Continuity

Last session: 2026-03-15
Stopped at: Completed 11-01-PLAN.md (Dual-Renderer Example). v0.6 milestone complete.
Resume file: None
