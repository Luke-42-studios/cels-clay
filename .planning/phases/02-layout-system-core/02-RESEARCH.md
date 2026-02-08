# Phase 2: Layout System Core - Research

**Researched:** 2026-02-08
**Domain:** Clay layout engine integration with CELS ECS composition system
**Confidence:** HIGH

## Summary

This research covers the exact internals needed to implement CEL_Clay() as an inline composition macro that wraps Clay's CLAY() macro, plus the ClaySurface built-in composition that owns the BeginLayout/EndLayout boundary, the entity tree walk system, per-frame arena management for dynamic strings, and auto-generated Clay IDs.

The critical insight is that Clay's CLAY() macro is a `for` loop that calls `Clay__OpenElementWithId()` + `Clay__ConfigureOpenElement()` in the initializer and `Clay__CloseElement()` in the increment. CEL_Clay() must wrap this pattern while injecting auto-generated IDs and intercepting dynamic strings. The ClaySurface composition runs at a specific pipeline phase and calls `Clay_BeginLayout()` before its children contribute CLAY() nodes, then `Clay_EndLayout()` after.

The entity tree walk uses flecs' `ecs_children()` iterator to walk parent-child relationships in declaration order. Each entity with a ClayUI component contributes its layout function to the Clay tree. Non-Clay entities are transparent pass-throughs whose children still participate.

**Primary recommendation:** CEL_Clay() should expand to a CLAY() macro call with an auto-generated `CLAY_SID()` ID derived from the entity name + a counter. ClaySurface should be a built-in CEL_Composition that registers a PreStore system to drive the layout pass. Dynamic strings should use a simple frame arena (malloc + bump pointer, reset each frame).

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Clay | v0.14 | Layout engine | Already fetched via FetchContent in Phase 1 |
| CELS | v0.2 (v2 branch) | ECS framework | Host framework -- all patterns use CELS macros |
| flecs | (bundled with CELS) | ECS runtime | Provides entity hierarchy, ecs_children() iterator |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| stdlib.h | C99 | malloc/free for frame arena | Dynamic string arena allocation |
| string.h | C99 | memcpy for arena string copies | Copying dynamic strings into arena |
| stdio.h | C99 | fprintf(stderr, ...) | Error/warning messages |

### No Additional Dependencies
Phase 2 uses only Clay (already integrated) and CELS APIs. No new libraries needed.

## Architecture Patterns

### Recommended File Structure
```
include/cels-clay/
    clay_engine.h          # Existing: Clay_Engine_use(), Clay_EngineConfig
    clay_layout.h          # NEW: CEL_Clay(), CEL_Clay_Children(), ClaySurface, ClayUI
src/
    clay_impl.c            # Existing: CLAY_IMPLEMENTATION translation unit
    clay_engine.c          # Existing: Clay_Engine module init
    clay_layout.c          # NEW: ClaySurface composition, layout system, tree walk, frame arena
```

### Pattern 1: Clay CLAY() Macro Internals

**What:** The CLAY() macro is a single-iteration `for` loop that wraps Clay__OpenElementWithId / Clay__ConfigureOpenElement / Clay__CloseElement. Understanding its exact expansion is critical for CEL_Clay().

**Source:** `/home/cachy/workspaces/libs/clay/clay.h` lines 143-148

```c
// CLAY(id, ...) expands to:
for (
    CLAY__ELEMENT_DEFINITION_LATCH = (
        Clay__OpenElementWithId(id),
        Clay__ConfigureOpenElement(
            (Clay__Clay_ElementDeclarationWrapper){ __VA_ARGS__ }).wrapped),
        0);
    CLAY__ELEMENT_DEFINITION_LATCH < 1;
    CLAY__ELEMENT_DEFINITION_LATCH = 1, Clay__CloseElement()
)
```

**Key observations:**
- `CLAY__ELEMENT_DEFINITION_LATCH` is a `static uint8_t` declared at file scope (line 104). It is a single global latch variable, NOT per-instance. This is safe because CLAY() loops execute synchronously in a single-threaded context (one iteration each).
- The `id` parameter is a `Clay_ElementId` struct (from CLAY_ID, CLAY_SID, etc.)
- `Clay__OpenElementWithId` pushes onto an internal element stack. `Clay__CloseElement` pops it.
- Children declared inside the `for` body are automatically nested (they push/pop between the parent's open/close).
- `CLAY_TEXT(text, config)` calls `Clay__OpenTextElement()` which is a leaf node (no for-loop, no close needed).

**Confidence:** HIGH (verified from clay.h source)

### Pattern 2: CEL_Clay() Macro Design

**What:** CEL_Clay() wraps CLAY() with auto-ID generation, runs inline inside compositions.

**Design approach:**

```c
// CEL_Clay() should expand to something like:
#define CEL_Clay(...) \
    CLAY(CEL_CLAY__AUTO_ID(), __VA_ARGS__)

// Where CEL_CLAY__AUTO_ID() generates a unique Clay_ElementId
// from the entity name + a counter for uniqueness within the composition
```

**Critical constraint:** CEL_Clay() is used INSIDE compositions. At composition time, we have access to:
- `cels_get_current_entity()` - the current entity ID
- Entity name (via flecs API)
- `__COUNTER__` or `__LINE__` for per-call-site uniqueness

**ID generation strategy:**
- Use `CLAY_SID()` with a dynamic Clay_String constructed from entity context
- For uniqueness across multiple CEL_Clay() blocks in one composition: combine entity ID with `__COUNTER__`
- Use `CLAY_SIDI()` (hash string + index) where index = `__COUNTER__` value

```c
// Recommended approach:
#define CEL_Clay(...) \
    _CEL_CLAY_IMPL(_CEL_CAT(_cel_clay_, __COUNTER__), __VA_ARGS__)

#define _CEL_CLAY_IMPL(uid, ...) \
    CLAY(_cel_clay_auto_id(__COUNTER__), __VA_ARGS__)
```

The `_cel_clay_auto_id()` function would:
1. Get the current entity ID (from cels_get_current_entity)
2. Construct a Clay_ElementId by hashing the entity ID + counter offset
3. Return the Clay_ElementId

**Confidence:** HIGH (derived from Clay macro analysis + CELS API study)

### Pattern 3: ClaySurface as Built-in Composition

**What:** ClaySurface is a CEL_Composition that owns the BeginLayout/EndLayout boundary.

**Key insight from Clay source (lines 4216-4266):**
- `Clay_BeginLayout()` resets ephemeral memory, opens a root container element
- Between Begin and End, all CLAY() calls build the layout tree
- `Clay_EndLayout()` closes the root element, runs `Clay__CalculateFinalLayout()`, returns render commands
- `Clay_SetLayoutDimensions()` must be called BEFORE `Clay_BeginLayout()` if dimensions changed

**ClaySurface implementation approach:**

ClaySurface is a composition that:
1. Stores width/height as props
2. On composition, records the entity ID as the "surface root"
3. A PreStore-phase system finds ClaySurface entities and runs the layout pass

```c
CEL_Composition(ClaySurface, float width; float height;) {
    CEL_Has(ClaySurfaceConfig, .width = props.width, .height = props.height);
    // Children are declared in the trailing block
}
```

The layout system (registered at PreStore):
```c
// Pseudocode
CEL_System(ClayLayoutSystem, .phase = CELS_Phase_PreStore) {
    // Find ClaySurface entities
    // For each surface:
    //   1. Clay_SetLayoutDimensions({config.width, config.height})
    //   2. Clay_BeginLayout()
    //   3. Walk child entity tree depth-first, calling CEL_Clay() blocks
    //   4. Clay_EndLayout() -> store render commands
}
```

**Problem:** The context decision says CEL_Clay() evaluates at composition time, not per-frame. But Clay_BeginLayout/EndLayout must bracket ALL CLAY() calls. This means compositions must either:
- (a) Store layout data and replay it during the layout pass, OR
- (b) Be re-evaluated during the layout pass

Option (a) -- the ClayUI component model from API-DESIGN.md (function pointer approach) -- was the original design but the context decision explicitly replaced it with inline CEL_Clay().

**Resolution:** The context decision says "CEL_Clay() evaluates at composition time -- CELS reactivity (CEL_Watch) keeps it fresh via recomposition on state change." This means CEL_Clay() MUST still produce CLAY() calls at composition time. But those calls must happen between BeginLayout/EndLayout.

**The only way this works:** ClaySurface's composition time IS the layout pass. The PreStore system triggers recomposition of the ClaySurface subtree, which re-evaluates all CEL_Clay() blocks. Alternatively, ClaySurface itself calls Begin/End around its children during composition, and the system ensures this happens at the right pipeline phase.

**Recommended approach:** CEL_Clay() produces real CLAY() calls inline. ClaySurface wraps its body with BeginLayout/EndLayout. The PreStore system calls Clay_SetLayoutDimensions and triggers the ClaySurface subtree to compose (which calls Begin, walks children with their CEL_Clay blocks, calls End).

Since CELS compositions run during initial build AND during recomposition, the ClaySurface composition body IS the layout pass. The system just needs to mark the surface dirty each frame so it recomposes, or use a different mechanism: the system itself walks the entity tree and invokes stored layout data.

**Hybrid approach (recommended):** CEL_Clay() at composition time stores a layout descriptor (function pointer or config struct) on the entity as a component. The PreStore system walks the tree and calls those layout functions between Begin/End. This is essentially the original ClayUI model but with CEL_Clay() as syntactic sugar that auto-generates the function pointer.

Wait -- re-reading the context decision more carefully: "CEL_Clay() opens a CLAY() scope inline within the composition body." This is unambiguous. CEL_Clay() literally calls CLAY() at composition time.

**Final resolution:** The composition must execute between Begin/End. ClaySurface calls `Clay_BeginLayout()` at the START of its composition body, and `Clay_EndLayout()` at cleanup. The PreStore system ensures ClaySurface recomposes every frame (or on state change). All child compositions with CEL_Clay() blocks execute their CLAY() calls during this recomposition window.

This requires that the ClaySurface composition forces recomposition of itself and its children every frame (since Clay rebuilds the tree every frame). The system at PreStore would:
1. Set layout dimensions
2. Trigger recomposition of the ClaySurface entity subtree
3. During recomposition, ClaySurface's body calls Begin/End, children execute CEL_Clay()

**Alternative (simpler):** ClaySurface does NOT use standard recomposition. Instead, it stores layout function pointers (like the original API-DESIGN.md) as an internal detail hidden behind CEL_Clay() macro sugar. CEL_Clay() generates a static function and stores it, the system calls it.

**Confidence:** MEDIUM (the exact mechanism needs careful design; the context decision is clear on the API surface but the implementation path has tradeoffs)

### Pattern 4: Entity Tree Walk

**What:** Depth-first walk of CELS entity hierarchy to produce Clay layout tree nesting.

**Source:** `/home/cachy/workspaces/libs/cels/src/cels.cpp` lines 804-810

```c
// Flecs provides ecs_children() to iterate children of a parent entity
ecs_iter_t it = ecs_children(world, parent);
while (ecs_children_next(&it)) {
    for (int i = 0; i < it.count; i++) {
        ecs_entity_t child = it.entities[i];
        // Process child...
    }
}
```

**Key observations:**
- `ecs_children()` returns children in the order they were added (which matches declaration order in compositions)
- Each child may or may not have a ClayUI-type component
- Non-Clay entities are "transparent" -- skip them but recurse into their children
- The walk must be recursive: parent opens CLAY scope -> children contribute -> parent closes

**Tree walk pseudocode:**
```c
void clay_walk_entity(ecs_world_t* world, ecs_entity_t entity) {
    // Check if entity has a layout component
    const ClayLayout* layout = ecs_get_id(world, entity, ClayLayoutID);

    if (layout && layout->layout_fn) {
        // Entity has layout -- call its function
        // The function opens CLAY() and may call CEL_Clay_Children()
        layout->layout_fn(world, entity);
    } else {
        // Transparent pass-through -- just walk children
        clay_walk_children(world, entity);
    }
}

void clay_walk_children(ecs_world_t* world, ecs_entity_t parent) {
    ecs_iter_t it = ecs_children(world, parent);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            clay_walk_entity(world, it.entities[i]);
        }
    }
}
```

**Confidence:** HIGH (ecs_children pattern verified from CELS source)

### Pattern 5: CEL_Clay_Children() Explicit Child Placement

**What:** Developer controls WHERE in the CLAY tree children appear.

**How it works:** Inside a CEL_Clay() block, the developer calls CEL_Clay_Children() to emit child entities at that point. This is how a sidebar composition can have a header, then children, then a footer.

```c
CEL_Composition(Sidebar) {
    CEL_Clay(.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM }) {
        CLAY_TEXT(CLAY_STRING("Header"), text_config);
        CEL_Clay_Children();  // Child entities render HERE
        CLAY_TEXT(CLAY_STRING("Footer"), text_config);
    }
}
```

**Implementation:** CEL_Clay_Children() needs to know the current entity and walk its children. This requires access to `(world, entity)` context. If using the layout function pointer approach, these are function parameters. If using inline composition approach, they need to be available via thread-local or global state.

**Recommended approach:** Store the "current layout entity" in module-level state during the tree walk. CEL_Clay_Children() reads this state and calls `clay_walk_children()`.

```c
// Module-level state during layout pass
static ecs_world_t* g_layout_world = NULL;
static ecs_entity_t g_layout_current_entity = 0;

#define CEL_Clay_Children() \
    clay_emit_children(g_layout_world, g_layout_current_entity)
```

**Confidence:** HIGH (straightforward pattern)

### Pattern 6: Clay_SetLayoutDimensions

**What:** Updates Clay's internal layout dimensions.

**Source:** `/home/cachy/workspaces/libs/clay/clay.h` line 859

```c
CLAY_DLL_EXPORT void Clay_SetLayoutDimensions(Clay_Dimensions dimensions);
```

**When to call:** Before `Clay_BeginLayout()`. The ClaySurface composition reads its width/height props and calls this.

**Implementation:**
```c
Clay_SetLayoutDimensions((Clay_Dimensions){ .width = config.width, .height = config.height });
```

**Confidence:** HIGH (direct from Clay API)

### Pattern 7: Clay_SetMeasureTextFunction

**What:** Binds a text measurement callback for CLAY_TEXT elements.

**Source:** `/home/cachy/workspaces/libs/clay/clay.h` line 896

```c
CLAY_DLL_EXPORT void Clay_SetMeasureTextFunction(
    Clay_Dimensions (*measureTextFunction)(
        Clay_StringSlice text,
        Clay_TextElementConfig *config,
        void *userData),
    void *userData);
```

**When to call:** After Clay_Initialize, before any layout pass that uses CLAY_TEXT.

**For terminal rendering:** Text measurement is simple -- each character is 1 unit wide, 1 unit tall (in cell units). The terminal renderer example in Clay's source (`renderers/terminal/clay_renderer_terminal_ansi.c` lines 45-73) uses a `columnWidth` multiplier:

```c
static inline Clay_Dimensions
Console_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    Clay_Dimensions textSize = {0};
    int columnWidth = *(int *)userData;
    float maxTextWidth = 0.0f;
    float lineTextWidth = 0;
    float textHeight = 1;
    for (int i = 0; i < text.length; ++i) {
        if (text.chars[i] == '\n') {
            maxTextWidth = maxTextWidth > lineTextWidth ? maxTextWidth : lineTextWidth;
            lineTextWidth = 0;
            textHeight++;
            continue;
        }
        lineTextWidth++;
    }
    maxTextWidth = maxTextWidth > lineTextWidth ? maxTextWidth : lineTextWidth;
    textSize.width = maxTextWidth * columnWidth;
    textSize.height = textHeight * columnWidth;
    return textSize;
}
```

**Phase 2 approach:** Register a default text measurement function that measures in character cells (1 char = 1 unit). Renderer backends can override this via a hook. Clay will generate TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED errors if no function is set before CLAY_TEXT is used.

**Confidence:** HIGH (verified from Clay source and terminal renderer example)

### Pattern 8: Dynamic String Arena

**What:** Per-frame arena for copying dynamic (stack-local) strings so Clay can reference them during the render pass.

**Problem:** Clay_String stores a `const char*` pointer -- it does NOT copy the string data. If a string is constructed on the stack inside a composition/layout function, it will be invalid by the time the renderer reads it.

**Clay's isStaticallyAllocated flag:** When `isStaticallyAllocated = true`, Clay hashes using the pointer address (fast). When `false`, Clay hashes the actual string contents (correct for dynamic strings at different addresses each frame).

**Per-frame arena design:**
```c
typedef struct {
    char* memory;      // Allocated block
    size_t capacity;   // Total capacity
    size_t offset;     // Current bump position
} CeluiFrameArena;

static CeluiFrameArena g_frame_arena;

// Reset at start of each frame (before BeginLayout)
void frame_arena_reset(void) {
    g_frame_arena.offset = 0;
}

// Copy a string into the arena, return Clay_String
Clay_String frame_arena_copy_string(const char* str, int32_t length) {
    if (g_frame_arena.offset + length > g_frame_arena.capacity) {
        // Handle overflow -- grow or error
    }
    char* dest = g_frame_arena.memory + g_frame_arena.offset;
    memcpy(dest, str, length);
    g_frame_arena.offset += length;
    return (Clay_String){
        .isStaticallyAllocated = false,  // MUST be false for dynamic text
        .length = length,
        .chars = dest
    };
}
```

**Sizing:** Default 16KB should be ample for a terminal UI. Most apps have < 100 dynamic strings per frame, each typically < 64 bytes.

**Confidence:** HIGH (straightforward pattern, verified Clay string lifetime from source)

### Pattern 9: Auto-Generated Clay IDs

**What:** CEL_Clay() automatically generates a unique Clay_ElementId so developers don't need CLAY_ID().

**Clay ID requirements:**
- Each element in a layout pass MUST have a unique ID (Clay detects duplicates)
- IDs are `uint32_t` hashes
- `CLAY_ID("literal")` hashes a string literal
- `CLAY_SID(clay_string)` hashes a Clay_String (can be dynamic)
- `CLAY_IDI("literal", index)` hashes string + numeric index (for loops)
- `CLAY_SID_LOCAL(string)` hashes with parent element ID as seed (scoped uniqueness)

**Auto-ID approach:**

Option A: Hash entity ID + counter
```c
// Generate unique ID per CEL_Clay() call site
static inline Clay_ElementId _cel_clay_auto_id(uint32_t counter) {
    uint32_t entity_id = (uint32_t)cels_get_current_entity();
    return Clay__HashNumber(counter, entity_id);
}
#define CEL_Clay(...) \
    CLAY(_cel_clay_auto_id(__COUNTER__), __VA_ARGS__)
```

This is simple and produces unique IDs because:
- `entity_id` is unique per entity
- `__COUNTER__` is unique per call site within a translation unit

Option B: Hash entity name + counter (more debuggable)
```c
static inline Clay_ElementId _cel_clay_auto_id(uint32_t counter) {
    // Construct a string from entity context for debugging
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "cel_%u_%u",
                       (uint32_t)cels_get_current_entity(), counter);
    Clay_String s = { .length = len, .chars = buf, .isStaticallyAllocated = false };
    return Clay__HashString(s, 0);
}
```

**Recommended:** Option A (Clay__HashNumber). Faster, no string allocation, and Clay IDs are just hashes anyway. For debugging, Clay's error handler prints the string ID which would be empty for hash-only IDs, but this is acceptable for auto-generated IDs.

**Better yet:** Use `CLAY_SIDI()` pattern with entity name:
```c
// At composition time, entity name is available
#define CEL_Clay(...) \
    _CEL_CLAY_IMPL(__COUNTER__, __VA_ARGS__)

#define _CEL_CLAY_IMPL(counter, ...) \
    CLAY(Clay__HashNumber(counter, (uint32_t)cels_get_current_entity()), __VA_ARGS__)
```

**Confidence:** HIGH (Clay__HashNumber is a public-enough internal, and the pattern is sound)

### Anti-Patterns to Avoid

- **Opening CLAY() outside BeginLayout/EndLayout:** Clay will crash or corrupt state. ALL CLAY() calls must happen between Begin and End.
- **Forgetting to close elements:** CLAY() for-loop handles this, but manual Clay__OpenElement/CloseElement pairs must be balanced. Clay checks this at EndLayout.
- **Using CLAY_STRING() with non-literals:** CLAY_STRING() uses sizeof() to compute length, which only works with string literals. For dynamic strings, construct Clay_String manually.
- **Nested static CLAY__ELEMENT_DEFINITION_LATCH:** The latch is a single static uint8_t. Since CLAY() loops run synchronously and each completes its single iteration before the next starts, this is safe. But do NOT try to parallelize Clay layout calls.
- **Calling Clay_SetLayoutDimensions during layout pass:** Must be called before BeginLayout, not between Begin and End.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Clay element ID hashing | Custom hash function | Clay__HashString / Clay__HashNumber | Clay has specific hash collision handling |
| Element tree nesting | Manual Open/Close pairs | CLAY() macro | For-loop ensures balanced Open/Close |
| Text measurement caching | Own measurement cache | Clay's built-in cache | Clay caches per generation, handles invalidation |
| Layout computation | Manual box model | Clay_EndLayout() | Clay handles flexbox-style layout internally |
| Entity child iteration | Custom parent-child tracking | ecs_children() | Flecs already tracks parent-child via ChildOf |
| String lifetime management | Reference counting | Per-frame bump arena | Simpler, zero fragmentation, reset is free |

## Common Pitfalls

### Pitfall 1: Clay calls outside Begin/End boundary
**What goes wrong:** CLAY() calls outside BeginLayout/EndLayout corrupt Clay's internal state or crash.
**Why it happens:** Composition code runs at various times. If CEL_Clay() runs during initial composition but not during the layout pass, the CLAY() calls have no valid context.
**How to avoid:** Ensure CEL_Clay() only produces CLAY() calls during the layout pass. Either: (a) store layout data and replay during the pass, or (b) ensure compositions only run between Begin/End.
**Warning signs:** Clay error "unbalanced open/close", segfaults in Clay internals.

### Pitfall 2: Dynamic string lifetime
**What goes wrong:** CLAY_TEXT with a stack-local buffer produces garbled text in the renderer, because Clay_String stores only a pointer.
**Why it happens:** Stack-local buffers (like `snprintf` results) go out of scope before the renderer reads them.
**How to avoid:** Copy dynamic strings into the frame arena before passing to CLAY_TEXT. Set `isStaticallyAllocated = false` so Clay hashes contents, not pointer.
**Warning signs:** Garbled text, inconsistent text content between frames.

### Pitfall 3: Duplicate Clay IDs
**What goes wrong:** Clay reports CLAY_ERROR_TYPE_DUPLICATE_ID and may produce incorrect layout.
**Why it happens:** Multiple entities using the same ID generation seed, or reusing __COUNTER__ across translation units.
**How to avoid:** Use entity ID as part of the hash seed. Entity IDs are globally unique in flecs.
**Warning signs:** Clay error handler fires with "duplicate ID" message.

### Pitfall 4: ecs_children() ordering not guaranteed stable
**What goes wrong:** Children may appear in different order than declared.
**Why it happens:** Flecs stores entities in archetypes; iteration order depends on archetype structure.
**How to avoid:** Verify that ecs_children() returns entities in creation order for ChildOf relationships. If not, add a "child_index" component to explicitly track order. CELS currently relies on creation order (see cels.cpp get_child_at_index).
**Warning signs:** Layout elements appearing in wrong order.

### Pitfall 5: Text measurement function not set
**What goes wrong:** Clay fires CLAY_ERROR_TYPE_TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED for every CLAY_TEXT call.
**Why it happens:** No text measurement callback was registered after Clay_Initialize.
**How to avoid:** Register a default text measurement function in the Clay_Engine module init. The default can measure in character cells (1 char = 1 unit). Renderer backends can override.
**Warning signs:** Error spam on stderr, all text elements have zero dimensions.

### Pitfall 6: ClaySurface missing from tree
**What goes wrong:** CEL_Clay() blocks produce CLAY() calls but they happen outside any layout pass, causing undefined behavior.
**Why it happens:** Developer uses CEL_Clay() without wrapping the composition tree in a ClaySurface.
**How to avoid:** CEL_Clay() checks a "layout pass active" flag. If not active (no ClaySurface ancestor), emit stderr warning and skip the CLAY() call. This is the context decision requirement.
**Warning signs:** The warning message itself.

## Code Examples

### Clay BeginLayout/EndLayout contract
```c
// Source: clay.h lines 4216-4266

// Before layout:
Clay_SetLayoutDimensions((Clay_Dimensions){ width, height });

// Layout pass:
Clay_BeginLayout();
// ... all CLAY() calls happen here ...
Clay_RenderCommandArray commands = Clay_EndLayout();

// BeginLayout does:
// 1. Resets ephemeral memory (Clay__InitializeEphemeralMemory)
// 2. Increments generation counter
// 3. Opens root container element covering full dimensions

// EndLayout does:
// 1. Closes root element
// 2. Checks for unbalanced open/close
// 3. Runs Clay__CalculateFinalLayout()
// 4. Returns render command array
```

### CLAY() macro expansion
```c
// Source: clay.h lines 143-148
// CLAY(id, .layout = {...}, .backgroundColor = {...}) { children }
// expands to:
for (
    CLAY__ELEMENT_DEFINITION_LATCH = (
        Clay__OpenElementWithId(id),
        Clay__ConfigureOpenElement(/* declaration struct */),
        0);
    CLAY__ELEMENT_DEFINITION_LATCH < 1;
    CLAY__ELEMENT_DEFINITION_LATCH = 1, Clay__CloseElement()
) {
    // children
}
```

### CLAY_TEXT (leaf element, no for-loop)
```c
// Source: clay.h line 159
#define CLAY_TEXT(text, textConfig) Clay__OpenTextElement(text, textConfig)
// This is a simple function call, NOT a for-loop. No nesting.
// text: Clay_String (pointer + length)
// textConfig: Clay_TextElementConfig* (from CLAY_TEXT_CONFIG())
```

### Creating a dynamic Clay_String
```c
// For dynamic text (snprintf results, concatenated strings, etc.):
char buf[64];
int len = snprintf(buf, sizeof(buf), "Count: %d", value);
Clay_String dynamic = {
    .isStaticallyAllocated = false,
    .length = len,
    .chars = frame_arena_copy(buf, len)  // Copy to frame arena
};
CLAY_TEXT(dynamic, CLAY_TEXT_CONFIG({ .fontSize = 16 }));
```

### Text measurement function (terminal/character-cell)
```c
// Source pattern: clay/renderers/terminal/clay_renderer_terminal_ansi.c lines 45-73
static Clay_Dimensions measure_text_cells(
    Clay_StringSlice text,
    Clay_TextElementConfig *config,
    void *userData
) {
    (void)config;
    (void)userData;
    float max_width = 0, line_width = 0, height = 1;
    for (int i = 0; i < text.length; i++) {
        if (text.chars[i] == '\n') {
            if (line_width > max_width) max_width = line_width;
            line_width = 0;
            height++;
        } else {
            line_width++;
        }
    }
    if (line_width > max_width) max_width = line_width;
    return (Clay_Dimensions){ .width = max_width, .height = height };
}

// Register:
Clay_SetMeasureTextFunction(measure_text_cells, NULL);
```

### Entity child iteration (flecs)
```c
// Source: cels.cpp lines 804-810
ecs_iter_t it = ecs_children(world, parent_entity);
while (ecs_children_next(&it)) {
    for (int i = 0; i < it.count; i++) {
        ecs_entity_t child = it.entities[i];
        // Process child
    }
}
```

### CELS composition pattern (for reference)
```c
// Source: cels.h lines 554-562
// _CEL_Compose uses a for-loop that:
// 1. Calls cels_begin_entity(name) -- pushes to entity stack, creates ChildOf pair
// 2. Runs composition body (CEL_Has calls, child compositions)
// 3. Calls cels_end_entity() -- pops entity stack
```

### Clay ID generation approaches
```c
// Static string literal:
CLAY_ID("MyElement")  // -> Clay__HashString(CLAY_STRING("MyElement"), 0)

// Dynamic string:
CLAY_SID(my_clay_string)  // -> Clay__HashString(my_clay_string, 0)

// With numeric index (for loops):
CLAY_IDI("Item", i)  // -> Clay__HashStringWithOffset(CLAY_STRING("Item"), i, 0)

// Local (scoped to parent):
CLAY_ID_LOCAL("Child")  // -> Clay__HashString(CLAY_STRING("Child"), parent_id)

// From a number (internal but available):
Clay__HashNumber(counter, seed)  // -> hash of counter with seed, returns Clay_ElementId
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Clay v0.12 CLAY() took inline struct | Clay v0.14 CLAY() takes (id, ...) with separate id param | v0.13-0.14 | CLAY macro signature changed; first param is always an ID |
| Clay_ElementDeclaration had .id field | id moved to first CLAY() param / CLAY_AUTO_ID() for anonymous | v0.14 | ID is no longer part of the declaration struct |
| CLAY_LAYOUT({...}) config macro | Layout fields inline in Clay_ElementDeclaration | v0.14 | .layout = { ... } directly in CLAY() |
| CLAY_RECTANGLE({...}) config macro | .backgroundColor field in Clay_ElementDeclaration | v0.14 | Simpler API, fewer config wrappers |

**Clay v0.14 API changes (critical for this phase):**
- `CLAY(id, ...)` -- first arg is always a Clay_ElementId
- `CLAY_AUTO_ID(...)` -- auto-generates an anonymous ID (no first id arg)
- `Clay_ElementDeclaration` struct consolidates layout, colors, configs into one struct
- `Clay_SetMeasureTextFunction` signature: `(Clay_StringSlice, Clay_TextElementConfig*, void*) -> Clay_Dimensions`
  - Note: takes `Clay_StringSlice` (not Clay_String) as of v0.14

## Open Questions

### 1. Composition-time vs Layout-pass execution of CEL_Clay()

**What we know:**
- Context decision says CEL_Clay() runs at composition time, not per-frame re-evaluation
- But Clay requires CLAY() calls between BeginLayout/EndLayout
- CELS compositions run during build AND during recomposition

**What's unclear:**
- Does "composition time" mean the composition runs DURING the layout pass (triggered by the PreStore system)?
- Or does CEL_Clay() store layout data that gets replayed during the pass?

**Recommendation:** Implement as a hybrid. CEL_Clay() at composition time generates a static layout function (via macro tricks or stores config data). The PreStore system walks the entity tree and calls those functions between Begin/End. This gives the appearance of "inline composition" while respecting Clay's Begin/End contract. The key insight: the layout function IS the composition body -- CEL_Clay() captures the code block as a function pointer automatically via macro expansion.

Alternatively, the simpler approach: the PreStore system forces recomposition of the ClaySurface subtree each frame, and ClaySurface's composition body wraps everything in Begin/End. This way, composition time IS layout time.

### 2. ecs_children() ordering guarantees

**What we know:** CELS currently assumes creation order via `get_child_at_index()`. Flecs ChildOf iteration likely returns in creation order for simple cases.

**What's unclear:** Is this order guaranteed by flecs across archetype changes, or is it incidental?

**Recommendation:** Test it. If ordering is not stable, add an explicit `ChildOrder { int index; }` component to entities.

### 3. Frame arena sizing and growth

**What we know:** Terminal UIs are small. 16KB should cover most cases.

**What's unclear:** Should the arena grow dynamically, or is a fixed-size arena with overflow error acceptable?

**Recommendation:** Start with a fixed 16KB arena. Log an error if exceeded. Growth can be added later.

## Sources

### Primary (HIGH confidence)
- `/home/cachy/workspaces/libs/clay/clay.h` v0.14 -- Full source read: CLAY macro (lines 136-148), BeginLayout (4216-4233), EndLayout (4236-4266), Clay_SetMeasureTextFunction (3952-3956), Clay__HashString (1371-1384), Clay__HashNumber (1359-1369), Clay_String (195-202), Clay__OpenElement (2001-2035), Clay__CloseElement (1815-1914)
- `/home/cachy/workspaces/libs/cels/include/cels/cels.h` -- Full API: CEL_Composition (554-562), CEL_Has (334-335), _CEL_Compose (590-612), CEL_System (860-886), entity stack
- `/home/cachy/workspaces/libs/cels/src/cels.cpp` -- Entity hierarchy: cels_begin_entity (880-973), cels_end_entity (975-996), ecs_children (804-810), entity_stack, child_of pattern (940)
- `/home/cachy/workspaces/libs/clay/renderers/terminal/clay_renderer_terminal_ansi.c` -- Terminal text measurement pattern (lines 45-73)

### Secondary (MEDIUM confidence)
- `.planning/phases/02-layout-system-core/02-CONTEXT.md` -- Phase context decisions
- `.planning/API-DESIGN.md` -- Original API design (partially superseded by context decisions)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- Only Clay + CELS, both source-verified
- Architecture: HIGH for Clay internals, MEDIUM for the composition-time vs layout-pass question
- Pitfalls: HIGH -- All derived from Clay source analysis and CELS runtime study
- Code examples: HIGH -- All from direct source reading

**Research date:** 2026-02-08
**Valid until:** Stable -- Clay v0.14 is pinned, CELS v0.2 is under our control
