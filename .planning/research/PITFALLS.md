# Pitfalls: cels-clay v0.6 Entity-Based UI + Dual Renderers

**Domain:** Compose-Inspired Entity-Based UI Framework (CELS + Clay)
**Researched:** 2026-03-15
**Confidence:** HIGH (derived from v0.5 implementation experience, CELS v0.4 API analysis, Clay macro internals, INTERFACE library patterns)

## Critical Pitfalls

### P-01: Entity-to-CLAY() Generation Without Layout Functions

**Risk:** Removing layout functions means the layout system must synthesize correct `CLAY()` macro nesting from entity component data. The `CLAY()` macro uses an internal static latch variable — wrapper code must not interfere.

**Warning signs:** Layout produces no output, Clay errors about unmatched open/close elements, segfaults in Clay internals.

**Prevention:**
- The walker directly calls `CLAY()` macro with property data — same approach as v0.5 layout functions, just data-driven instead of function-driven
- Each entity type maps to a known CLAY() pattern (container with children, or leaf like CLAY_TEXT)
- Test with nested entities (3+ levels deep) from the start

**Phase:** Layout system rewrite

### P-02: Component Explosion / Archetype Fragmentation

**Risk:** Six separate property component types (RowProps, ColumnProps, BoxProps, TextProps, SpacerProps, ImageProps) create six distinct ECS archetypes. The layout walker needs to check each type per entity.

**Warning signs:** Slow layout pass, excessive archetype queries.

**Prevention:**
- Accept 6 archetypes — this is correct ECS design (each entity type IS different)
- Consider internal optimization: Row and Column are structurally identical except direction. Could use a shared `ContainerProps` with a direction field internally while exposing Row/Column compositions externally
- The walker does at most 6 `cel_get()` checks per entity — these are O(1) lookups in Flecs

**Phase:** Component definitions

### P-03: CLAY_IMPLEMENTATION in INTERFACE Library

**Risk:** v0.5 already solved this: `clay_impl.c` defines `CLAY_IMPLEMENTATION`. But v0.6 changes the source file structure. Must ensure exactly one TU still owns this.

**Warning signs:** Linker errors (duplicate symbols or undefined symbols for Clay functions).

**Prevention:**
- Keep `clay_impl.c` as-is — it only includes clay.h with the define
- Don't move Clay implementation into any new files

**Phase:** Module refactor

### P-04: Text Measurement Callback Conflict (NCurses vs SDL3)

**Risk:** Clay has ONE global text measurement callback (`Clay_SetMeasureTextFunction`). NCurses measures in cells (1 char = 1 unit), SDL3 measures in pixels (TTF font metrics). If both renderers are compiled and registered, the wrong measurement function produces broken layout.

**Warning signs:** Text overflows containers, text truncated, layout looks wrong.

**Prevention:**
- Each renderer module sets its own measurement callback in its init observer
- Document: register only ONE renderer module
- The last-registered module's callback wins
- Set measurement in lifecycle observer (on_create), which runs before any layout pass

**Phase:** Renderer modules

### P-05: Dual CLAY_IMPLEMENTATION When Both Renderers Compile

**Risk:** If both renderer source files include clay.h headers differently or define conflicting static state, linking fails.

**Warning signs:** Linker duplicate symbol errors, undefined behavior from conflicting static state.

**Prevention:**
- Renderer source files never define CLAY_IMPLEMENTATION — only `clay_impl.c` does
- Renderers include clay.h for type access only (Clay_RenderCommand, etc.)
- Both renderer .c files can coexist since they only use Clay types, not implementation

**Phase:** Build system / CMake

### P-06: Renderer Mutual Exclusion

**Risk:** If both `Clay_NCurses` and `Clay_SDL3` modules are registered, both render systems run at OnRender, causing duplicate drawing or conflicts.

**Warning signs:** Double rendering, visual artifacts, crashes from drawing to wrong backend.

**Prevention:**
- Each renderer module's system should check a state flag indicating it's the active renderer
- Or: document that only one renderer module should be registered
- The ClayEngineState could have an `active_renderer` enum set by whichever renderer module initialized last

**Phase:** Renderer selection / integration

---

## Moderate Pitfalls

### P-07: CEL_Module vs _CEL_DefineModule Migration

**Risk:** v0.5 uses deprecated `_CEL_DefineModule`, `_CEL_Feature`, `_CEL_Provides`. CELS v0.4 uses `CEL_Module`, `CEL_Register`. Wrong macro usage causes compilation failures or registration misses.

**Warning signs:** Components not found, systems not running, state singletons returning NULL.

**Prevention:**
- Follow cels-sdl3 as reference — it uses the correct v0.4 patterns
- `CEL_Module(Name) { cels_register(...); }` replaces `_CEL_DefineModule`
- `CEL_Component(Name)` replaces `CEL_Define(Name)`
- Systems use `cels_system_declare()` pattern from cels-ncurses

**Phase:** Module refactor

### P-08: SDL3 Renderer Accessing SDL_Renderer Handle

**Risk:** The SDL3 renderer needs `SDL_Renderer*` to draw. cels-sdl3 currently only initializes SDL3 context (SDL_Init, TTF_Init) but may not expose the renderer handle via a state singleton.

**Warning signs:** Cannot access SDL_Renderer in the render system, need upstream cels-sdl3 changes.

**Prevention:**
- Check cels-sdl3 for `SDL_Renderer*` exposure before building the renderer
- If not available, extend cels-sdl3 with a Window/Renderer composition that exposes the handle via state singleton
- Alternatively, the SDL3 renderer module could accept renderer config from the app

**Phase:** SDL3 renderer (may require cels-sdl3 upstream work)

### P-09: ClaySurface Lifecycle Changes

**Risk:** v0.5 ClaySurface uses `static ClaySurface_Impl` for INTERFACE library compatibility. v0.6 needs to preserve this while adapting to modern CELS patterns.

**Warning signs:** Multi-TU linking errors, ClaySurface not triggering layout pass.

**Prevention:**
- Keep the static implementation pattern that solved v0.5 multi-TU issues
- Test with a consumer that has multiple TUs including cels-clay headers

**Phase:** ClaySurface adaptation

### P-10: Aspect Ratio Divergence Between Renderers

**Risk:** NCurses uses 1:1 cell units (1 Clay unit = 1 terminal cell, with aspect ratio compensation for non-square cells). SDL3 uses pixel units. The same entity tree would produce different layouts if sizing values are renderer-specific.

**Warning signs:** Layout looks correct on one renderer but wrong on the other.

**Prevention:**
- Entity property values should be renderer-agnostic (Clay units)
- Each renderer's text measurement callback determines how Clay interprets units
- Document that sizing values may need adjustment per renderer (cells vs pixels)
- The example app should use relative sizing (CLAY_SIZING_GROW, CLAY_SIZING_PERCENT) which work identically in both

**Phase:** Example app / integration

### P-11: Component Registration in INTERFACE Libraries

**Risk:** CELS v0.4 component registration (`CEL_Component` + `cels_register()`) may behave differently in INTERFACE library context where sources compile in consumer TUs.

**Warning signs:** Component IDs are 0 or uninitialized, `cel_get()` returns NULL for attached components.

**Prevention:**
- Follow the exact registration pattern from cels-ncurses (which is also an INTERFACE library)
- Component registration functions use extern linkage
- Registration happens in `CEL_Module` body, called from consumer's `CEL_Build`

**Phase:** Module refactor

### P-12: SDL3 Font Lifecycle Management

**Risk:** TTF_Font handles must be loaded before first layout pass (text measurement needs them) and freed before SDL3 shutdown. If the renderer module doesn't manage font lifecycle, fonts leak or are freed too early.

**Warning signs:** Segfault in text measurement, memory leaks on shutdown.

**Prevention:**
- Renderer module lifecycle observer loads default font on create, frees on destroy
- Font handle stored in renderer state singleton
- App can configure font via renderer module config (font path, size)

**Phase:** SDL3 renderer

### P-13: Entity Ordering / SiblingOrder

**Risk:** ECS entity iteration order is non-deterministic, but UI layout depends on child order (first child renders first in a Row). v0.5 solved this with explicit tree walking.

**Warning signs:** Children rendered in wrong order, layout shuffles between frames.

**Prevention:**
- Preserve v0.5's explicit tree walk (parent-before-child, children in creation order)
- CELS compositions create children in declaration order — this IS the ordering
- Do not use ECS queries for layout traversal — walk the entity tree directly

**Phase:** Layout system rewrite

---

## Minor Pitfalls

### P-14: Dead Code from Feature/Provider Pattern

**Risk:** v0.5's `ClayRenderable` Feature, `_CEL_Provides` pattern, and `ClayRenderableData` singleton will be unused in v0.6. Leaving dead code confuses future development.

**Warning signs:** Unused code warnings, developer confusion about which pattern to use.

**Prevention:**
- Remove `clay_render.h/c` (Feature/Provider render bridge)
- Remove `ClayRenderable`, `ClayRenderableData`
- Replace with direct state singleton access for render commands

**Phase:** Module refactor (cleanup)

### P-15: atexit Cleanup Replacement

**Risk:** v0.5 may use `atexit()` for Clay arena cleanup. Modern CELS lifecycle observers handle cleanup via `on_destroy`. Using both causes double-free.

**Warning signs:** Double-free crash on shutdown, "already freed" errors.

**Prevention:**
- Remove any `atexit()` calls
- All cleanup in lifecycle `on_destroy` observers

**Phase:** Module refactor

### P-16: Composition Shorthand Macros

**Risk:** `#define Row(...) cel_init(Row, __VA_ARGS__)` can conflict with other macros or variable names. The macro name `Row` is very common.

**Warning signs:** Unexpected macro expansion, compilation errors in unrelated code.

**Prevention:**
- Use the namespaced pattern if conflicts arise: `Clay_Row(...)` instead of `Row(...)`
- Or accept short names (Compose does: `Row`, `Column`, `Text`) since they're in the developer's include scope by choice

**Phase:** Component definitions

### P-17: SDL3 IMAGE Render Command Handling

**Risk:** Clay emits `CLAY_RENDER_COMMAND_TYPE_IMAGE` commands with an opaque `void* imageData`. The SDL3 renderer must know this is an `SDL_Texture*`. The NCurses renderer must handle this gracefully (draw placeholder).

**Warning signs:** Crash when rendering Image entities, NCurses crash on Image commands.

**Prevention:**
- SDL3 renderer casts `imageData` to `SDL_Texture*` (documented contract)
- NCurses renderer draws a colored rectangle placeholder for Image commands
- Document that Image `source` pointer is renderer-specific

**Phase:** SDL3 renderer + NCurses renderer

---

## Phase Ordering Implications

**Based on pitfall dependencies:**

1. **Module refactor first** (P-03, P-07, P-11, P-14, P-15) — Foundation must be correct before building on it
2. **Component definitions** (P-02, P-16) — Define entity types before layout walker
3. **Layout system rewrite** (P-01, P-13) — Core translation engine
4. **ClaySurface adaptation** (P-09) — Wire layout system to surface
5. **NCurses renderer** (P-04, P-06, P-10, P-17) — Port existing, establish renderer contract
6. **SDL3 renderer** (P-04, P-05, P-06, P-08, P-12, P-17) — New work, most pitfalls
7. **Renderer selection + integration** (P-06, P-10) — Final wiring

---
*Researched: 2026-03-15*
