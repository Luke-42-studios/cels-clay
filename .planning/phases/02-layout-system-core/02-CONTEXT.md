# Phase 2: Layout System Core - Context

**Gathered:** 2026-02-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Developers can write CELS compositions that contribute CLAY() blocks to a single per-frame layout tree with correct nesting and ordering. This covers the CEL_Clay() macro, ClaySurface built-in, entity tree walking, layout pipeline coordination (BeginLayout/EndLayout), developer helpers, and resize propagation.

Does NOT include: render bridge (Phase 3), ncurses rendering (Phase 4), or demo app (Phase 5).

</domain>

<decisions>
## Implementation Decisions

### CEL_Clay() Scope API
- **New model: CEL_Clay() is used inside compositions** — replaces the separated layout-function model from API-DESIGN.md
- CEL_Clay() opens a CLAY() scope inline within the composition body. The composition IS the layout unit
- CEL_Clay() evaluates at **composition time** — CELS reactivity (CEL_Watch) keeps it fresh via recomposition on state change. No per-frame re-evaluation needed
- Multiple CEL_Clay() blocks allowed per composition (not restricted to 1:1 entity-to-node)
- Clay IDs are **auto-generated from entity/composition name** — no manual CLAY_ID() needed
- Dynamic strings inside CEL_Clay() are **automatically arena-copied** — no explicit CLAY_DYN_STRING needed by the developer
- If CEL_Clay() is used outside a ClaySurface subtree: **stderr warning**, layout ignored, continues running

### ClaySurface Built-in
- **ClaySurface** is a built-in composition provided by cels-clay
- It owns the BeginLayout/EndLayout boundary — the scope `ClaySurface() { ... }` IS the layout pass
- Everything inside ClaySurface's block contributes CLAY() nodes between Begin and End
- Accepts dimensions as **props (reactive)**: `ClaySurface(.width = w, .height = h) { ... }`
- App wires window size to props (e.g., from cels-ncurses WindowSize state). ClaySurface calls Clay_SetLayoutDimensions() before BeginLayout
- ClaySurface always runs Begin/End — even if empty

### Entity State Model
- State lives as **components on the entity itself** (per-entity, not global AppState)
- Input systems query entity components (e.g., Selectable, ButtonAction) at OnUpdate
- Systems modify entity components directly, which triggers recomposition
- Recomposition re-evaluates CEL_Clay() with fresh component data
- Standard CELS patterns: CEL_Get(), CEL_Watch() for data access inside CEL_Clay() blocks

### Tree Walk Ordering
- **Declaration order** — children appear in the order they're declared in the parent composition
- Non-Clay compositions use **transparent pass-through**: skipped in layout but their Clay children still participate (enables data providers and logic wrappers)
- Dynamic entity additions/removals: **automatic** — new entities appear on next frame, removed entities disappear
- No slot/portal pattern in Phase 2 (deferred to CELS framework)

### Developer Helpers
- **CEL_Clay_Children()** — explicit placement helper. Developer controls exactly WHERE in the CLAY layout tree child entities appear (not always at end of scope)
- **No Clay_Get() helper** — use existing CEL_Get/CEL_Watch (standard CELS patterns, no new abstractions)
- **No cell-aware sizing helpers** — use Clay's native float-based API (CLAY_SIZING_FIXED, etc.). Renderer handles float-to-cell mapping
- Dynamic string arena-copy is automatic (see CEL_Clay() scope section)

### Resize + Lifecycle
- Dimensions provided via ClaySurface props (reactive pattern). App updates props on window resize
- Entity removal: **just disappear** — Clay rebuilds every frame, removed entity = gone next frame. No cleanup hooks
- ClaySurface scope boundary = layout pass boundary. Always runs Begin/End

### Claude's Discretion
- Internal representation of CEL_Clay() data (how the macro captures layout config for the tree walk)
- Tree walk algorithm details (depth-first implementation)
- Per-frame arena management for dynamic strings
- How auto-generated Clay IDs handle uniqueness for multiple instances of the same composition
- Error handling details beyond the stderr warning for misuse

</decisions>

<specifics>
## Specific Ideas

- "If CLAY is an Entity, can you nest Compositions within CEL_Clay" — the core mental model. Compositions ARE the layout tree. Entity nesting = Clay nesting
- ClaySurface as the "canvas" concept — you're telling the developer "you need a surface to do layout on"
- The sidebar example with CEL_Clay_Children(): header above, children in a scrollable section, footer below — all in one composition
- Entity-local state model: button has its own Selected component, input system flips it, recomposition updates the Clay layout

</specifics>

<deferred>
## Deferred Ideas

- **Slot/portal pattern for CELS** — ability for parent compositions to define named slots and descendants to inject content. Powerful template pattern. Should be a CELS framework feature, not cels-clay specific. Add to CELS v0.2 roadmap backlog
- **Update API-DESIGN.md** — the separated layout-function model is superseded by the inline CEL_Clay() model. API-DESIGN.md should be updated to reflect new decisions (can happen during planning)

</deferred>

---

*Phase: 02-layout-system-core*
*Context gathered: 2026-02-08*
