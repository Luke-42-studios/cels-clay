---
phase: 02-layout-system-core
verified: 2026-02-08T19:15:00Z
status: gaps_found
score: 4/5 must-haves verified
gaps:
  - truth: "CEL_Clay() scope inside a composition opens an entity-backed CLAY() block and all standard Clay macros work inside it"
    status: failed
    reason: "CEL_Clay macro passes auto-ID positionally to CLAY() but CLAY() uses a wrapper struct (Clay__Clay_ElementDeclarationWrapper) that requires designated initializer syntax. The header's own documented usage example fails to compile."
    artifacts:
      - path: "include/cels-clay/clay_layout.h"
        issue: "Line 133: CEL_Clay macro defined as CLAY(_cel_clay_auto_id(__COUNTER__), __VA_ARGS__) -- positional auto-ID breaks CLAY()'s CLAY__CONFIG_WRAPPER pattern which wraps args in a single-member wrapper struct"
    missing:
      - "CEL_Clay macro must wrap args in braces with .id designator: CLAY({ .id = _cel_clay_auto_id(__COUNTER__), __VA_ARGS__ })"
      - "Verify the fixed macro compiles with all documented usage examples (CEL_Clay(.layout = {...}), CEL_Clay(.backgroundColor = ...), etc.)"
---

# Phase 2: Layout System Core Verification Report

**Phase Goal:** Developers can write CELS compositions that contribute CLAY() blocks to a single per-frame layout tree with correct nesting and ordering
**Verified:** 2026-02-08T19:15:00Z
**Status:** gaps_found
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | CEL_Clay() scope opens entity-backed CLAY() block with standard Clay macros working | FAILED | CEL_Clay macro produces compilation error: `Clay__Clay_ElementDeclarationWrapper has no member named 'layout'`. Tested with the header's own documented example. Root cause: positional auto-ID arg breaks CLAY() wrapper struct pattern. |
| 2 | Parent-child entity relationships produce correctly nested CLAY() output in depth-first order | VERIFIED | `clay_walk_entity` (line 236) recursively calls `clay_walk_children` (line 259) using `ecs_children`/`ecs_children_next`. Save/restore of `g_layout_current_entity` ensures correct nesting. Non-ClayUI entities are transparent pass-throughs (line 250-253). |
| 3 | Layout system runs at PreStore phase each frame with single BeginLayout/EndLayout pass | VERIFIED | `ClayLayoutSystem_callback` (line 311) registered at `EcsPreStore` (line 370). Calls `Clay_BeginLayout` (line 335), tree walk (line 340), `Clay_EndLayout` (line 347) in correct sequence. |
| 4 | Terminal resize updates propagate to Clay_SetLayoutDimensions | VERIFIED | `ClayLayoutSystem_callback` reads `ClaySurfaceConfig.width/.height` each frame (line 322) and calls `Clay_SetLayoutDimensions` (line 326-328) before BeginLayout. ClaySurface composition (header line 89-91) attaches ClaySurfaceConfig with reactive width/height props. |
| 5 | Text measurement callback is registered and available for renderer backends | VERIFIED | `_cel_clay_layout_init` (line 205) calls `Clay_SetMeasureTextFunction(_cel_clay_measure_text, NULL)`. Callback implements character-cell counting (line 126-151). Signature matches Clay v0.14 (`Clay_StringSlice`, `Clay_TextElementConfig*`, `void*`). |

**Score:** 4/5 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `include/cels-clay/clay_layout.h` | Complete public API with macros, components, declarations | EXISTS + SUBSTANTIVE (193 lines) + PARTIAL WIRED | Contains all macros, components, declarations. ClaySurface composition present. CEL_Clay macro has implementation bug (wrong arg passing to CLAY()). |
| `src/clay_layout.c` | Complete layout system implementation | EXISTS + SUBSTANTIVE (380 lines) + WIRED | Component registration, frame arena, text measurement, tree walk, PreStore system, system registration -- all implemented. No stubs remaining. |
| `src/clay_engine.c` | Module init wires layout subsystem | EXISTS + SUBSTANTIVE (129 lines) + WIRED | Calls `_cel_clay_layout_init()` (line 113), `_cel_clay_layout_system_register()` (line 116), `_cel_clay_layout_cleanup()` (line 73). All calls come after `Clay_Initialize`. |
| `CMakeLists.txt` | Updated target_sources with clay_layout.c | EXISTS + SUBSTANTIVE (56 lines) + WIRED | `clay_layout.c` added to `target_sources(cels-clay INTERFACE ...)` at line 46. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `clay_engine.c` | `clay_layout.c` | `_cel_clay_layout_init`, `_cel_clay_layout_system_register`, `_cel_clay_layout_cleanup` calls | WIRED | Module init calls layout init after Clay_Initialize (line 113), registers system (line 116), cleanup called before arena free (line 73). |
| `clay_layout.c ClayLayoutSystem` | `Clay_BeginLayout/Clay_EndLayout` | System callback brackets tree walk | WIRED | `ClayLayoutSystem_callback` calls `Clay_BeginLayout` (line 335), walks tree (line 340), calls `Clay_EndLayout` (line 347). |
| `clay_layout.c clay_walk_entity` | `ClayUI.layout_fn` | Tree walk calls stored function pointers | WIRED | `clay_walk_entity` gets `ClayUI` via `ecs_get_id` (line 237-238), calls `layout->layout_fn(world, entity)` (line 249). |
| `clay_layout.c _cel_clay_emit_children` | `clay_walk_children` | CEL_Clay_Children macro delegates to child walk | WIRED | `_cel_clay_emit_children` calls `clay_walk_children(g_layout_world, g_layout_current_entity)` (line 282). Guard for misuse outside layout pass (line 278-280). |
| `CEL_Clay macro` | `CLAY macro` | Wraps CLAY() with auto-ID | BROKEN | Macro passes auto-ID positionally but CLAY() expects `Clay_ElementDeclaration` struct fields. Causes compile error when used. |
| `ClaySurface composition` | `ClaySurfaceConfig component` | `CEL_Has(ClaySurfaceConfig, ...)` in composition body | WIRED | ClaySurface composition (header line 89-91) correctly attaches ClaySurfaceConfig with width/height from props. Compiles successfully. |

### Requirements Coverage

| Requirement | Status | Blocking Issue |
|-------------|--------|----------------|
| CORE-04: Terminal dimensions synced to Clay_SetLayoutDimensions | SATISFIED | -- |
| API-01: CEL_Clay() scope macro opens entity-backed CLAY() scope | BLOCKED | CEL_Clay macro produces compile error |
| API-02: ClayUI component with layout function pointer | SATISFIED | Component registered, tree walk uses it |
| API-03: clay_emit_children() helper walks child entities | SATISFIED | `_cel_clay_emit_children` implemented and wired |
| API-04: Text measurement hook point for renderer backends | SATISFIED | `Clay_SetMeasureTextFunction` called in init |
| API-05: All Clay macros work inside CEL_Clay blocks | BLOCKED | CEL_Clay macro itself fails to compile, so nothing works inside it |
| API-06: Cell-aware sizing helpers | N/A (by design) | Clay's 1:1 cell unit system makes helpers redundant |
| API-07: Dynamic string helper macro | SATISFIED | `CEL_Clay_Text(buf, len)` copies to frame arena, compiles correctly |
| API-08: Auto Clay ID generation from CELS context | PARTIAL | `_cel_clay_auto_id` function works, but CEL_Clay macro that calls it is broken |
| PIPE-03: Layout system runs at PreStore phase | SATISFIED | Registered at EcsPreStore, verified in flecs headers |
| PIPE-05: Entity tree walked depth-first parent-before-child | SATISFIED | Recursive walk with ecs_children confirmed |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| -- | -- | -- | -- | No anti-patterns (TODO/FIXME/stub/placeholder) found in any modified file |

### Human Verification Required

### 1. Runtime Layout Pipeline Test

**Test:** Build a minimal consumer app that links to cels-clay, creates a ClaySurface with two child entities having ClayUI layout functions (using direct CLAY() calls instead of broken CEL_Clay), and verify layout system executes at PreStore.
**Expected:** Clay_BeginLayout/EndLayout called each frame, tree walk invokes layout functions, render commands stored.
**Why human:** No consumer target exists yet -- INTERFACE library sources only compile when linked. Runtime behavior (flecs system execution, ECS query results) cannot be verified statically.

### 2. Depth-First Walk Order Verification

**Test:** Create parent with 3 children each having unique layout functions. Add debug prints to confirm invocation order matches entity creation order (depth-first).
**Expected:** Parent layout called first, then child 1, child 2, child 3 in creation order.
**Why human:** `ecs_children` order is documented as creation order in flecs, but runtime confirmation needed.

### Gaps Summary

One critical gap blocks phase goal achievement: the `CEL_Clay()` macro -- the primary developer-facing API for opening Clay layout blocks -- fails to compile when used.

**Root cause:** `CEL_Clay(...)` is defined as `CLAY(_cel_clay_auto_id(__COUNTER__), __VA_ARGS__)`. This passes the auto-generated `Clay_ElementId` as a positional argument to `CLAY()`. However, Clay v0.14's `CLAY()` macro uses an internal wrapper struct pattern (`CLAY__CONFIG_WRAPPER`) that wraps all arguments in `(Clay__Clay_ElementDeclarationWrapper){ __VA_ARGS__ }.wrapped`. The positional auto-ID becomes the initializer for the wrapper's single `wrapped` field, and subsequent designated initializers like `.layout` fail because they reference members of the wrapper struct, not `Clay_ElementDeclaration`.

**Fix:** Change line 133 of `clay_layout.h` from:
```c
#define CEL_Clay(...) \
    CLAY(_cel_clay_auto_id(__COUNTER__), __VA_ARGS__)
```
to:
```c
#define CEL_Clay(...) \
    CLAY({ .id = _cel_clay_auto_id(__COUNTER__), __VA_ARGS__ })
```

This is a one-line fix verified to compile correctly with all documented usage patterns. The fix wraps arguments in braces (as CLAY() expects a struct initializer or a struct variable) and uses `.id =` designated initializer for the auto-generated ID.

All other phase infrastructure (tree walk, PreStore system, frame arena, text measurement, ClaySurface composition, module wiring) is fully implemented and correctly connected. Only the CEL_Clay macro needs this fix.

---

_Verified: 2026-02-08T19:15:00Z_
_Verifier: Claude (gsd-verifier)_
