# Architecture: cels-clay Module

**Domain:** CELS-Clay hybrid UI layout integration
**Researched:** 2026-02-07
**Confidence:** HIGH (derived from Clay 0.14 header analysis + CELS v0.1 runtime source + cels-ncurses reference architecture)

## Executive Summary

cels-clay bridges Clay's immediate-mode layout engine into CELS's reactive declarative framework. The fundamental tension: Clay rebuilds its entire element tree every frame via `Clay_BeginLayout` / `Clay_EndLayout`, while CELS compositions only re-execute when observed state changes. The architecture resolves this by making CELS compositions emit CLAY() calls as part of a single coordinated layout pass that runs every frame during the OnRender phase, while CELS reactivity controls *which* compositions participate in the rebuild.

## Recommended Architecture

```
                          CELS Frame Pipeline
                          ==================

  OnLoad        PostLoad         OnUpdate       PreStore        OnStore         OnRender
  ------        --------         --------       --------        -------         --------
  Lifecycle     Recomposition    App Systems    Clay Layout     (unused)        Clay Render
  Input Read    (dirty queue)    (input logic)  Pass                            Provider
                                                |
                                                v
                                     Clay_BeginLayout()
                                     Walk entity tree
                                       -> composition emit CLAY() calls
                                     Clay_EndLayout()
                                       -> Clay_RenderCommandArray
                                                |
                                                v
                                     ClayRenderable provider
                                       -> dispatch to renderer backend
```

### The Two-System Design

The module registers two ECS systems, not one:

1. **Clay_LayoutSystem** (PreStore phase): Calls `Clay_BeginLayout`, walks the CELS entity tree emitting CLAY() calls, calls `Clay_EndLayout`. Produces `Clay_RenderCommandArray`. This runs every frame because Clay requires a full rebuild.

2. **Clay_RenderSystem** (OnRender phase): The Feature/Provider system. Takes the `Clay_RenderCommandArray` and dispatches it to whatever renderer backend is registered (ncurses, SDL, etc.). This is where the `ClayRenderable` Feature/Provider contract lives.

**Why two systems, not one?** Separation of layout computation from rendering enables:
- Multiple renderer backends without changing layout logic
- Dirty-checking at the render level (skip render if command array unchanged)
- Clear phase ordering: layout completes before any rendering begins

## Component Boundaries

| Component | Responsibility | Communicates With |
|-----------|---------------|-------------------|
| **Clay_Engine** (module facade) | Single entry point, bundles Layout + Render systems, owns Clay arena | CELS core (module registration), Clay library |
| **Clay_Layout** (layout system) | Clay_BeginLayout/EndLayout, entity tree walking, CLAY() emission | CELS core (entity queries, composition data), Clay library |
| **Clay_Render** (render provider) | Feature/Provider registration, render command dispatch | CELS core (Feature/Provider API), renderer backends |
| **Clay_Arena** (arena manager) | Clay_Arena lifecycle: create, resize on window change, destroy | Clay library, window state |
| **Clay_TextMeasure** (text callback) | Clay_SetMeasureTextFunction bridge | Clay library, renderer backend (font system) |

## Frame Pipeline: Detailed Timing

### Phase 1: OnLoad -- Lifecycle + Input (CELS core, unchanged)

```
CELS_LifecycleSystem:
  - Evaluates lifecycle conditions (MainMenuVisible, etc.)
  - Enables/disables entities based on visibility
  - Deletes entities when destroy conditions met

TUI_InputSystem (or other backend input):
  - Reads platform input
  - Populates CELS_Input struct
```

No Clay involvement here. This is standard CELS.

### Phase 2: PostLoad -- Recomposition (CELS core, unchanged)

```
CELS_RecompositionSystem:
  - Processes dirty_queue (compositions whose observed state changed)
  - For each dirty composition:
    1. Delete old child entities
    2. Re-execute factory function with stored props
    3. New entities created with updated component data
  - Compositions with CEL_Watch(SomeState) re-run here
```

Critical interaction: When a composition re-runs during recomposition, it emits `CEL_Has(ClayUI, ...)` to mark entities as Clay-backed. But it does NOT emit CLAY() calls yet -- those happen in the layout phase.

### Phase 3: OnUpdate -- Application Systems (app code, unchanged)

```
MainMenuInputSystem, SettingsInputSystem, etc.:
  - Read CELS_Input
  - Modify state via CEL_Update(SomeState)
  - State changes trigger cels_state_notify_change()
  - Compositions are queued for recomposition NEXT frame
```

### Phase 4: PreStore -- Clay Layout Pass (NEW, cels-clay)

```
Clay_LayoutSystem:
  1. Clay_SetLayoutDimensions(window_width, window_height)
  2. Clay_BeginLayout()
  3. Walk entity tree in depth-first order:
     For each entity with ClayUI component:
       a. Look up the entity's composition factory + stored ClayUI data
       b. Call the entity's clay_layout_fn(props, entity_id)
          - This function emits CLAY() macro calls
          - Children are walked recursively inside CLAY() blocks
  4. commands = Clay_EndLayout()
  5. Store commands pointer for render system
```

**Why PreStore?** It runs after OnUpdate (state is settled) but before OnStore/OnRender (rendering needs the layout). PreStore is the correct slot for "prepare data that renderers will consume."

### Phase 5: OnRender -- Clay Render Provider (NEW, cels-clay)

```
Clay_RenderSystem (via Feature/Provider):
  1. Retrieve Clay_RenderCommandArray from layout system
  2. Iterate commands:
     For each Clay_RenderCommand:
       - Switch on commandType
       - Call renderer backend callback with command data
  3. Renderer backend does actual drawing
```

This is where the Feature/Provider pattern applies. The cels-clay module defines `CEL_DefineFeature(ClayRenderable)` and registers the layout-to-render bridge. Renderer backends (cels-ncurses, SDL, etc.) provide the actual rendering implementation.

## Entity-Backed Clay Blocks

### The ClayUI Component

```c
CEL_Define(ClayUI, {
    void (*layout_fn)(void* props, cels_entity_t entity_id);
    // The function that emits CLAY() calls for this entity's UI subtree
});
```

Every composition that has UI defines a `layout_fn` alongside its normal CELS composition body. The composition body creates ECS entities with components. The `layout_fn` emits Clay element declarations.

### Composition Pattern: CELS + Clay Hybrid

```c
// The composition creates entities (ECS side)
CEL_Composition(Sidebar, int width;) {
    CEL_Has(ClayUI, .layout_fn = Sidebar_layout);
    // Children are also compositions with ClayUI
    CEL_Call(NavButton, .label = "Home") {}
    CEL_Call(NavButton, .label = "Settings") {}
}

// The layout function emits Clay calls (layout side)
static void Sidebar_layout(void* props, cels_entity_t entity_id) {
    CLAY(CLAY_ID("Sidebar"), {
        .layout = { .sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_GROW() } }
    }) {
        // Walk children and call their layout_fns
        clay_emit_children(entity_id);
    }
}
```

### Entity Tree Walking Order

The layout system walks the ECS entity hierarchy in **parent-before-child** (depth-first preorder) to match Clay's nesting requirement. CLAY() blocks must be opened before children are declared inside them.

**Algorithm:**

```
function emit_clay_tree(entity_id):
    clay_ui = get_component(entity_id, ClayUI)
    if clay_ui is NULL:
        return

    // Entity's layout function opens CLAY() block and calls clay_emit_children
    clay_ui.layout_fn(get_props(entity_id), entity_id)

function clay_emit_children(parent_entity_id):
    children = get_children(parent_entity_id)  // flecs parent-child query
    sort children by creation order (or explicit order component)
    for each child in children:
        if child has ClayUI component:
            emit_clay_tree(child)
```

**Why flecs parent-child?** CELS compositions already create parent-child entity relationships via `cels_begin_entity` / `cels_end_entity`. The entity tree IS the UI tree. No separate tree structure needed.

**Lifecycle filtering:** Disabled entities (from CELS lifecycle system) are automatically excluded from flecs queries. A suspended lifecycle's entities will not appear in the tree walk, so their CLAY() calls are never emitted. This is how CELS reactivity controls Clay layout participation.

### Root Entity

The root composition creates a root entity with `ClayUI` whose `layout_fn` is the entry point for the entire Clay tree. The layout system finds all root-level `ClayUI` entities (those without a parent ClayUI entity) and starts the walk from there.

## Integration with CELS Reactive Recomposition

### The Key Insight

CELS reactivity and Clay's immediate-mode rebuild are **complementary, not conflicting**:

- **Clay rebuilds every frame** -- it must, that is how immediate-mode layout works
- **CELS compositions only re-run when state changes** -- but the layout functions read the *current* component data, which reflects whatever recomposition produced
- **The flow:** State change -> recomposition (PostLoad) -> entities updated -> layout pass reads updated entities (PreStore) -> Clay gets correct tree

### What "Reactive" Means in This Context

When `CEL_Update(MenuState)` triggers a recomposition:

1. **PostLoad:** `MenuRouter` composition re-runs. It conditionally calls different child compositions based on `MenuState.screen`. Old children are deleted, new children are created with `ClayUI` components.

2. **PreStore:** Layout system walks entity tree. The new children have different `ClayUI.layout_fn` callbacks. Clay sees a different element tree. Layout computes new positions/sizes.

3. **OnRender:** Renderer draws the new layout.

### Compositions That Watch State

Compositions that use `CEL_Watch(SomeState)` get re-run when that state changes. During re-run:
- Old child entities are deleted (their `ClayUI` layout functions go away)
- New child entities are created (possibly different children, same children with different props, or different arrangement)
- Next frame's layout pass sees the updated entity tree

### No Double Layout

There is exactly one `Clay_BeginLayout`/`Clay_EndLayout` pair per frame. All compositions contribute to this single pass. CELS recomposition happens BEFORE the layout pass (PostLoad before PreStore), so by the time layout runs, all entities have their final state for this frame.

## The Rendering Bridge: ClayRenderable Feature/Provider

### Feature Definition

```c
// In cels-clay module:
CEL_DefineFeature(ClayRenderable, .phase = CELS_Phase_OnRender);
```

### Provider Contract

Renderer backends implement the `ClayRenderable` provider:

```c
// In cels-ncurses (or cels-sdl, etc.):
CEL_Feature(ClayUI, ClayRenderable);
CEL_Provides(TUI, ClayRenderable, ClayUI, tui_clay_render_callback);
```

### Data Flow Through the Callback

The render callback receives the `Clay_RenderCommandArray` via a module-global pointer:

```c
// Set by Clay_LayoutSystem after Clay_EndLayout():
static Clay_RenderCommandArray g_clay_commands = {0};

// Read by render provider callback:
static void tui_clay_render_callback(CELS_Iter* it) {
    for (int i = 0; i < g_clay_commands.length; i++) {
        Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&g_clay_commands, i);
        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                // Draw rectangle using backend primitives
                break;
            case CLAY_RENDER_COMMAND_TYPE_TEXT:
                // Draw text using backend primitives
                break;
            case CLAY_RENDER_COMMAND_TYPE_BORDER:
                // Draw border using backend primitives
                break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                // Push clip region
                break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                // Pop clip region
                break;
            // ...
        }
    }
}
```

### Text Measurement

Clay requires a `Clay_SetMeasureTextFunction` callback to compute text dimensions. This is renderer-dependent (ncurses measures in character cells, SDL in pixels). The renderer backend module provides this callback during initialization.

**cels-clay provides a hook point:**

```c
typedef Clay_Dimensions (*Clay_MeasureTextFn)(
    Clay_StringSlice text,
    Clay_TextElementConfig* config,
    void* userData
);

void Clay_Engine_set_measure_text(Clay_MeasureTextFn fn, void* userData);
```

The renderer backend calls this during its init. The cels-clay module forwards it to `Clay_SetMeasureTextFunction`.

## Clay Arena Lifecycle

### Who Owns the Arena

The **Clay_Engine module** (cels-clay) owns the Clay_Arena. It is NOT owned by the renderer backend or the application.

### Lifecycle

```
Clay_Engine_use() called in CEL_Build:
  1. Calculate arena size: Clay_MinMemorySize()
  2. Allocate memory: malloc(arena_size)
  3. Create arena: Clay_CreateArenaWithCapacityAndMemory(size, memory)
  4. Initialize Clay: Clay_Initialize(arena, dimensions, error_handler)
  5. Set initial layout dimensions from window config

Window resize (detected via WindowState observation):
  6. Clay_SetLayoutDimensions(new_width, new_height)
  (Arena is NOT reallocated -- Clay handles this internally)

Shutdown:
  7. free(arena.memory)
  (No Clay_Destroy -- Clay has no teardown function in v0.14)
```

### Relation to CELS Lifecycle

The Clay arena lives for the entire application lifetime, created during `CEL_Build` and freed at process exit. It is NOT tied to any CELS lifecycle -- it persists through all screen transitions, recompositions, and state changes. The arena is Clay's internal working memory, not a per-frame allocation.

### Arena Sizing

Clay_MinMemorySize() returns the minimum required arena size based on the current max element count and text cache settings. Default is ~1MB. For larger UIs:

```c
Clay_SetMaxElementCount(8192);       // Default: 8192
Clay_SetMaxMeasureTextCacheWordCount(16384);  // Default: 16384
uint32_t size = Clay_MinMemorySize();
```

These must be called BEFORE Clay_MinMemorySize() / Clay_Initialize().

## Module Structure

### File Layout

```
cels-clay/
  CMakeLists.txt                    # INTERFACE library definition
  include/
    cels-clay/
      clay_engine.h                 # Module facade: Clay_Engine_use(), Clay_EngineConfig
      clay_layout.h                 # Layout system: ClayUI component, tree walking API
      clay_render.h                 # Render provider: ClayRenderable feature, provider hooks
  src/
    clay_engine.c                   # CEL_DefineModule(Clay_Engine), arena lifecycle
    clay_layout.c                   # Layout system: BeginLayout, tree walk, EndLayout
    clay_render.c                   # Feature/Provider registration, render dispatch
```

### Headers vs Sources

**Headers (public API surface):**
- `clay_engine.h`: `Clay_EngineConfig`, `Clay_EngineContext`, `Clay_Engine_use()`, `Clay_Engine_init()`
- `clay_layout.h`: `ClayUI` component definition (CEL_Define), `clay_emit_children()` helper, layout system registration
- `clay_render.h`: `ClayRenderable` feature declaration, `Clay_Engine_set_measure_text()` hook, render command accessor

**Sources (implementation):**
- `clay_engine.c`: Module init, arena allocation, Clay_Initialize, window resize handling
- `clay_layout.c`: Layout system registered at PreStore, entity tree walking, CLAY() emission coordination
- `clay_render.c`: Feature/Provider definitions, render system callback, command array forwarding

### Public API Surface

```c
// --- clay_engine.h ---

// Configuration for Clay_Engine_use()
typedef struct Clay_EngineConfig {
    int max_element_count;      // 0 = default (8192)
    int max_text_cache_words;   // 0 = default (16384)
} Clay_EngineConfig;

// Context passed to callbacks
typedef struct Clay_EngineContext {
    cels_entity_t clay_state_id;  // Observable: Clay_State (layout dimensions, error count)
} Clay_EngineContext;

// Module entry point
extern void Clay_Engine_use(Clay_EngineConfig config);
extern void Clay_Engine_init(void);
extern cels_entity_t Clay_Engine;

// --- clay_layout.h ---

// The ClayUI ECS component (defined via CEL_Define in the header)
// layout_fn: called during the layout pass to emit CLAY() calls
// Compositions attach this to mark themselves as Clay-backed
CEL_Define(ClayUI, {
    void (*layout_fn)(void* props, cels_entity_t entity_id);
});

// Helper: emit Clay layout calls for all ClayUI children of an entity
extern void clay_emit_children(cels_entity_t parent_entity_id);

// Layout system initialization (called by Clay_Engine module)
extern void clay_layout_init(void);

// --- clay_render.h ---

// Text measurement hook (called by renderer backends)
extern void clay_render_set_measure_text(
    Clay_Dimensions (*fn)(Clay_StringSlice, Clay_TextElementConfig*, void*),
    void* userData
);

// Access render commands (called by renderer provider callbacks)
extern Clay_RenderCommandArray clay_render_get_commands(void);

// Render system initialization (called by Clay_Engine module)
extern void clay_render_init(void);
```

## Integration Pattern: How cels-clay Fits with cels-ncurses

The cels-clay module does NOT replace cels-ncurses. They compose together:

```
Application code:
  CEL_Build(App) {
      // Use TUI_Engine for window/input/frame loop (from cels-ncurses)
      TUI_Engine_use((TUI_EngineConfig){
          .title = "My App",
          .fps = 60,
          .root = AppUI
      });

      // Use Clay_Engine for layout computation (from cels-clay)
      Clay_Engine_use((Clay_EngineConfig){
          .max_element_count = 4096
      });

      // The renderer backend bridges Clay commands to ncurses drawing
      // (could be in cels-ncurses as an additional provider,
      //  or in a separate cels-ncurses-clay bridge module)
  }
```

The window provider (cels-ncurses) owns the frame loop. The layout system (cels-clay) runs inside `Engine_Progress` at PreStore. The render provider runs at OnRender. The renderer backend (TUI Clay provider) maps Clay_RenderCommands to ncurses draw calls.

## Patterns to Follow

### Pattern 1: INTERFACE Library

Same as cels-ncurses -- all source files compile in the consumer's translation unit. No separate .so/.a.

```cmake
add_library(cels-clay INTERFACE)
target_sources(cels-clay INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_layout.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_render.c
)
target_include_directories(cels-clay INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(cels-clay INTERFACE cels)
# Clay is a single-header library -- consumer includes it directly
```

### Pattern 2: Module Facade

```c
CEL_DefineModule(Clay_Engine) {
    // Allocate and initialize Clay arena
    clay_arena_init(g_clay_config);

    // Register layout system (PreStore phase)
    clay_layout_init();

    // Register render feature/provider framework
    clay_render_init();
}
```

### Pattern 3: Composition + Layout Function Pairing

Every composition that participates in Clay layout has two functions:
1. **Composition body** (CEL_Composition): Creates ECS entities, attaches components
2. **Layout function** (static): Emits CLAY() calls during layout pass

The composition body sets `ClayUI.layout_fn` to point at the layout function. This pairing is the fundamental integration contract.

## Anti-Patterns to Avoid

### Anti-Pattern 1: CLAY() Inside Composition Body

**What:** Calling CLAY() macros directly inside a `CEL_Composition()` body.
**Why bad:** Compositions run during recomposition (PostLoad), but Clay_BeginLayout has not been called yet. CLAY() calls outside BeginLayout/EndLayout are undefined behavior.
**Instead:** Separate the layout function. The composition body only attaches the `ClayUI` component with a function pointer.

### Anti-Pattern 2: Storing Clay IDs in Components

**What:** Saving `Clay_ElementId` in ECS components and trying to reuse them across frames.
**Why bad:** Clay rebuilds the entire tree every frame. Element IDs are recomputed from strings each frame. Stored IDs from a previous frame are meaningless.
**Instead:** Use CLAY_ID("literal") or CLAY_IDI("literal", index) fresh each frame in layout functions.

### Anti-Pattern 3: Multiple Clay_BeginLayout/EndLayout Per Frame

**What:** Each composition calling its own BeginLayout/EndLayout.
**Why bad:** Clay has ONE global tree per frame. Multiple Begin/End pairs would create multiple independent layouts, not a single composed tree.
**Instead:** ONE BeginLayout at the start of the layout system, ONE EndLayout at the end. All compositions contribute to the same tree.

### Anti-Pattern 4: Conditional CLAY() Calls Based on Runtime State in Layout Functions

**What (subtle):** Reading CELS state directly in layout functions to decide whether to emit CLAY() blocks.
**Why tricky:** This is actually fine and expected -- layout functions SHOULD branch on state. The anti-pattern is reading state that the composition does NOT observe via CEL_Watch. If state changes but the composition does not recompose, the layout function will still be called with stale component data.
**Instead:** Ensure any state that affects layout is either:
- Passed through ClayUI component data (updated by recomposition), or
- Read from ECS components that are updated by systems (outside recomposition)

## Scalability Considerations

| Concern | 100 elements | 1K elements | 10K elements |
|---------|--------------|-------------|--------------|
| Arena size | ~1MB (default) | ~2MB | ~8MB (bump max_element_count) |
| Layout computation | <1ms | ~2ms | ~5-10ms |
| Entity tree walk | Negligible | ~0.5ms | ~2ms (optimize with cached child lists) |
| Text measurement cache | Default 16K words | Sufficient | May need bump |

## Build Order (Dependencies Between Components)

### Phase 1: Clay_Layout (foundation)

- `ClayUI` component definition
- Entity tree walking (`clay_emit_children`)
- Layout system registration (PreStore phase)
- No renderer dependency -- can test with Clay debug view
- **Depends on:** CELS core only

### Phase 2: Clay_Render (rendering bridge)

- `ClayRenderable` feature definition
- Render command accessor API
- Provider registration framework
- **Depends on:** Phase 1 (layout produces commands)

### Phase 3: Clay_Engine (module facade)

- Arena lifecycle management
- Module bundling (Layout + Render)
- Window state observation for resize
- Text measurement hook point
- **Depends on:** Phases 1 + 2

### Phase 4: Backend Integration (separate module or extension)

- Concrete renderer implementations (ncurses, SDL, etc.)
- Text measurement function implementations
- **Depends on:** Phase 3 + specific backend module

## Data Flow Summary

```
State change (CEL_Update)
  |
  v
cels_state_notify_change() -- marks compositions dirty
  |
  v
[Next frame]
  |
  v
OnLoad: Lifecycle evaluation (enable/disable entities)
  |
  v
PostLoad: Recomposition (re-run dirty compositions, update entity tree)
  |
  v
OnUpdate: App systems (read input, modify state for next frame)
  |
  v
PreStore: Clay_LayoutSystem
  |-- Clay_SetLayoutDimensions(width, height)
  |-- Clay_BeginLayout()
  |-- Walk entity tree (depth-first, parent-before-child)
  |     |-- For each entity with ClayUI:
  |           |-- Call layout_fn(props, entity_id)
  |           |     |-- Emits CLAY() macro calls
  |           |     |-- Inside CLAY() block: clay_emit_children(entity_id)
  |           |           |-- Recursively walk children
  |-- Clay_EndLayout() -> Clay_RenderCommandArray
  |-- Store commands for render system
  |
  v
OnRender: Clay_RenderSystem (Feature/Provider)
  |-- Retrieve Clay_RenderCommandArray
  |-- Provider callback iterates commands
  |     |-- RECTANGLE -> backend draw rect
  |     |-- TEXT -> backend draw text
  |     |-- BORDER -> backend draw border
  |     |-- SCISSOR_START -> backend push clip
  |     |-- SCISSOR_END -> backend pop clip
  |     |-- CUSTOM -> backend custom handler
  |
  v
[Frame loop: doupdate/present/swap buffers]
```

## Sources

- Clay v0.14 header: `/home/cachy/workspaces/libs/clay/clay.h` (direct analysis)
- Clay SDL3 example: `/home/cachy/workspaces/libs/clay/examples/SDL3-simple-demo/main.c` (integration pattern)
- CELS runtime: `/home/cachy/workspaces/libs/cels/src/cels.cpp` (recomposition system, phase mapping)
- CELS API: `/home/cachy/workspaces/libs/cels/include/cels/cels.h` (macro definitions, Feature/Provider API)
- cels-ncurses architecture: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/.planning/codebase/ARCHITECTURE.md`
- cels-ncurses structure: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/.planning/codebase/STRUCTURE.md`
- cels-ncurses integrations: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/.planning/codebase/INTEGRATIONS.md`
- cels-ncurses research architecture: `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/.planning/research/ARCHITECTURE.md`
- App example: `/home/cachy/workspaces/libs/cels/examples/app.c` (composition/state/lifecycle usage)

---
*Architecture research: 2026-02-07*
