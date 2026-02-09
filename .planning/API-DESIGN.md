# cels-clay API Design

**Agreed:** 2026-02-07
**Status:** Canonical reference for all phases

## Core Principle

Clay layout is a **CELS component** on entities. Compositions declare structure and wire data. A single module-level layout system walks the entity tree each frame, calling layout functions. CELS reactivity drives data updates; Clay handles spatial layout.

## Architecture

```
Composition (reactive)         ECS Entity              Layout Function (per-frame)
──────────────────────         ──────────              ───────────────────────────
CEL_Has(ClayUI, .layout=fn)  → [ClayUI]             → fn(world, entity)
CEL_Has(Counter, .value=5)   → [Counter: 5]         → Clay_Get(Counter) → "Count: 5"
CEL_Has(Selectable, .idx=0)  → [Selectable: 0]      → Clay_Get(Selectable) → highlight

state changes → recomp        [Counter: 6]           → Clay_Get(Counter) → "Count: 6"
```

### Execution Timeline (per frame)

| Phase | What runs | Role |
|-------|-----------|------|
| OnUpdate | Input systems (`CEL_Use`) | Read keyboard, mutate state via `CEL_Update` |
| (reactive) | Compositions re-run | Update component data on entities |
| PreStore | ClayLayoutSystem (module) | `BeginLayout` → walk entity tree → `EndLayout` |
| OnRender | Render provider | Read `Clay_RenderCommandArray` → draw to terminal |

### Entity Tree → Clay Nesting

The layout system walks CELS entity hierarchy depth-first. Parent layout opens a `CLAY()` scope, `CEL_Clay_Children()` recurses into children, parent scope closes. Entity order = Clay nesting order.

```
Entity: App            →  CLAY("App") {
  Entity: Sidebar      →    CLAY("Sidebar") {
    Entity: Button0    →      CLAY("Button") { CLAY_TEXT("Home") }
    Entity: Button1    →      CLAY("Button") { CLAY_TEXT("Settings") }
  }                    →    }
  Entity: Content      →    CLAY("Content") {
    Entity: Label      →      CLAY_TEXT("Welcome")
  }                    →    }
}                      →  }
```

## API Surface

### Components

```c
// Layout attachment — function pointer called per-frame by layout system
CEL_Define(ClayUI, { void (*layout)(ecs_world_t*, ecs_entity_t); });
```

Entities with `ClayUI` participate in the Clay layout tree. The layout system queries these and calls `.layout` during the tree walk.

### Layout Functions

```c
CEL_Clay_Layout(name) {
    // Expands to: void name(ecs_world_t* world, ecs_entity_t self)
    // Available helpers:
    //   Clay_Get(Component)       → ecs_get(world, self, ComponentID)
    //   CEL_Clay_Children()       → recurse into child entities
    //   CLAY_DYN_STRING(buf)      → frame-arena copy for dynamic text
}
```

Layout functions read component data from the entity and emit `CLAY()` calls. They run every frame during the layout pass — no state, no side effects, pure layout declaration.

### Compositions

Standard CELS compositions. They set up component data on entities:

```c
CEL_Composition(Button, const char* label; int index; void (*on_click)(void);) {
    CEL_Has(ClayUI,       .layout = button_layout);
    CEL_Has(Label,        .text = props.label);
    CEL_Has(Selectable,   .index = props.index);
    CEL_Has(ButtonAction, .on_click = props.on_click);
}
```

When observed state changes, the composition re-runs and component data updates. Next layout frame picks up the new data automatically.

### Input & Interaction

Input systems run at `OnUpdate` phase, query interactive entities, and mutate state:

```c
CEL_System(ButtonInputSystem, .phase = CELS_Phase_OnUpdate) {
    // Query entities with Selectable + ButtonAction
    // Check keyboard input
    // Call on_click() which does CEL_Update(State) { ... }
}
```

Registered per-composition via `CEL_Use(ButtonInputSystem)` — inherits lifecycle.

### Module System (layout coordinator)

One system registered by `Clay_Engine_use()`. Not per-entity:

```c
// Registered by the module — walks ALL ClayUI entities
CEL_System(ClayLayoutSystem, .phase = CELS_Phase_PreStore) {
    Clay_BeginLayout();
    clay_walk_entity_tree(root_entity);  // depth-first, calls .layout
    Clay_EndLayout();
}
```

## Interaction Flow

```
User presses Enter
  → ButtonInputSystem (OnUpdate) queries Selectable entities
  → Finds entity where sel->index == AppState.selected
  → Calls btn->on_click() → action_increment()
  → CEL_Update(AppState) { AppState.count++ }
  → CounterDisplay watches AppState → recomposition
  → CEL_Has(Counter, .value = new_count) → component updated
  → Next frame: layout reads Counter → formats "Count: 1"
  → Clay computes layout → renderer draws updated text
```

## Complete Example: Counter App

```c
// ── State ──────────────────────────────────────────────

CEL_State(AppState, {
    int count;
    int selected;
});

// ── Components ─────────────────────────────────────────

CEL_Define(ClayUI,       { void (*layout)(ecs_world_t*, ecs_entity_t); });
CEL_Define(Counter,      { int value; });
CEL_Define(Label,        { const char* text; });
CEL_Define(Selectable,   { int index; });
CEL_Define(ButtonAction, { void (*on_click)(void); });

// ── Actions ────────────────────────────────────────────

void action_increment(void) {
    CEL_Update(AppState) {
        AppState.count++;
    }
}

// ── Input System ───────────────────────────────────────

CEL_System(ButtonInputSystem, .phase = CELS_Phase_OnUpdate) {
    CELS_Context* ctx = cels_get_context();
    const CELS_Input* input = cels_input_get(ctx);
    ecs_world_t* world = cels_get_world(ctx);

    ecs_iter_t it = ecs_each_id(world, SelectableID);
    while (ecs_each_next(&it)) {
        Selectable* sel = ecs_field(&it, Selectable, 0);
        for (int i = 0; i < it.count; i++) {
            if (sel[i].index == AppState.selected) {
                if (input->key == KEY_ENTER) {
                    ButtonAction* btn = ecs_get(world, it.entities[i], ButtonActionID);
                    if (btn && btn->on_click) btn->on_click();
                }
            }
        }
    }
}

// ── Layout Functions ───────────────────────────────────

CEL_Clay_Layout(root_layout) {
    CLAY(CLAY_ID("App"),
         CLAY_LAYOUT({ .layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .padding = CLAY_PADDING_ALL(2) })) {
        CEL_Clay_Children();
    }
}

CEL_Clay_Layout(counter_display_layout) {
    Counter* c = Clay_Get(Counter);
    char buf[32];
    snprintf(buf, sizeof(buf), "Count: %d", c->value);

    CLAY(CLAY_LAYOUT({ .padding = CLAY_PADDING_ALL(1) })) {
        CLAY_TEXT(CLAY_DYN_STRING(buf), CLAY_TEXT_CONFIG({
            .textColor = {255, 255, 255, 255}
        }));
    }
}

CEL_Clay_Layout(button_layout) {
    Label* label = Clay_Get(Label);
    Selectable* sel = Clay_Get(Selectable);
    bool is_selected = (sel->index == AppState.selected);

    CLAY(CLAY_LAYOUT({ .padding = CLAY_PADDING_ALL(1) }),
         is_selected
           ? CLAY_RECTANGLE({ .color = {60, 60, 100, 255} })
           : CLAY_RECTANGLE({ .color = {30, 30, 30, 255} })) {
        CLAY_TEXT(CLAY_STRING(label->text), CLAY_TEXT_CONFIG({
            .textColor = {255, 255, 255, 255}
        }));
    }
}

// ── Compositions ───────────────────────────────────────

CEL_Composition(CounterDisplay) {
    AppState_t* state = CEL_Watch(AppState);
    CEL_Has(ClayUI,  .layout = counter_display_layout);
    CEL_Has(Counter, .value = state->count);
}

#define Button(...) CEL_Init(Button, __VA_ARGS__)
CEL_Composition(Button, const char* label; int index; void (*on_click)(void);) {
    CEL_Has(ClayUI,       .layout = button_layout);
    CEL_Has(Label,        .text = props.label);
    CEL_Has(Selectable,   .index = props.index);
    CEL_Has(ButtonAction, .on_click = props.on_click);
}

// ── Root ───────────────────────────────────────────────

CEL_Root(App) {
    CEL_Has(ClayUI, .layout = root_layout);
    CEL_Use(ButtonInputSystem);

    CounterDisplay() {}
    Button(.label = "+ Increment", .index = 0, .on_click = action_increment) {}
}
```

## Design Notes

### Why component + function pointer (not inline blocks)

C99 has no closures. `CEL_Has(CLAY) { ... }` would be an orphaned block after the macro expands — the block isn't captured. Separating layout functions from compositions is the only clean approach in standard C.

### Why one layout system (not per-entity systems)

Clay requires nested `CLAY()` calls in tree order. A parent's scope must **open**, children emit inside it, then the parent scope **closes**. Flat system iteration can't produce this nesting. One coordinating system walks the tree depth-first to maintain correct order.

### Dynamic strings (CLAY_DYN_STRING)

Clay stores `Clay_String` as pointer + length (a view, not a copy). Stack-local buffers in layout functions die before the renderer reads them. `CLAY_DYN_STRING` copies into a per-frame arena that survives through the render phase. This is requirement API-07.

### Reactivity bridge

CELS compositions are reactive (re-run on state change). Clay is immediate-mode (rebuild every frame). The bridge: compositions update **component data** reactively, layout functions **read** that data every frame. No special synchronization needed — ECS is the shared state.

---
*Agreed: 2026-02-07*
*Reference this document from phase plans and context files*
