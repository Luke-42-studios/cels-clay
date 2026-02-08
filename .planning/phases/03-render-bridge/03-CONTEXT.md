# Phase 3: Render Bridge + Module Definition - Context

**Gathered:** 2026-02-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Layout results are exposed to renderer backends through the CELS Feature/Provider pattern, and the module has a clean single-call initialization with composable sub-pieces. This phase does NOT implement any renderer — it builds the bridge that renderers plug into.

</domain>

<decisions>
## Implementation Decisions

### Provider callback contract
- Callback receives a struct with: Clay_RenderCommandArray, layout dimensions, frame number, delta time
- Include a dirty flag indicating whether layout changed since last frame — renderers can skip redraw when clean
- Both provider-based and getter API: standard backends use `_CEL_Provides`, advanced users call `cel_clay_get_render_commands()` for custom systems

### Module initialization API
- Config struct pattern: `Clay_Engine_use(&(ClayEngineConfig){.arena_size = 8192})` — zero = defaults (C99 designated initializer idiom)
- Config pointer always required (no NULL shorthand): `Clay_Engine_use(&(ClayEngineConfig){0})` for all defaults
- Configurable fields: arena size, error handler, initial layout dimensions
- Composable sub-modules: `clay_layout_use()` and `clay_render_use()` as individual pieces
- `Clay_Engine_use()` exists as convenience wrapper calling all sub-modules
- Automatic cleanup via CELS lifecycle — no manual cleanup call needed

### Render dispatch timing
- Render dispatch runs at PostStore phase (after PreStore layout completes)
- Trust CELS phase ordering for correctness — no explicit readiness flag
- New ClayRenderable feature, separate from cels-ncurses Renderable

### Backend registration
- One backend at a time (single provider for ClayRenderable)
- Use raw `_CEL_Provides` for registration — consistent with all other CELS modules, no special macro
- Component + Provider pattern: cels-clay creates a singleton render target entity, backends provide for ClayRenderable on that entity
- Log warning once to stderr if no renderer backend is registered on first frame dispatch

### Claude's Discretion
- Exact frame info struct field naming and types
- Singleton render target entity lifecycle details
- Internal dirty-tracking implementation approach
- Getter API function signature details

</decisions>

<specifics>
## Specific Ideas

- Developer consistency is the priority — all engines register the same way via `_CEL_Provides`, no special-case APIs
- Composable architecture: developers who want full control can use `clay_layout_use()` + custom system with getter, while standard path is `Clay_Engine_use()` + provider registration
- "It's easy to know I just have to listen for X Phase when I create a system" — the PostStore phase should be clearly documented as the render slot

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-render-bridge*
*Context gathered: 2026-02-08*
