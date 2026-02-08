---
phase: 03-render-bridge
verified: 2026-02-08T20:15:00Z
status: passed
score: 3/3 must-haves verified
gaps: []
---

# Phase 3: Render Bridge + Module Definition Verification Report

**Phase Goal:** Layout results are exposed to renderer backends through the CELS Feature/Provider pattern, and the module has a clean single-call initialization
**Verified:** 2026-02-08T20:15:00Z
**Status:** passed
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | ClayRenderable Feature is defined and a Provider callback receives the Clay_RenderCommandArray each frame | VERIFIED | `_CEL_DefineFeature(ClayRenderable, .phase = CELS_Phase_OnStore, .priority = 0)` at file scope in `src/clay_render.c:35`. `_CEL_Feature(ClayRenderableData, ClayRenderable)` called in init function at `src/clay_render.c:121`. `ClayRenderableData` struct contains `Clay_RenderCommandArray render_commands` field at `include/cels-clay/clay_render.h:33`. Matches 3-step pattern from `examples/render_provider.h:39`. Backend registration documented: `_CEL_Provides(MyBackend, ClayRenderable, ClayRenderableData, my_renderer)` in header comment. |
| 2 | Render dispatch runs at OnStore phase (after layout is complete) and backends can iterate all render commands | VERIFIED | `ClayRenderDispatch_callback` at `src/clay_render.c:69` reads render commands via `_cel_clay_get_render_commands()`, packages into `ClayRenderableData`, and writes to singleton entity via `ecs_set_id()`. System registered at `EcsOnStore` phase (`src/clay_render.c:140-141`). Layout system at `EcsPreStore` (`src/clay_layout.c:379`) runs first. Registration order in `src/clay_engine.c:125-126` ensures dispatch registers before providers. |
| 3 | CEL_DefineModule(Clay_Engine) bundles layout + render systems and Clay_Engine_use() initializes the full module in one call | VERIFIED | `_CEL_DefineModule(Clay_Engine)` at `src/clay_engine.c:85` calls `_cel_clay_layout_init()` (line 117), `_cel_clay_render_init()` (line 120), `_cel_clay_layout_system_register()` (line 125), `_cel_clay_render_system_register()` (line 126). `Clay_Engine_use(const ClayEngineConfig*)` at `src/clay_engine.c:136` copies config and calls `Clay_Engine_init()`. Composable sub-modules `clay_layout_use()` (line 152) and `clay_render_use()` (line 157) also available. |

**Score:** 3/3 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `include/cels-clay/clay_render.h` | ClayRenderableData struct, component ID, getter API, render init declarations | EXISTS, SUBSTANTIVE (66 lines), WIRED | Struct has 6 fields (render_commands, layout_width, layout_height, frame_number, delta_time, dirty). Included by `clay_engine.h:31` and `clay_render.c:19`. No stubs. |
| `src/clay_render.c` | Feature definition, component registration, singleton entity, dispatch system | EXISTS, SUBSTANTIVE (151 lines), WIRED | `_CEL_DefineFeature` at line 35, `ClayRenderableData_ensure` at line 43, `g_render_target` singleton at line 55, `ClayRenderDispatch_callback` at line 69, init at line 108, system register at line 132. No stubs or TODOs. |
| `include/cels-clay/clay_engine.h` | ClayEngineConfig with pointer API, composable sub-module declarations | EXISTS, SUBSTANTIVE (62 lines), WIRED | `ClayEngineConfig` struct with `arena_size`, `initial_width`, `initial_height`. `Clay_Engine_use(const ClayEngineConfig*)` pointer signature. `clay_layout_use()` and `clay_render_use()` declared. Includes `clay_render.h` transitively. |
| `src/clay_engine.c` | Module definition calling layout + render init, composable wrappers | EXISTS, SUBSTANTIVE (161 lines), WIRED | `_CEL_DefineModule(Clay_Engine)` at line 85 calls all 4 init/register functions. Pointer-based `Clay_Engine_use` at line 136. Composable wrappers at lines 152-160. No stubs. |
| `include/cels-clay/clay_layout.h` | Layout dimension getter declaration | EXISTS, SUBSTANTIVE (195 lines), WIRED | `_cel_clay_get_layout_dimensions()` declared at line 182. `_cel_clay_get_render_commands()` declared at line 181. Both called from `clay_render.c`. |
| `src/clay_layout.c` | Layout dimension storage during layout pass | EXISTS, SUBSTANTIVE (389 lines), WIRED | `g_last_layout_dimensions` at line 166. Set during layout pass at lines 335-338. Getter `_cel_clay_get_layout_dimensions()` at line 298. |
| `CMakeLists.txt` | clay_render.c added to INTERFACE library sources | EXISTS, SUBSTANTIVE (57 lines), WIRED | `src/clay_render.c` listed at line 47 in `target_sources`. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `clay_render.c` | `clay_layout.c` | `_cel_clay_get_render_commands()` + `_cel_clay_get_layout_dimensions()` | WIRED | Called at `clay_render.c:73-74`. Declared in `clay_layout.h:181-182`. Implemented in `clay_layout.c:294-300`. |
| `clay_render.c` | singleton entity | `ecs_set_id` updating ClayRenderableData each frame | WIRED | `ecs_set_id(world, g_render_target, ClayRenderableDataID, sizeof(ClayRenderableData), &data)` at `clay_render.c:85-86`. Singleton created at `clay_render.c:113-115`. |
| `clay_render.c` | CELS Feature/Provider | `_CEL_DefineFeature` + `_CEL_Feature` | WIRED | `_CEL_DefineFeature(ClayRenderable, .phase = CELS_Phase_OnStore, .priority = 0)` at file scope (line 35). `_CEL_Feature(ClayRenderableData, ClayRenderable)` inside init function (line 121). Matches reference pattern in `examples/render_provider.h:39,245`. |
| `clay_engine.c` | `clay_render.c` | `_cel_clay_render_init()` + `_cel_clay_render_system_register()` | WIRED | Called at `clay_engine.c:120,126`. Functions defined at `clay_render.c:108,132`. |
| `clay_engine.c` | `clay_layout.c` | `_cel_clay_layout_init()` + `_cel_clay_layout_system_register()` | WIRED | Called at `clay_engine.c:117,125`. Functions defined at `clay_layout.c:194,370`. |
| `clay_engine.h` | public API | `Clay_Engine_use(const ClayEngineConfig*)` pointer signature | WIRED | Declared at `clay_engine.h:54`. Implemented at `clay_engine.c:136`. Takes const pointer, NULL for defaults. |

### Requirements Coverage

| Requirement | Status | Evidence |
|-------------|--------|----------|
| CORE-02: CEL_DefineModule(Clay_Engine) facade bundles layout + render systems | SATISFIED | `_CEL_DefineModule(Clay_Engine)` at `clay_engine.c:85` calls all init and system registration functions in correct order. `Clay_Engine_use()` is the single-call entry point. |
| PIPE-01: ClayRenderable Feature defined by cels-clay module | SATISFIED | `_CEL_DefineFeature(ClayRenderable, .phase = CELS_Phase_OnStore, .priority = 0)` at `clay_render.c:35`. `_CEL_Feature(ClayRenderableData, ClayRenderable)` at `clay_render.c:121`. |
| PIPE-02: Provider callback pattern dispatches Clay_RenderCommandArray to registered backends | SATISFIED | `ClayRenderDispatch_callback` at `clay_render.c:69` packages `Clay_RenderCommandArray` into `ClayRenderableData` and updates singleton entity. Backends register via `_CEL_Provides(Backend, ClayRenderable, ClayRenderableData, callback)`. Header documents pattern at `clay_render.h:10`. |
| PIPE-04: Render dispatch runs at OnRender phase (after layout complete) | SATISFIED | Implementation uses `CELS_Phase_OnStore` / `EcsOnStore`. Research finding (`03-RESEARCH.md:198,550`) confirmed `CELS_Phase_OnRender` maps to `EcsOnStore` in the flecs backend -- they are the SAME phase. Using `OnStore` is consistent with the reference implementation in `examples/render_provider.h:39`. REQUIREMENTS.md text says "OnRender" but intent (after layout) is achieved. |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (none) | - | - | - | No TODO, FIXME, placeholder, or stub patterns found in any source or header file |

### Human Verification Required

### 1. End-to-end Feature/Provider wiring at runtime

**Test:** Create a minimal app that calls `Clay_Engine_use(NULL)`, defines a ClaySurface composition, and registers a provider via `_CEL_Provides(TestBackend, ClayRenderable, ClayRenderableData, test_callback)`. Run one frame and verify `test_callback` receives a non-empty `ClayRenderableData`.
**Expected:** The callback fires with `render_commands.length > 0` and valid `layout_width`/`layout_height`.
**Why human:** Requires runtime execution with the full CELS framework. Structural verification confirms correct wiring but cannot prove the flecs pipeline dispatches systems in the correct order at runtime.

### 2. Provider ordering within OnStore phase

**Test:** Verify that `ClayRenderDispatch_callback` runs BEFORE the auto-generated provider system within OnStore.
**Expected:** Provider receives current-frame data, not stale/empty data from the previous frame.
**Why human:** Flecs system ordering within a phase depends on registration order, which is an implicit runtime behavior. Research confirms the design should work (dispatch registered before provider finalization), but runtime verification is needed.

### Gaps Summary

No gaps found. All three success criteria from the ROADMAP are met:

1. **ClayRenderable Feature** -- Defined at file scope with `_CEL_DefineFeature`, component declared with `_CEL_Feature` in init, struct contains all expected fields including `Clay_RenderCommandArray render_commands`.

2. **Render dispatch at OnStore** -- System registered at `EcsOnStore` phase, reads from layout subsystem getters, writes to singleton entity with `ecs_set_id`. Registration order in module init ensures dispatch runs before providers.

3. **CEL_DefineModule(Clay_Engine) facade** -- Module body bundles arena allocation, Clay initialization, layout init, render bridge init, system registration (layout at PreStore, dispatch at OnStore), and cleanup. `Clay_Engine_use(const ClayEngineConfig*)` provides single-call initialization with pointer-based config. Composable sub-modules available for advanced users.

All 4 mapped requirements (CORE-02, PIPE-01, PIPE-02, PIPE-04) are satisfied. Build compiles successfully with zero errors and zero warnings.

---

_Verified: 2026-02-08T20:15:00Z_
_Verifier: Claude (gsd-verifier)_
