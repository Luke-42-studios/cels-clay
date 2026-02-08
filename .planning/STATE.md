# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-07)

**Core value:** Declarative UI development where CELS handles application state and reactivity while Clay handles layout
**Current focus:** Phase 2 complete. Next: Phase 3 - Render Bridge + Module Definition

## Current Position

Phase: 2 of 5 (Layout System Core) -- COMPLETE
Plan: 3 of 3 in current phase
Status: Phase verified, goal achieved
Last activity: 2026-02-08 -- Phase 2 verified (1 gap fixed: CEL_Clay macro syntax)

Progress: [====......] 33%

## Performance Metrics

**Velocity:**
- Total plans completed: 5
- Average duration: 2.6 min
- Total execution time: 13 min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 2 | 4 min | 2 min |
| 2 | 3 | 9 min | 3 min |

**Recent Trend:**
- Last 5 plans: 01-02 (2 min), 02-01 (3 min), 02-02 (4 min), 02-03 (2 min)
- Trend: Stable around 2-4 min per plan

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: 5-phase structure following build -> layout -> render bridge -> renderer -> demo dependency chain
- [Roadmap]: CORE-01 and CORE-03 (arena init, error handler) placed in Phase 1 with build system rather than Phase 2
- [API]: Clay as component model -- ClayUI component with layout function pointer, one module-level layout system walks entity tree (see .planning/API-DESIGN.md)
- [API]: Input via CEL_Use systems on compositions, state mutation via CEL_Update triggers reactive recomposition
- [API]: Dynamic strings need per-frame arena (CLAY_DYN_STRING) -- API-07 requirement
- [01-01]: FetchContent_Populate over MakeAvailable (Clay CMakeLists.txt builds examples)
- [01-01]: clay_engine.h does NOT include clay.h -- consumers include clay.h separately for CLAY() macros
- [01-02]: Arena size defaults to Clay_MinMemorySize() -- no multiplier, consumer overrides via config
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
- [02-03]: ClaySurface composition in header -- CEL_Composition generates static functions, safe across TUs
- [02-03]: Shorthand macro AFTER composition definition to prevent preprocessor token conflict
- [02-03]: No if/else guard on CEL_Clay -- Clay error handler catches misuse

### Pending Todos

None.

### Blockers/Concerns

- cels-ncurses drawing primitives gap: PROJECT.md notes the planned graphics API is "not yet built." Phase 4 may need to build ncurses drawing helpers or work directly on stdscr.
- No consumer target links to cels-clay yet -- runtime testing requires a consumer executable or test target
- CELS test suite (test_cels) has pre-existing compilation failures from v0.2 API changes (CEL_Call -> CEL_Init) -- unrelated to cels-clay

## Session Continuity

Last session: 2026-02-08
Stopped at: Phase 2 Layout System Core verified and complete. All 5 success criteria met.
Resume file: None
Key reference: .planning/API-DESIGN.md
