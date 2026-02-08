# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-07)

**Core value:** Declarative UI development where CELS handles application state and reactivity while Clay handles layout
**Current focus:** Phase 4 verified. Next: Phase 5 (Demo App + Integration)

## Current Position

Phase: 4 of 5 (ncurses Clay Renderer) -- VERIFIED
Plan: 2 of 2 in current phase
Status: Phase verified, goal achieved (5/5 must-haves passed)
Last activity: 2026-02-08 -- Phase 4 verified and complete

Progress: [=========.] 82%

## Performance Metrics

**Velocity:**
- Total plans completed: 9
- Average duration: 2.6 min
- Total execution time: 23 min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 2 | 4 min | 2 min |
| 2 | 3 | 9 min | 3 min |
| 3 | 2 | 5 min | 2.5 min |
| 4 | 2 | 5 min | 2.5 min |

**Recent Trend:**
- Last 5 plans: 03-01 (3 min), 03-02 (2 min), 04-01 (4 min), 04-02 (1 min)
- Trend: Stable 1-4 min per plan

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
- [03-01]: dirty flag = (commands.length > 0) -- simple always-dirty approach, optimize later if needed
- [03-01]: Render dispatch uses internal _cel_clay_get_render_commands/get_layout_dimensions, not public API
- [03-01]: System registration order: layout (PreStore) then dispatch (OnStore), providers finalized lazily after
- [03-02]: Renamed Clay_EngineConfig to ClayEngineConfig (consistent C naming without mixed underscore)
- [03-02]: No error_handler in config -- internal handler sufficient, avoids Clay_ErrorData coupling
- [03-02]: Composable wrappers skip arena -- caller responsible for Clay_Initialize when using individual pieces
- [03-02]: NULL config to Clay_Engine_use means use all defaults (zero-initialized static config)
- [04-01]: TUI_BORDER_SINGLE for v1 borders -- default theme matches, custom chars deferred to v2
- [04-01]: Dual bbox converters: clay_bbox_to_cells (aspect-ratio) vs clay_text_bbox_to_cells (cell-accurate)
- [04-01]: Text x-position still aspect-scaled even for text bbox -- only width is unscaled
- [04-01]: Omitted init_theme_border_chars cchar_t conversion -- not needed with TUI_BORDER_SINGLE
- [04-01]: Stack buffer 512 bytes for StringSlice null-termination, malloc fallback for long strings
- [04-02]: Scroll handler is app-callable plain function, not ECS system -- app controls focus
- [04-02]: Priority fallback: Vim raw_key first, then CELS nav keys, then axis -- prevents double-scrolling
- [04-02]: gg uses cross-frame static state (g_prev_raw_key) -- second g in consecutive frames triggers scroll-to-top

### Pending Todos

None.

### Blockers/Concerns

- No consumer target links to cels-clay yet -- runtime testing requires a consumer executable or test target
- CELS test suite (test_cels) has pre-existing compilation failures from v0.2 API changes (CEL_Call -> CEL_Init) -- unrelated to cels-clay

## Session Continuity

Last session: 2026-02-08
Stopped at: Phase 4 verified and complete. All 5 success criteria met. Next: Phase 5 (Demo App + Integration).
Resume file: None
Key reference: .planning/API-DESIGN.md
