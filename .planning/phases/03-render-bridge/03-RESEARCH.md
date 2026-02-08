# Phase 3: Render Bridge + Module Definition - Research

**Researched:** 2026-02-08
**Domain:** CELS Feature/Provider pattern, module definition, Clay render commands
**Confidence:** HIGH

## Summary

Phase 3 bridges Clay layout output to renderer backends via CELS Feature/Provider, and restructures module initialization with config + composable sub-modules. Research focused on three domains: (1) the exact Feature/Provider pattern used in CELS and cels-ncurses, (2) the `_CEL_DefineModule` pattern and how config is threaded through it, and (3) Clay's render command API.

Key findings: the Feature/Provider system is well-established with clear patterns in cels-ncurses (`_CEL_DefineFeature` -> `_CEL_Feature` -> `_CEL_Provides`). Module definition uses `_CEL_DefineModule` with a static config global pattern already used by both `TUI_Engine` and the current `Clay_Engine`. The "PostStore" phase requested in CONTEXT.md does not exist in flecs -- the natural phase for the render dispatch is `OnStore` (matching cels-ncurses Renderable), or a custom phase via `_CEL_DefinePhase(.after = CELS_Phase_PreStore)`. Using `OnStore` directly is recommended since it already runs after `PreStore` and matches the existing Renderable pattern.

**Primary recommendation:** Define `ClayRenderable` feature at `CELS_Phase_OnStore`, create a singleton render target entity during module init, register the render dispatch system alongside layout, and restructure `Clay_Engine_use()` as a convenience wrapper over `clay_layout_use()` + `clay_render_use()`.

## Standard Stack

No new libraries needed. This phase uses only existing CELS APIs and Clay APIs.

### Core
| API | Source | Purpose | Why Standard |
|-----|--------|---------|--------------|
| `_CEL_DefineFeature` | cels.h | Define ClayRenderable capability | Standard CELS pattern for feature contracts |
| `_CEL_Feature` | cels.h | Declare component supports feature | Required link between component and feature |
| `_CEL_Provides` | cels.h | Register backend provider callback | Standard CELS pattern for backend implementations |
| `_CEL_DefineModule` | cels.h | Idempotent module initialization | Used by both TUI_Engine and existing Clay_Engine |
| `Clay_EndLayout()` | clay.h | Returns `Clay_RenderCommandArray` | Already in use -- returns by value from layout pass |
| `Clay_RenderCommandArray_Get()` | clay.h | Bounds-checked element access | Official Clay API for iterating render commands |

### Supporting
| API | Source | Purpose | When to Use |
|-----|--------|---------|-------------|
| `cels_system_declare` | cels.h | Register standalone systems | For render dispatch system at specific phase |
| `_CEL_DefinePhase` | cels.h | Create custom execution phase | Only if custom "PostStore" phase is needed |
| `cels_component_register` | cels.h | Register ClayRenderable component | For singleton render target entity |

## Architecture Patterns

### Recommended File Structure
```
include/cels-clay/
  clay_engine.h           # Module facade: Clay_EngineConfig, Clay_Engine_use()
  clay_layout.h           # (exists) Layout API: CEL_Clay, ClaySurface, etc.
  clay_render.h           # NEW: ClayRenderable component, render bridge API
src/
  clay_impl.c             # (exists) CLAY_IMPLEMENTATION
  clay_engine.c           # (exists) Module definition -- restructure to call sub-modules
  clay_layout.c           # (exists) Layout system
  clay_render.c           # NEW: Render bridge -- feature, component, dispatch system
```

### Pattern 1: Feature/Provider Registration (from cels-ncurses tui_renderer.c)

**What:** The exact 3-step pattern used by cels-ncurses to define capabilities and wire backends.
**When to use:** Any module that defines a rendering or processing contract.

```c
// Source: /home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/renderer/tui_renderer.c

// Step 1: Define the feature with a phase
_CEL_DefineFeature(Renderable, .phase = CELS_Phase_OnStore, .priority = 0);

// Step 2: Declare which component supports the feature
// (called in an init function, not at file scope)
void tui_renderer_init(void) {
    _CEL_Feature(Canvas, Renderable);
    _CEL_Provides(TUI, Renderable, Canvas, tui_prov_render_screen);
    _CEL_ProviderConsumes(Text, ClickArea, Selectable, Range, GraphicsSettings);
}
```

**Critical detail:** `_CEL_DefineFeature` is at file scope (creates static variables). `_CEL_Feature` and `_CEL_Provides` are called inside an init function -- NOT at file scope. The init function is called from the module's `_CEL_DefineModule` body.

### Pattern 2: Module Definition with Config (from TUI_Engine)

**What:** Static config global + `_use()` function + `_CEL_DefineModule` body reads the global.
**When to use:** Any module that needs user configuration before initialization.

```c
// Source: /home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/tui_engine.c

// Header declares:
//   extern cels_entity_t TUI_Engine;
//   extern void TUI_Engine_init(void);
//   extern void TUI_Engine_use(TUI_EngineConfig config);

// Implementation:
static TUI_EngineConfig g_tui_config = {0};

_CEL_DefineModule(TUI_Engine) {
    // Read g_tui_config here (it was set before _init() was called)
    CEL_Use(TUI_Window,
        .title = g_tui_config.title ? g_tui_config.title : "CELS App",
        ...
    );
    tui_renderer_init();
    tui_frame_init();
}

void TUI_Engine_use(TUI_EngineConfig config) {
    g_tui_config = config;
    TUI_Engine_init();  // idempotent
}
```

**Critical detail:** `_CEL_DefineModule(Name)` generates:
1. `cels_entity_t Name = 0;` -- the module entity ID
2. `void Name_init(void)` -- public init function with `if (Name != 0) return;` guard
3. `static void Name_init_body(void)` -- the user's block becomes this function body

The `Name_use(config)` function stores config to a static global THEN calls `Name_init()`. This works because init is idempotent -- if called twice, the second call returns immediately.

### Pattern 3: Singleton Entity for Provider Target

**What:** The module creates ONE entity with the ClayRenderable component. Backends provide for this entity's component.
**When to use:** When the feature contract involves module-global data (not per-entity iteration).

```c
// During module init:
static cels_entity_t g_render_target = 0;

void _cel_clay_render_init(void) {
    // Create singleton entity
    ecs_world_t* world = cels_get_world(cels_get_context());
    g_render_target = ecs_entity_init(world, &(ecs_entity_desc_t){
        .name = "ClayRenderTarget"
    });

    // Attach ClayRenderable component to it
    ClayRenderable renderable = { /* initial data */ };
    ecs_set_id(world, g_render_target, ClayRenderableID, sizeof(ClayRenderable), &renderable);
}
```

**Critical detail:** The Feature/Provider system auto-generates a flecs system that queries entities with the specified component. By having exactly ONE entity with ClayRenderable, the provider callback fires once per frame for that entity. The render dispatch system updates the component data before the provider runs.

### Pattern 4: Render Dispatch System (updating the singleton)

**What:** A system at OnStore that copies current render commands into the singleton's component data.
**When to use:** Bridging layout output to provider callbacks.

```c
// Runs at OnStore, BEFORE the provider system queries ClayRenderable entities
static void ClayRenderDispatch_callback(ecs_iter_t* it) {
    (void)it;
    ecs_world_t* world = cels_get_world(cels_get_context());

    // Get current render commands from layout system
    Clay_RenderCommandArray commands = _cel_clay_get_render_commands();

    // Update the singleton's ClayRenderable data
    ClayRenderableData data = {
        .render_commands = commands,
        .layout_width = ...,
        .layout_height = ...,
        .frame_number = ...,
        .delta_time = ...,
        .dirty = ...
    };
    ecs_set_id(world, g_render_target, ClayRenderableID, sizeof(ClayRenderableData), &data);
}
```

**IMPORTANT NOTE:** There is a phase ordering concern here. Both the render dispatch system and the provider system would run at OnStore. Flecs executes systems within the same phase in registration order. Since the render dispatch system is registered during module init (before providers are finalized), it should run first. However, this is an implicit ordering dependency. See Pitfalls section.

### Pattern 5: Alternative -- Direct Dispatch Without Feature/Provider

**What:** Instead of using Feature/Provider (which creates an auto-generated flecs system), the render dispatch system directly invokes a registered callback.
**When to use:** When the provider callback needs data that can't be easily stuffed into a component.

```c
// Registered by backend:
static void (*g_render_callback)(const ClayRenderableData* data) = NULL;

void cel_clay_register_renderer(void (*callback)(const ClayRenderableData*)) {
    g_render_callback = callback;
}

// Dispatch system calls the callback directly:
static void ClayRenderDispatch_callback(ecs_iter_t* it) {
    if (g_render_callback) {
        ClayRenderableData data = { ... };
        g_render_callback(&data);
    } else {
        // Log warning once
        static bool warned = false;
        if (!warned) {
            fprintf(stderr, "[cels-clay] warning: no renderer backend registered\n");
            warned = false;
        }
    }
}
```

**Why not recommended:** The CONTEXT.md explicitly says "Use raw `_CEL_Provides` for registration -- consistent with all other CELS modules." This alternative is noted for completeness only.

### Anti-Patterns to Avoid

- **Defining ClayRenderable at CELS_Phase_OnRender expecting it to run after OnStore:** `CELS_Phase_OnRender` maps to `EcsOnStore` in the backend -- they are the SAME phase. There is no separate render phase after store. Use `CELS_Phase_OnStore` for clarity.

- **Putting render data in a global and having the provider read it directly:** While `_cel_clay_get_render_commands()` already stores commands in a global, the provider callback should receive data through the component on the entity it queries, not by calling a global getter. The getter API is the "advanced" escape hatch per CONTEXT.md.

- **Registering the Feature/Provider at file scope:** `_CEL_DefineFeature` goes at file scope, but `_CEL_Feature` and `_CEL_Provides` MUST be called inside a function (the init function). Calling them at file scope violates C99 static initialization rules.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Idempotent module init | Custom once-guard | `_CEL_DefineModule` macro | Handles entity registration, idempotency guard, naming |
| Phase ordering | Manual dependency tracking | Flecs phase system (`EcsDependsOn`) | Flecs guarantees ordering within pipeline |
| Feature/provider wiring | Custom callback registry | `_CEL_DefineFeature` + `_CEL_Provides` | Auto-generates flecs systems, warns on missing providers |
| Render command iteration | Manual array indexing | `Clay_RenderCommandArray_Get()` | Bounds-checked access, official API |

**Key insight:** The CELS Feature/Provider system auto-generates flecs systems from provider registrations at `cels_finalize_providers()` time (called automatically on first `Engine_Progress`). Don't manually create systems for provider callbacks -- the framework handles this.

## Common Pitfalls

### Pitfall 1: Phase Ordering Between Dispatch and Provider

**What goes wrong:** The render dispatch system (which updates ClayRenderable data) and the auto-generated provider system (which reads it) both run at OnStore. If the provider runs first, it reads stale data.
**Why it happens:** Systems within the same flecs phase run in registration order, which depends on when `_CEL_Provides` was called relative to the dispatch system registration.
**How to avoid:** Register the dispatch system during module init (in `_CEL_DefineModule` body), BEFORE calling the render init function that does `_CEL_Provides`. Since providers are finalized lazily (on first `Engine_Progress`), the dispatch system is guaranteed to be registered before the provider system is created. Verify with flecs explorer or debug output.
**Warning signs:** Renderer always shows blank/stale frames, one frame behind.

### Pitfall 2: "PostStore" Phase Does Not Exist

**What goes wrong:** The CONTEXT.md specifies "PostStore phase" but flecs pipeline has no such phase. The phases are: `PreStore -> OnStore -> PostFrame`.
**Why it happens:** PostStore was discussed as a concept (after layout at PreStore) but doesn't map to a real flecs phase.
**How to avoid:** Use `OnStore` (which naturally runs after `PreStore`) for the render dispatch. This matches how cels-ncurses already does it (Renderable at CELS_Phase_OnStore). If stricter ordering is needed, use `_CEL_DefinePhase(PostStore, .after = CELS_Phase_PreStore)` to create a custom phase between PreStore and OnStore.
**Warning signs:** Phase constant doesn't exist; compile error or runtime misfire.

### Pitfall 3: Clay_RenderCommandArray Lifetime

**What goes wrong:** Render commands are only valid until the next `Clay_BeginLayout()` call. If the renderer reads them too late (e.g., in PostFrame), Clay may have already started the next frame's layout.
**Why it happens:** Clay's render commands point into Clay's internal arena memory. `Clay_BeginLayout()` resets this memory.
**How to avoid:** Render commands MUST be consumed within the same frame, after `Clay_EndLayout()` (which happens at PreStore) and before the next `Clay_BeginLayout()` (which happens at the next frame's PreStore). Running at OnStore is safe because OnStore runs after PreStore within the same frame.
**Warning signs:** Garbled text, random render data, segfaults.

### Pitfall 4: ClayRenderable vs Renderable Name Collision

**What goes wrong:** cels-ncurses defines `Renderable` feature. cels-clay defining another `Renderable` would collide.
**Why it happens:** `_CEL_DefineFeature(Renderable, ...)` creates `Renderable_feature_id` static variable. If both modules are included in the same translation unit, they shadow each other.
**How to avoid:** Name the clay feature `ClayRenderable` (as specified in CONTEXT.md). This is a distinct feature with its own component type (`ClayRenderableData`), separate from the ncurses `Renderable` feature that queries `Canvas` entities.
**Warning signs:** Compile warnings about shadowed variables; wrong feature being matched.

### Pitfall 5: Config Struct Passed by Value vs Pointer

**What goes wrong:** CONTEXT.md says `Clay_Engine_use(&(ClayEngineConfig){.arena_size = 8192})` (pointer), but existing code uses `Clay_Engine_use(Clay_EngineConfig config)` (by value).
**Why it happens:** The current Phase 1/2 implementation passes config by value. The CONTEXT.md discussion decided on pointer.
**How to avoid:** Change the signature to take a pointer: `void Clay_Engine_use(const ClayEngineConfig* config)`. This matches the CONTEXT.md decision. The compound literal `&(ClayEngineConfig){...}` creates a pointer to a temporary -- valid in C99 within the enclosing block.
**Warning signs:** API inconsistency between documentation and code.

### Pitfall 6: Missing Renderer Warning Timing

**What goes wrong:** The "log warning once if no renderer backend registered on first frame" check runs too early (during init) when providers haven't been finalized yet.
**Why it happens:** Providers are finalized lazily on first `Engine_Progress`, not during module init. Checking during init would always find no provider.
**How to avoid:** Check inside the render dispatch system callback (which runs each frame at OnStore). Use a `static bool warned = false` guard. On the first frame, if no provider callback has been registered, print the warning and set `warned = true`.
**Warning signs:** Warning fires immediately on startup even when a backend is registered.

## Code Examples

### ClayRenderable Component Definition

```c
// In clay_render.h
// Source: Pattern follows cels-ncurses tui_renderer.c

typedef struct ClayRenderableData {
    Clay_RenderCommandArray render_commands;
    float layout_width;
    float layout_height;
    uint32_t frame_number;
    float delta_time;
    bool dirty;
} ClayRenderableData;

// Component ID and ensure function (extern linkage for cross-file use)
extern cels_entity_t ClayRenderableDataID;
extern cels_entity_t ClayRenderableData_ensure(void);

// Getter API for advanced users
extern Clay_RenderCommandArray cel_clay_get_render_commands(void);

// Render bridge lifecycle (called from clay_engine.c)
extern void _cel_clay_render_init(void);
extern void _cel_clay_render_system_register(void);
```

### Feature Definition and Render Dispatch System

```c
// In clay_render.c
// Source: Pattern from cels-ncurses/src/renderer/tui_renderer.c

#include "cels-clay/clay_render.h"
#include <cels/cels.h>
#include <flecs.h>

// Feature definition at file scope
_CEL_DefineFeature(ClayRenderable, .phase = CELS_Phase_OnStore, .priority = 0);

// Component registration
cels_entity_t ClayRenderableDataID = 0;

cels_entity_t ClayRenderableData_ensure(void) {
    if (ClayRenderableDataID == 0) {
        ClayRenderableDataID = cels_component_register("ClayRenderableData",
            sizeof(ClayRenderableData), CELS_ALIGNOF(ClayRenderableData));
    }
    return ClayRenderableDataID;
}

// Singleton render target
static cels_entity_t g_render_target = 0;
static uint32_t g_frame_number = 0;
static bool g_no_renderer_warned = false;

// Render dispatch system -- updates singleton each frame
static void ClayRenderDispatch_callback(ecs_iter_t* it) {
    (void)it;
    g_frame_number++;

    ecs_world_t* world = cels_get_world(cels_get_context());

    Clay_RenderCommandArray commands = _cel_clay_get_render_commands();

    // Update the singleton entity's component
    ClayRenderableData data = {
        .render_commands = commands,
        .layout_width = 0,   // TODO: read from ClaySurfaceConfig
        .layout_height = 0,
        .frame_number = g_frame_number,
        .delta_time = it->delta_time,
        .dirty = (commands.length > 0)
    };

    ecs_set_id(world, g_render_target, ClayRenderableDataID,
               sizeof(ClayRenderableData), &data);
}

// Getter API for advanced users
Clay_RenderCommandArray cel_clay_get_render_commands(void) {
    return _cel_clay_get_render_commands();
}

// Init: create singleton entity, register component
void _cel_clay_render_init(void) {
    ClayRenderableData_ensure();

    ecs_world_t* world = cels_get_world(cels_get_context());

    // Create singleton render target entity
    g_render_target = ecs_entity_init(world, &(ecs_entity_desc_t){
        .name = "ClayRenderTarget"
    });

    // Attach initial (empty) ClayRenderable data
    ClayRenderableData initial = {0};
    ecs_set_id(world, g_render_target, ClayRenderableDataID,
               sizeof(ClayRenderableData), &initial);

    // Declare feature relationship
    _CEL_Feature(ClayRenderableData, ClayRenderable);
}

// System registration: dispatch at OnStore, before providers are finalized
void _cel_clay_render_system_register(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());

    ecs_system_desc_t sys_desc = {0};
    ecs_entity_desc_t entity_desc = {0};
    entity_desc.name = "ClayRenderDispatch";

    ecs_id_t phase_ids[3] = {
        ecs_pair(EcsDependsOn, EcsOnStore),
        EcsOnStore,
        0
    };
    entity_desc.add = phase_ids;

    sys_desc.entity = ecs_entity_init(world, &entity_desc);
    sys_desc.callback = ClayRenderDispatch_callback;

    ecs_system_init(world, &sys_desc);
}
```

### Restructured Clay_Engine Module

```c
// In clay_engine.c (restructured)
// Source: Pattern from cels-ncurses/src/tui_engine.c

#include "cels-clay/clay_engine.h"
#include "cels-clay/clay_layout.h"
#include "cels-clay/clay_render.h"

static ClayEngineConfig g_clay_config = {0};

_CEL_DefineModule(Clay_Engine) {
    // 1. Arena and Clay initialization (existing code)
    uint32_t min_memory = Clay_MinMemorySize();
    uint32_t arena_size = /* ... existing logic ... */;
    // ... allocate, initialize Clay ...

    // 2. Initialize layout subsystem
    _cel_clay_layout_init();

    // 3. Initialize render bridge (creates singleton, registers feature)
    _cel_clay_render_init();

    // 4. Register systems in correct order:
    //    a) Layout at PreStore (existing)
    //    b) Render dispatch at OnStore (new -- must register BEFORE providers)
    _cel_clay_layout_system_register();
    _cel_clay_render_system_register();

    // 5. Register cleanup
    atexit(clay_cleanup);
}

void Clay_Engine_use(const ClayEngineConfig* config) {
    if (config) {
        g_clay_config = *config;
    }
    Clay_Engine_init();  // idempotent
}
```

### Backend Registration (Consumer Side)

```c
// In the application or backend module (e.g., cels-ncurses clay renderer)
// This is how a backend registers to receive Clay render commands

#include <cels-clay/clay_render.h>

static void my_clay_renderer(CELS_Iter* it) {
    // The provider callback queries entities with ClayRenderableData
    ecs_world_t* world = cels_get_world(cels_get_context());

    // Get the ClayRenderableData from the iterated entity
    ClayRenderableData* data = (ClayRenderableData*)
        cels_iter_column(it, ClayRenderableDataID, sizeof(ClayRenderableData));
    if (!data) return;

    int count = cels_iter_count(it);
    for (int i = 0; i < count; i++) {
        if (!data[i].dirty) continue;

        Clay_RenderCommandArray commands = data[i].render_commands;
        for (int j = 0; j < commands.length; j++) {
            Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&commands, j);
            // Dispatch to backend drawing primitives
            switch (cmd->commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                    // ...
                    break;
                case CLAY_RENDER_COMMAND_TYPE_TEXT:
                    // ...
                    break;
                // ... etc
            }
        }
    }
}

// In backend init:
void my_backend_clay_init(void) {
    _CEL_Provides(MyBackend, ClayRenderable, ClayRenderableData, my_clay_renderer);
}
```

### Composable Sub-Module Pattern

```c
// In clay_engine.h -- composable pieces

// Individual sub-module inits (for advanced users)
extern void clay_layout_use(void);   // Just layout, no render bridge
extern void clay_render_use(void);   // Just render bridge, no layout

// Convenience wrapper
extern void Clay_Engine_use(const ClayEngineConfig* config);

// In clay_engine.c:
void clay_layout_use(void) {
    _cel_clay_layout_init();
    _cel_clay_layout_system_register();
}

void clay_render_use(void) {
    _cel_clay_render_init();
    _cel_clay_render_system_register();
}

// Clay_Engine_use calls both plus arena setup
```

## State of the Art

| Old Approach (Phase 1-2) | Current Approach (Phase 3) | Impact |
|--------------------------|---------------------------|--------|
| `Clay_Engine_use(Clay_EngineConfig config)` by value | `Clay_Engine_use(const ClayEngineConfig* config)` by pointer | Matches CONTEXT.md decision |
| Config has only `arena_size` | Config adds `error_handler`, `initial_width`, `initial_height` | More configurable |
| Render commands in global only | Both global getter + component-based provider | Dual access pattern |
| Single monolithic module init | Composable `clay_layout_use()` + `clay_render_use()` | Advanced users can pick pieces |

## Phase Ordering Summary

```
Frame N:
  PreStore   -> ClayLayoutSystem (walks tree, BeginLayout/EndLayout)
                  g_last_render_commands = Clay_EndLayout()

  OnStore    -> ClayRenderDispatch (copies commands into ClayRenderableData component)
             -> [Provider system] (backend reads ClayRenderableData, draws)

  PostFrame  -> (nothing from cels-clay)

Frame N+1:
  PreStore   -> ClayLayoutSystem (resets arena, begins new layout)
                  Previous render commands invalidated
```

This ordering is guaranteed because:
1. PreStore runs before OnStore (flecs phase pipeline)
2. Within OnStore, dispatch system was registered before provider system (registration order)

## Open Questions

1. **Dirty flag implementation**
   - What we know: The flag should indicate whether layout changed since last frame. Renderers skip redraw when clean.
   - What's unclear: Comparing `Clay_RenderCommandArray` across frames is not trivial (pointer comparison won't work because Clay reuses arena memory). Options: (a) always mark dirty (simple, correct), (b) hash command array contents (complex, fragile), (c) compare command array length + a few key fields (heuristic).
   - Recommendation: Start with "always dirty" (dirty = commands.length > 0). Optimize later if profiling shows wasted render cycles.

2. **ClaySurfaceConfig dimensions for render data**
   - What we know: The provider callback should receive layout dimensions. These come from ClaySurfaceConfig.
   - What's unclear: How the render dispatch system reads the current ClaySurfaceConfig values. The layout system already queries these, but the render dispatch needs them too.
   - Recommendation: Store last-used dimensions in a static global during the layout pass, similar to how `g_last_render_commands` is stored.

3. **System registration order guarantee**
   - What we know: Flecs runs systems within the same phase in registration order. Dispatch must run before the provider system.
   - What's unclear: Whether `cels_finalize_providers()` always creates provider systems AFTER all manually registered systems.
   - Recommendation: Registration order is reliable because: (a) module init registers dispatch system, (b) provider systems are created on first `Engine_Progress()`, which happens after all module init. Validate with flecs explorer.

## Sources

### Primary (HIGH confidence)
- `/home/cachy/workspaces/libs/cels/include/cels/cels.h` - Full macro definitions for `_CEL_DefineModule`, `_CEL_DefineFeature`, `_CEL_Feature`, `_CEL_Provides`, `_CEL_DefinePhase`, `CELS_Phase` enum
- `/home/cachy/workspaces/libs/cels/src/core/flecs_backend.cpp` - Phase mapping (`CELS_Phase_OnRender` maps to `EcsOnStore`), Feature/Provider finalization logic
- `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/renderer/tui_renderer.c` - Reference Feature/Provider implementation (3-step pattern)
- `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/tui_engine.c` - Reference module with config pattern
- `/home/cachy/workspaces/libs/cels/modules/cels-clay/src/clay_engine.c` - Current module implementation (Phase 1-2)
- `/home/cachy/workspaces/libs/cels/modules/cels-clay/src/clay_layout.c` - Current layout system with `g_last_render_commands`, `_cel_clay_get_render_commands()`
- `/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-src/clay.h` - Clay_RenderCommandArray struct (lines 708-715), Clay_RenderCommand struct (lines 683-705), Clay_EndLayout() (line 857)

### Secondary (MEDIUM confidence)
- `/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-src/renderers/terminal/clay_renderer_terminal_ansi.c` - Reference terminal renderer showing render command consumption pattern
- `/home/cachy/workspaces/libs/cels/modules/cels-clay/build/_deps/clay-src/examples/terminal-example/main.c` - Clay terminal example showing `Clay_EndLayout()` -> render pattern
- `/home/cachy/workspaces/libs/cels/build/_deps/flecs-src/include/flecs.h` - Flecs phase constants (PreStore, OnStore, PostFrame -- no PostStore)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All patterns verified directly from codebase source files
- Architecture: HIGH - Exact patterns copied from working cels-ncurses and existing clay_engine implementations
- Pitfalls: HIGH - Phase mapping verified in flecs_backend.cpp; PostStore absence confirmed in flecs headers
- Code examples: MEDIUM - Synthesized from multiple verified patterns; will need compile validation
- Open questions: MEDIUM - Reasonable recommendations but need implementation validation

**Research date:** 2026-02-08
**Valid until:** 2026-03-08 (stable -- CELS v0.1 API is frozen on main, v2 branch is active)
