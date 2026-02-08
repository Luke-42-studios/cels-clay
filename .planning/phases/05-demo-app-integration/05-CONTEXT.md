# Phase 5: Demo App + Integration - Context

**Gathered:** 2026-02-08
**Status:** Ready for planning

<domain>
## Phase Boundary

A working terminal application proving CELS state + Clay layout + ncurses rendering work end-to-end. Three-page app (Home, Settings, About) with sidebar navigation, keyboard interaction, and live state reactivity. Structured as a reusable example in `examples/demo/`.

</domain>

<decisions>
## Implementation Decisions

### App structure
- Three pages: Home, Settings, About
- Top title bar spanning full width with app name ("cels-clay demo")
- Bottom status bar showing key hints (e.g., "j/k: navigate  h/l: switch pane  q: quit")
- Both bars fully bordered (box-drawing characters)
- `q` quits the app cleanly (Ctrl-C as fallback)
- Lives in `examples/demo/` as a reusable starting template with clean code

### Home page content
- Feature showcase: mix of nested containers and varied text styles
- Demonstrates Clay's nesting/hierarchy plus text rendering capabilities
- Not just placeholder text — shows what cels-clay can do

### Settings page
- Two toggles: "Show borders" and "Color mode"
- "Show borders" toggles content area borders only (sidebar and bars keep borders)
- "Color mode" switches between two different color schemes (Theme A vs Theme B)
- Toggles activated with Enter key only
- Toggle state persists when navigating away and returning (state lives in CELS components)
- Changes take immediate visible effect in the app (live reactivity demo)

### About page
- Scrollable content demonstrating Phase 4's scroll container
- Longer text that requires scrolling to read fully

### Color theme
- Use terminal default colors (respects user's theme)
- Color mode toggle provides Theme A vs Theme B alternatives

### Layout proportions
- Sidebar: 25% of terminal width with a max width cap
- Vertical line divider between sidebar and content, plus subtle color difference
- 1 cell padding inside content areas
- Content title bar at top of content area showing current page name

### Navigation & interaction
- h/l (Vim-style) to switch focus between sidebar and content pane
- j/k to move sidebar highlight, Enter to select and switch page
- Focused pane indicated by both border highlight and title highlight
- Page switch is immediate (no transition)
- Scroll navigation in About page uses Phase 4's Vim-style bindings (j/k/Ctrl-U/Ctrl-D/G/gg)

### Claude's Discretion
- Max width value for sidebar
- Exact Theme A and Theme B color definitions
- Feature showcase specific content and nesting structure
- About page text content
- Scroll container configuration for About page

</decisions>

<specifics>
## Specific Ideas

- "This should be an app-level example" — meant to be the canonical way to build a cels-clay app
- Reusable example: clean code with comments, users can copy and modify
- Existing Main Menu + Settings concept as starting point, extended to three pages
- Settings toggles should have immediate visible effect on the UI (best reactivity demo)

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 05-demo-app-integration*
*Context gathered: 2026-02-08*
