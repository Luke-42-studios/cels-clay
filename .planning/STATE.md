# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-07)

**Core value:** Declarative UI development where CELS handles application state and reactivity while Clay handles layout
**Current focus:** Phase 2 - Layout System Core (Plan 2 of 3 complete)

## Current Position

Phase: 2 of 5 (Layout System Core)
Plan: 2 of 3 in current phase
Status: In progress
Last activity: 2026-02-08 -- Completed 02-02-PLAN.md

Progress: [====......] 33%

## Performance Metrics

**Velocity:**
- Total plans completed: 4
- Average duration: 2.8 min
- Total execution time: 11 min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 2 | 4 min | 2 min |
| 2 | 2 | 7 min | 3.5 min |

**Recent Trend:**
- Last 5 plans: 01-01 (2 min), 01-02 (2 min), 02-01 (3 min), 02-02 (4 min)
- Trend: Slight increase (layout complexity growing)

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: 5-phase structure following build -> layout -> render bridge -> renderer -> demo dependency chain
- [Roadmap]: CORE-01 and CORE-03 (arena init, error handler) placed in Phase 1 with build system rather than Phase 2
- [API]: Clay as component model — ClayUI component with layout function pointer, one module-level layout system walks entity tree (see .planning/API-DESIGN.md)
- [API]: Input via CEL_Use systems on compositions, state mutation via CEL_Update triggers reactive recomposition
- [API]: Dynamic strings need per-frame arena (CLAY_DYN_STRING) — API-07 requirement
- [01-01]: FetchContent_Populate over MakeAvailable (Clay CMakeLists.txt builds examples)
- [01-01]: clay_engine.h does NOT include clay.h — consumers include clay.h separately for CLAY() macros
- [01-02]: Arena size defaults to Clay_MinMemorySize() — no multiplier, consumer overrides via config
- [01-02]: Store Clay_Context* for future multi-context support, unused in Phase 1
- [01-02]: Free g_clay_arena_memory (original malloc ptr), NOT arena.memory (cache-line aligned offset)
- [02-01]: Component named ClayUI (not ClayLayout) matching API-DESIGN.md and API-02
- [02-01]: CEL_Clay uses __COUNTER__ (not __LINE__) for per-call-site uniqueness
- [02-01]: Frame arena 16KB fixed, overflow to stderr, no dynamic growth
- [02-01]: Forward-declared Clay__HashNumber (lacks CLAY_DLL_EXPORT but linker symbol exists from clay_impl.c)
- [02-01]: Text measurement: 1 char = 1 unit width, newlines increment height (terminal cell model)
- [02-02]: System callback uses ecs_iter_t* (flecs native) not CELS_Iter* -- direct flecs system requires flecs callback signature
- [02-02]: Render commands stored in static global, returned by value via _cel_clay_get_render_commands
- [02-02]: Layout cleanup called before Clay arena free in clay_cleanup (independent allocations, safe ordering)

### Pending Todos

None.

### Blockers/Concerns

- cels-ncurses drawing primitives gap: PROJECT.md notes the planned graphics API is "not yet built." Phase 4 may need to build ncurses drawing helpers or work directly on stdscr.
- No consumer target links to cels-clay yet -- runtime testing requires a consumer executable or test target

## Session Continuity

Last session: 2026-02-08
Stopped at: Completed 02-02-PLAN.md (layout system tree walk, PreStore system, CEL_Clay_Children, module wiring). Plan 2 of Phase 2 complete.
Resume file: None
Key reference: .planning/API-DESIGN.md
