---
phase: 05-demo-app-integration
plan: 01
subsystem: ui
tags: [cels, clay, ncurses, terminal, demo, layout, flexbox]

# Dependency graph
requires:
  - phase: 04-ncurses-clay-renderer
    provides: ncurses renderer, scroll input handler, coordinate mapping
  - phase: 02-clay-layout-component
    provides: ClayUI component, CEL_Clay layout macros, ClaySurface composition
  - phase: 03-clay-render-bridge
    provides: render bridge, Feature/Provider pattern for Clay backends
provides:
  - Three-page demo app (Home, Settings, About) with sidebar navigation
  - Canonical cels-clay consumer code (reusable example/template)
  - CMake build target cels_clay_demo
  - End-to-end validation of CELS + Clay + ncurses pipeline
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "CEL_Clay_Layout with ecs_get_id for component data access in layout functions"
    - "CEL_Clay_Text for dynamic strings, CLAY_STRING for static literals"
    - "ClaySurface width = terminal_cols / 2.0 for aspect ratio compensation"
    - "ContentRouter composition with CEL_Watch for reactive page switching"
    - "DemoInputSystem with raw_key for Vim-style navigation (not axis_left)"

key-files:
  created:
    - examples/demo/main.c
    - examples/demo/components.h
    - examples/demo/theme.h
    - examples/demo/pages.h
  modified:
    - /home/cachy/workspaces/libs/cels/CMakeLists.txt
    - include/cels-clay/clay_layout.h
    - include/cels-clay/clay_render.h
    - src/clay_render.c
    - src/clay_ncurses_renderer.c

key-decisions:
  - "DemoInputSystem registered via ensure() in CEL_Build, not CEL_Use in composition (system defined in main.c, compositions in pages.h)"
  - "ContentRouter has no ClayUI -- transparent pass-through (layout tree walker handles this)"
  - "Settings toggle selection reuses sidebar_selected when focus_pane == 1"
  - "Static ClaySurface_Impl to fix INTERFACE library multi-TU linking"
  - "Non-static ClayRenderable_ensure for cross-TU visibility in INTERFACE library"
  - "Renderer callback uses CELS_Iter* (not ecs_iter_t*) for type-safe provider registration"

patterns-established:
  - "Composition ordering: leaf-first in headers so #define macros are visible to parent compositions"
  - "Dynamic text from component data: use CEL_Clay_Text(ptr, strlen(ptr)), not CLAY_STRING(ptr)"
  - "Theme system: DemoTheme struct with inline demo_get_theme() selecting by color_mode index"

# Metrics
duration: 12min
completed: 2026-02-08
---

# Phase 5 Plan 1: Demo App + Integration Summary

**Three-page terminal demo app (Home/Settings/About) with sidebar navigation, Vim-style input, live theme/border toggling, and scroll containers -- first consumer of the cels-clay module**

## Performance

- **Duration:** 12 min
- **Started:** 2026-02-08T23:13:01Z
- **Completed:** 2026-02-08T23:24:41Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments
- Complete three-page demo app with sidebar navigation, reactive page routing, and keyboard interaction
- Settings page with live toggles for border visibility and color theme (Theme A cool blue, Theme B warm amber)
- About page with Clay scroll container and Vim-style scroll keybindings
- First successful end-to-end compilation of CELS + Clay + ncurses consumer code
- Fixed three pre-existing cross-TU build issues in cels-clay INTERFACE library

## Task Commits

Each task was committed atomically:

1. **Task 1: Create demo app source files** - `d2d9585` (feat)
2. **Task 2: Build fixes for INTERFACE library** - `43fe0b5` (fix, cels-clay repo)
3. **Task 2: Add CMake build target** - `46d4ecd` (feat, parent CELS repo)

## Files Created/Modified
- `examples/demo/main.c` - CEL_Build entry point, DemoInputSystem, AppUI root composition
- `examples/demo/components.h` - NavState, DemoSettings, NavItemData component definitions
- `examples/demo/theme.h` - Two switchable color themes with 9-color DemoTheme struct
- `examples/demo/pages.h` - 11 layout functions + 12 compositions for complete UI tree
- `CMakeLists.txt` (root) - cels_clay_demo executable target
- `include/cels-clay/clay_layout.h` - Static ClaySurface_Impl for multi-TU safety
- `include/cels-clay/clay_render.h` - Extern ClayRenderable_ensure declaration
- `src/clay_render.c` - Non-static ClayRenderable_ensure for cross-TU visibility
- `src/clay_ncurses_renderer.c` - CELS_Iter callback signature, removed flecs dependency

## Decisions Made
- **DemoInputSystem registration:** Called `DemoInputSystem_ensure()` in CEL_Build rather than `CEL_Use()` in composition, because the system is defined in main.c but compositions are in pages.h (cross-file dependency)
- **ContentRouter without ClayUI:** The router composition only switches pages via CEL_Watch -- no visual element needed. The layout tree walker treats it as a transparent pass-through.
- **Settings selection reuses sidebar_selected:** When focus_pane == 1 and current_page == 1 (Settings), j/k moves sidebar_selected in range 0-1 to select between the two toggles
- **Composition ordering:** Leaf compositions defined first in pages.h so their #define shorthand macros are visible when parent compositions reference them

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] CLAY_STRING used with dynamic strings**
- **Found during:** Task 2 (build verification)
- **Issue:** `CLAY_STRING(item->label)` and `CLAY_STRING(page_names[page])` fail -- CLAY_STRING expects string literals (uses `"" x ""` concatenation)
- **Fix:** Replaced with `CEL_Clay_Text(ptr, strlen(ptr))` for dynamic strings
- **Files modified:** examples/demo/pages.h
- **Verification:** Build succeeds
- **Committed in:** 43fe0b5

**2. [Rule 1 - Bug] ClaySurface_Impl multiple definition across TUs**
- **Found during:** Task 2 (linking)
- **Issue:** CEL_Composition generates non-static `_Impl` function; INTERFACE library compiles it in multiple TUs causing linker errors
- **Fix:** Manually expanded CEL_Composition(ClaySurface) with static linkage
- **Files modified:** include/cels-clay/clay_layout.h
- **Verification:** cels_clay_demo and app targets both link successfully
- **Committed in:** 43fe0b5

**3. [Rule 1 - Bug] ClayRenderable_ensure invisible across TUs**
- **Found during:** Task 2 (compilation)
- **Issue:** `_CEL_DefineFeature` creates static inline function; clay_ncurses_renderer.c can't see it from clay_render.c
- **Fix:** Manually expanded with non-static linkage, added extern declaration in clay_render.h
- **Files modified:** src/clay_render.c, include/cels-clay/clay_render.h
- **Verification:** Build succeeds
- **Committed in:** 43fe0b5

**4. [Rule 1 - Bug] Renderer callback type mismatch with CELS provider system**
- **Found during:** Task 2 (compilation)
- **Issue:** clay_ncurses_render used `ecs_iter_t*` but `cels_provider_register` expects `cels_system_fn` = `void(*)(CELS_Iter*)`
- **Fix:** Changed callback to accept `CELS_Iter*`, use `cels_iter_count()` and `cels_iter_column()` instead of raw flecs API
- **Files modified:** src/clay_ncurses_renderer.c
- **Verification:** Build succeeds, existing app target unaffected
- **Committed in:** 43fe0b5

---

**Total deviations:** 4 auto-fixed (4 bugs)
**Impact on plan:** All fixes were necessary for the build target to compile and link. Three were pre-existing INTERFACE library issues exposed by the first multi-TU consumer. No scope creep.

## Issues Encountered
- INTERFACE library pattern creates separate translation units from each source file, exposing symbol visibility issues that single-TU builds (like the app target) never encounter. This is inherent to the chosen architecture and now resolved.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Demo app compiles and is ready for runtime testing
- All cels-clay APIs exercised: Clay_Engine_use, ClaySurface, CEL_Clay_Layout, CEL_Clay_Children, CEL_Clay_Text, clay_ncurses_renderer_init, clay_ncurses_handle_scroll_input
- Open questions from research (aspect ratio alignment, border color focus indication, theme toggle effect) can now be verified visually by running the demo

---
*Phase: 05-demo-app-integration*
*Completed: 2026-02-08*
