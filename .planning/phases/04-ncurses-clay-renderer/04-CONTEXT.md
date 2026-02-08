# Phase 4: ncurses Clay Renderer - Context

**Gathered:** 2026-02-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Translate Clay render commands into visible terminal output via ncurses. The renderer consumes `Clay_RenderCommandArray` from the Phase 3 render bridge and draws rectangles, text, borders, and clipped regions to the terminal with correct color mapping. This phase does NOT add new layout capabilities or input handling — it purely renders what Clay produces.

</domain>

<decisions>
## Implementation Decisions

### Color mapping strategy
- Target 256-color ncurses palette
- Use ncurses native color palette system (`init_color` + `init_pair`) to define custom colors from Clay's RGBA values
- Alpha channel maps to brightness — lower alpha = dimmer color (A_DIM or darker shade in 256 palette)
- Respect terminal's default background — only draw explicit Clay background colors; unset areas show through to terminal theme
- Color pair management through ncurses palette features (not manual pre-allocation)

### Border & box-drawing style
- Theme-driven border rendering — developer defines border character sets in a C struct theme
- Wide range of box-drawing characters supported — can mix styles (e.g., rounded top corners, single-line bottom)
- Per-side border configuration supported through theme
- Default border width = 1 cell; developer can override via theme
- Theme overrides Clay's native border/color data — Clay handles layout math, theme controls visual appearance
- Sensible built-in default theme ships with the renderer (single-line Unicode borders, 256-color mapping)
- Theme scoped per ClaySurface, not global — different surfaces can have different visual themes
- Border color comes from theme, not Clay's border config

### Scroll container behavior
- Vim-style key bindings: j/k for single lines, Ctrl-U/Ctrl-D for half-page, gg/G for top/bottom
- Scroll speed: 1 line per step (arrow/j/k)
- Unicode scrollbar character drawn on right edge of scrollable containers (block characters)
- App manages focus — renderer does NOT own which container receives scroll input; app decides via CELS input systems

### Float-to-cell coordinate mapping
- Round to nearest cell (3.7 → 4, 3.2 → 3)
- Minimum 1 cell — any element with width or height > 0 gets at least 1 cell rendered
- Aspect ratio compensation enabled — scale horizontal values to account for terminal cells being taller than wide
- Configurable aspect ratio: default 2:1, developer can override (e.g., 1.8:1 for their font)
- Raw terminal cols x rows passed to Clay as layout dimensions
- All spacing (padding, margin) also scaled by aspect ratio for proportional appearance
- Overlap resolved by Clay's z-order (floating layers, render command ordering) — not painter's algorithm
- SIGWINCH caught for immediate re-layout on terminal resize (update dimensions + trigger re-layout, don't wait for next frame tick)

### Claude's Discretion
- Exact ncurses `init_color`/`init_pair` allocation strategy and caching
- Scrollbar visual design (which Unicode block characters)
- Text measurement callback implementation details
- Scissor/clipping ncurses implementation approach
- Default theme's specific color palette choices
- Wide character / multi-byte text handling approach

</decisions>

<specifics>
## Specific Ideas

- Theme is a C struct (compile-time, type-safe) — not an external file
- ncurses color palette features are the right tool for color management (user explicitly called this out)
- Theme-per-surface means a debug panel could look different from the main app UI
- Vim-style navigation is the expected terminal convention for this project

</specifics>

<deferred>
## Deferred Ideas

- None — discussion stayed within phase scope

</deferred>

---

*Phase: 04-ncurses-clay-renderer*
*Context gathered: 2026-02-08*
