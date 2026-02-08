# Phase 5: Demo App + Integration - Research

**Researched:** 2026-02-08
**Domain:** CELS + Clay + ncurses integration demo application
**Confidence:** HIGH

## Summary

This phase creates the first consumer of cels-clay: a three-page terminal application (Home, Settings, About) proving the full CELS state + Clay layout + ncurses rendering pipeline. Research focused entirely on internal codebase investigation since all dependencies are custom (CELS, cels-clay, cels-ncurses) with no external libraries to evaluate.

The existing `examples/app.c` provides the canonical pattern for a CELS application using cels-ncurses. The demo app follows the same architecture but replaces the custom render provider with Clay-based layout. The cels-clay module provides all necessary APIs: `Clay_Engine_use()` for initialization, `ClaySurface` composition for layout boundary, `CEL_Clay_Layout` + `CEL_Clay` + `CEL_Clay_Children` for layout functions, `CEL_Clay_Text` for dynamic strings, and `clay_ncurses_renderer_init()` for terminal rendering. The ncurses renderer handles rectangles, text, borders, scissors, and scroll containers.

**Primary recommendation:** Follow the existing `examples/app.c` architecture exactly, replacing the manual `_CEL_Provides` render provider with Clay layout functions. The demo lives in `examples/demo/` under the cels-clay module and builds as a CMake executable target linked to `cels`, `cels-clay`, `cels-ncurses`, and `flecs::flecs_static`.

## Standard Stack

This is entirely an internal codebase integration. No external libraries beyond what is already fetched.

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cels | v0.2 | Declarative ECS framework | The framework being demonstrated |
| cels-clay | (this module) | Clay layout integration | Phases 1-4 built this |
| cels-ncurses | (sibling module) | Terminal rendering + input | Existing TUI backend |
| Clay | v0.14 | Flexbox-style layout engine | Fetched by cels-clay CMakeLists.txt |
| flecs | v4.1.4 | ECS runtime | Underlying ECS engine |
| ncurses (wide) | system | Terminal I/O | Required by cels-ncurses |

### No New Dependencies

The demo app requires zero new dependencies. Everything needed exists in the cels + cels-clay + cels-ncurses stack.

## Architecture Patterns

### App File Structure

Based on the existing `examples/app.c` pattern and the CONTEXT.md decision that the demo lives in `examples/demo/`:

```
modules/cels-clay/examples/demo/
    main.c          # CEL_Build entry point, module init, root composition
    pages.h         # Page compositions (Home, Settings, About) + layout functions
    components.h    # App-level components (PageId, Toggle, NavState, etc.)
    theme.h         # Color theme definitions (Theme A, Theme B)
```

**Rationale:** The existing app uses `app.c` (monolith) + `components.h` + `render_provider.h` + `widgets/*.h`. The demo replaces the render_provider layer entirely with Clay layout functions and needs far fewer files since Clay handles layout math. Splitting pages and theme into headers keeps main.c clean.

### Recommended Composition Hierarchy

```
CEL_Build(DemoApp)
  TUI_Engine_use(...)          # ncurses window + input + frame pipeline
  Clay_Engine_use(...)         # Clay arena + layout system + render bridge
  clay_ncurses_renderer_init() # Register ncurses as Clay renderer
  InitState()                  # Default state values

  CEL_Root(AppUI, TUI_EngineContext)
    ClaySurface(.width = win->width, .height = win->height)
      AppShell()               # Title bar + status bar + main body
        Sidebar()              # 25% width, navigation items
        ContentRouter()        # Watches page state, switches content
          HomePage()           # Feature showcase
          SettingsPage()       # Toggles with live reactivity
          AboutPage()          # Scroll container
```

### Pattern 1: Clay Layout Function per Composition

**What:** Each composition that participates in the Clay tree gets a `CEL_Clay_Layout(name)` function and attaches `ClayUI` component.
**When to use:** Every UI composition in the demo.

```c
// Source: cels-clay/include/cels-clay/clay_layout.h
CEL_Clay_Layout(sidebar_layout) {
    CEL_Clay(.layout = {
        .layoutDirection = CLAY_TOP_TO_BOTTOM,
        .sizing = { .width = CLAY_SIZING_PERCENT(0.25f), .height = CLAY_SIZING_GROW(0) },
        .padding = CLAY_PADDING_ALL(1)
    }) {
        CEL_Clay_Children();
    }
}

CEL_Composition(Sidebar) {
    (void)props;
    CEL_Has(ClayUI, .layout_fn = sidebar_layout);

    NavItem(.label = "Home",     .index = 0) {}
    NavItem(.label = "Settings", .index = 1) {}
    NavItem(.label = "About",    .index = 2) {}
}
#define Sidebar(...) CEL_Init(Sidebar, __VA_ARGS__)
```

### Pattern 2: Dynamic Text via CEL_Clay_Text

**What:** Runtime-computed strings use the per-frame arena to survive until the renderer reads them.
**When to use:** Any text that changes based on state (toggle values, page title, status bar hints).

```c
// Source: cels-clay/include/cels-clay/clay_layout.h
CEL_Clay_Layout(toggle_layout) {
    Toggle* t = (Toggle*)ecs_get_id(world, self, ToggleID);
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%s: %s",
                       t->label, t->enabled ? "ON" : "OFF");
    CEL_Clay(.layout = { .padding = CLAY_PADDING_ALL(1) }) {
        CLAY_TEXT(CEL_Clay_Text(buf, len),
                  CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255} }));
    }
}
```

### Pattern 3: Reactive Page Routing

**What:** A `ContentRouter` composition watches page state and conditionally mounts the active page's composition.
**When to use:** For the three-page navigation pattern.

```c
// Source: examples/app.c (MenuRouter pattern)
CEL_State(NavState, {
    int current_page;       // 0=Home, 1=Settings, 2=About
    int sidebar_selected;   // Which sidebar item is highlighted
    int focus_pane;         // 0=sidebar, 1=content
});

#define ContentRouter(...) CEL_Init(ContentRouter, __VA_ARGS__)
CEL_Composition(ContentRouter) {
    (void)props;
    NavState_t* nav = CEL_Watch(NavState);

    switch (nav->current_page) {
        case 0: HomePage() {} break;
        case 1: SettingsPage() {} break;
        case 2: AboutPage() {} break;
    }
}
```

### Pattern 4: ClaySurface Wired to Window Dimensions

**What:** ClaySurface dimensions are reactive to terminal resize via TUI_WindowState observation.
**When to use:** In the root composition, wrapping all UI content.

```c
// Source: cels-clay/include/cels-clay/clay_layout.h (ClaySurface composition)
CEL_Root(AppUI, TUI_EngineContext) {
    TUI_WindowState_t* win = CEL_WatchId(ctx.windowState, TUI_WindowState_t);

    if (win->state == WINDOW_STATE_READY) {
        // ClaySurface dimensions use terminal aspect ratio:
        // Clay's coordinate system maps to cells via the 2:1 aspect ratio
        // in the ncurses renderer, so we divide width by aspect ratio
        ClaySurface(.width = (float)win->width / 2.0f,
                    .height = (float)win->height) {
            AppShell() {}
        }
    }
}
```

**Critical detail:** The ncurses renderer applies `cell_aspect_ratio` (2.0f) to horizontal values when converting Clay coordinates to cells. So if the terminal is 80 columns wide, ClaySurface width should be 40.0f (80/2.0) so that Clay's proportional layout maps correctly. Text measurement already returns cell-accurate widths (not scaled), and the text bbox converter only aspect-scales the x-position.

### Pattern 5: Input System for Navigation

**What:** A CEL_System at OnUpdate reads CELS_Input and mutates NavState.
**When to use:** For sidebar navigation (j/k/h/l/Enter) and page-specific interactions.

```c
// Source: examples/app.c (MainMenuInputSystem pattern)
CEL_System(NavInputSystem, .phase = CELS_Phase_OnUpdate) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    const CELS_Input* input = cels_input_get(ctx);

    // Vim keys arrive as raw_key (h, l, j, k fall through to default case
    // in tui_input.c and set has_raw_key = true, raw_key = char value)
    if (input->has_raw_key) {
        switch (input->raw_key) {
            case 'j': /* move selection down */ break;
            case 'k': /* move selection up */ break;
            case 'h': /* focus sidebar */ break;
            case 'l': /* focus content */ break;
        }
    }
    if (input->button_accept) { /* Enter - select item */ }
}
```

**Key insight about input mapping:** Looking at `tui_input.c`, the keys j, k, h, l are NOT mapped to axis or any predefined field. They fall through to the `default:` case which sets `input->raw_key = ch` and `input->has_raw_key = true`. This is the correct path for Vim-style navigation. However, there is a conflict: `input->axis_left[1]` is set by arrow keys AND by w/s keys. The demo should use `raw_key` for j/k/h/l and `button_accept` for Enter. The scroll handler already uses this priority: Vim raw_key first.

### Pattern 6: Scroll Container for About Page

**What:** Clay scroll container with Phase 4's keyboard scroll handler.
**When to use:** The About page's scrollable content.

```c
// Source: cels-clay/include/cels-clay/clay_ncurses_renderer.h
// In the About page layout function:
CEL_Clay_Layout(about_layout) {
    CEL_Clay(.layout = {
        .layoutDirection = CLAY_TOP_TO_BOTTOM,
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }
    },
    .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() }) {
        CLAY_TEXT(CLAY_STRING("Long about text..."),
                  CLAY_TEXT_CONFIG({ .textColor = {200, 200, 200, 255} }));
        // ... more text blocks
    }
}

// In the frame loop or input system, when About page is focused:
// clay_ncurses_handle_scroll_input(input, delta_time);
```

**Critical detail:** `clay_ncurses_handle_scroll_input` is a plain function (not an ECS system) that the app calls explicitly. The app controls focus -- only call it when the About page's scroll container should receive scroll input. The function calls `Clay_UpdateScrollContainers()` internally.

### Anti-Patterns to Avoid

- **Do NOT create a custom render provider callback.** The old `examples/app.c` uses `_CEL_DefineFeature(Renderable)` + `_CEL_Provides()` with manual draw calls. The demo replaces this entirely with Clay layout functions and the ncurses renderer handles all drawing.
- **Do NOT use `tui_draw_text` / `tui_draw_fill_rect` directly.** All drawing goes through Clay render commands. The ncurses renderer translates them to draw primitives.
- **Do NOT use WASD for navigation.** WASD is mapped to `axis_left` which conflicts with Vim keys. Use `raw_key` for h/j/k/l navigation.
- **Do NOT call `wrefresh()` or `wnoutrefresh()`.** The frame pipeline (`tui_frame_begin/end`) handles compositing via `update_panels() + doupdate()`.
- **Do NOT register text measurement manually.** `clay_ncurses_renderer_init()` calls `Clay_SetMeasureTextFunction()` with the wcwidth-based measurer.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Layout computation | Manual ncurses cell positioning | Clay flexbox via CLAY() macros | Clay handles sizing, padding, alignment, grow/shrink |
| Text positioning | Manual mvwprintw with calculated positions | CLAY_TEXT inside CEL_Clay blocks | Clay positions text within its parent layout |
| Border drawing | Manual draw_border calls | Clay border config `CLAY_BORDER_OUTSIDE(1)` | Renderer translates to tui_draw_border automatically |
| Scroll handling | Manual scroll offset tracking | `Clay_GetScrollOffset()` + `.clip = { .vertical = true }` | Clay manages scroll state, renderer clips |
| Dynamic string lifetime | Manual malloc/strdup for formatted text | `CEL_Clay_Text(buf, len)` | Per-frame arena handles lifetime |
| Aspect ratio compensation | Manual width/2 calculations | Renderer's `cell_aspect_ratio` (2.0f default) | Handled transparently in bbox-to-cells conversion |
| Color pair management | Manual `init_pair()` + pair numbers | `tui_color_rgb()` + `TUI_Style` | `alloc_pair()` invisible pair management |

## Common Pitfalls

### Pitfall 1: ClaySurface Dimensions vs Terminal Columns

**What goes wrong:** Passing raw terminal COLS as ClaySurface width results in layout being 2x too wide because the renderer scales horizontal values by `cell_aspect_ratio` (2.0f).
**Why it happens:** Clay works in abstract coordinate units. The ncurses renderer divides Clay x-coordinates by aspect ratio to get cell columns. If you pass 80 columns as width, Clay thinks it has 80 units, which the renderer maps to 160 columns.
**How to avoid:** Divide terminal width by aspect ratio: `ClaySurface(.width = (float)win->width / 2.0f, .height = (float)win->height)`.
**Warning signs:** UI appears to only fill the left half of the terminal, or elements are half their expected width.

### Pitfall 2: j/k Keys Conflict with WASD Navigation

**What goes wrong:** Using `input->axis_left[1]` for sidebar navigation captures both arrow keys (desired) AND WASD (undesired if also using j/k via raw_key).
**Why it happens:** `tui_input.c` maps w/W to axis_left[1]=-1 and s/S to axis_left[1]=1. Meanwhile j/k fall through to raw_key. If input handling checks both axis AND raw_key, w and j both trigger "up".
**How to avoid:** Use `input->has_raw_key` and `input->raw_key` exclusively for j/k/h/l. Use `input->button_accept` for Enter. Do NOT check `axis_left` for demo navigation. Arrow keys also set axis_left but that is a secondary concern -- consistent Vim bindings via raw_key are cleaner.
**Warning signs:** Double-movement on key press, or confusing mixed controls.

### Pitfall 3: Scroll Input Called Every Frame Regardless of Focus

**What goes wrong:** Calling `clay_ncurses_handle_scroll_input()` unconditionally sends scroll deltas even when the About page scroll container is not active.
**Why it happens:** The function calls `Clay_UpdateScrollContainers()` which affects ALL scroll containers. If there is only one scroll container (About page), this works but wastes cycles on non-scroll pages.
**How to avoid:** Only call `clay_ncurses_handle_scroll_input()` when `focus_pane == CONTENT && current_page == ABOUT_PAGE`.
**Warning signs:** Unexpected scrolling behavior on other pages.

### Pitfall 4: Missing CEL_Clay_Children in Layout Functions

**What goes wrong:** Child compositions don't appear in the rendered output even though they exist in the ECS.
**Why it happens:** The layout tree walker only visits children when `CEL_Clay_Children()` is called inside the parent's layout function. Without it, the CLAY scope closes and children are never emitted.
**How to avoid:** Every layout function that has child compositions MUST call `CEL_Clay_Children()` at the point where children should appear.
**Warning signs:** Parent renders correctly but children are invisible.

### Pitfall 5: Forgetting clay.h Include for CLAY Macros

**What goes wrong:** Compilation errors for `CLAY()`, `CLAY_TEXT()`, `CLAY_STRING()`, etc.
**Why it happens:** `clay_engine.h` intentionally does NOT include `clay.h` (documented decision). Consumers must include it separately.
**How to avoid:** Include `clay.h` in files that use Clay layout macros. It is on the include path from CMakeLists.txt (`${clay_SOURCE_DIR}`).
**Warning signs:** "undeclared identifier" errors for Clay macros.

### Pitfall 6: Static String Literals Work Without CEL_Clay_Text

**What goes wrong:** Developers copy all strings to the frame arena even when they are compile-time constants.
**Why it happens:** Misunderstanding when `CEL_Clay_Text` is needed. `CLAY_STRING("literal")` creates a `Clay_String` pointing to the string literal (which has static lifetime). Only `snprintf`-formatted or dynamically constructed strings need `CEL_Clay_Text`.
**How to avoid:** Use `CLAY_STRING("text")` for static strings and `CEL_Clay_Text(buf, len)` only for dynamic strings.
**Warning signs:** Unnecessary frame arena usage, potential overflow with many strings.

### Pitfall 7: CMake Target Ordering

**What goes wrong:** `cels-ncurses` target does not exist when cels-clay CMakeLists.txt evaluates `if(TARGET cels-ncurses)`, so ncurses renderer sources are not compiled.
**Why it happens:** CMake `add_subdirectory` order matters. cels-ncurses must be added before cels-clay in the parent CMakeLists.txt.
**How to avoid:** Verify the root CMakeLists.txt adds cels-ncurses BEFORE cels-clay (it already does this correctly at lines 165-172).
**Warning signs:** Link errors for `clay_ncurses_renderer_init`, or the renderer simply not working.

## Code Examples

### Complete CEL_Build Entry Point

```c
// Source: Synthesized from examples/app.c + cels-clay API headers
#include <cels/cels.h>
#include <cels-ncurses/tui_engine.h>
#include <cels-clay/clay_engine.h>
#include <cels-clay/clay_layout.h>
#include <cels-clay/clay_ncurses_renderer.h>
#include <clay.h>
#include "components.h"
#include "pages.h"
#include "theme.h"

CEL_Build(DemoApp) {
    (void)props;

    // 1. Initialize TUI engine (window + input + frame pipeline)
    TUI_Engine_use((TUI_EngineConfig){
        .title = "cels-clay demo",
        .version = "0.2.0",
        .fps = 60,
        .root = AppUI
    });

    // 2. Initialize Clay engine (arena + layout system + render bridge)
    Clay_Engine_use(NULL);  // NULL = all defaults

    // 3. Register ncurses as Clay renderer (text measurement + provider)
    clay_ncurses_renderer_init(NULL);  // NULL = default theme

    // 4. Initialize application state
    NavState = (NavState_t){
        .current_page = 0,
        .sidebar_selected = 0,
        .focus_pane = 0  // sidebar
    };
    DemoSettings = (DemoSettings_t){
        .show_borders = true,
        .color_mode = 0  // Theme A
    };
}
```

### ClaySurface With Window Dimensions

```c
// Source: cels-clay/include/cels-clay/clay_layout.h (ClaySurface macro)
CEL_Root(AppUI, TUI_EngineContext) {
    TUI_WindowState_t* win = CEL_WatchId(ctx.windowState, TUI_WindowState_t);

    if (win->state == WINDOW_STATE_READY) {
        ClaySurface(.width = (float)win->width / 2.0f,
                    .height = (float)win->height) {
            AppShell() {}
        }
    }
}
```

### Layout Function With Border and Background Color

```c
// Source: Clay v0.14 API (verified from clay.h in build/_deps)
CEL_Clay_Layout(title_bar_layout) {
    CEL_Clay(
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(3) },
            .padding = { .left = 2, .right = 2 },
            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
        },
        .border = { .width = CLAY_BORDER_OUTSIDE(1), .color = {100, 100, 100, 255} },
        .backgroundColor = {30, 30, 50, 255}
    ) {
        CLAY_TEXT(CLAY_STRING("cels-clay demo"),
                  CLAY_TEXT_CONFIG({ .textColor = {200, 200, 255, 255} }));
    }
}
```

### Clay Scroll Container

```c
// Source: Clay v0.14 examples/raylib-sidebar-scrolling-container/main.c
CEL_Clay_Layout(about_content_layout) {
    CEL_Clay(
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
            .padding = CLAY_PADDING_ALL(1),
            .childGap = 1
        },
        .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() }
    ) {
        CLAY_TEXT(CLAY_STRING("About cels-clay..."),
                  CLAY_TEXT_CONFIG({ .textColor = {200, 200, 200, 255} }));
        // ... additional paragraphs of text
    }
}
```

### Input System Reading raw_key

```c
// Source: tui_input.c default case + clay_ncurses_renderer.c scroll handler
static CELS_Input g_prev_input = {0};

CEL_System(DemoInputSystem, .phase = CELS_Phase_OnUpdate) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    const CELS_Input* input = cels_input_get(ctx);

    if (input->has_raw_key && !g_prev_input.has_raw_key) {
        switch (input->raw_key) {
            case 'j':
                CEL_Update(NavState) {
                    if (NavState.focus_pane == 0) { // sidebar
                        NavState.sidebar_selected++;
                        if (NavState.sidebar_selected > 2)
                            NavState.sidebar_selected = 0;
                    }
                }
                break;
            case 'k':
                CEL_Update(NavState) {
                    if (NavState.focus_pane == 0) {
                        NavState.sidebar_selected--;
                        if (NavState.sidebar_selected < 0)
                            NavState.sidebar_selected = 2;
                    }
                }
                break;
            case 'h':
                CEL_Update(NavState) { NavState.focus_pane = 0; }
                break;
            case 'l':
                CEL_Update(NavState) { NavState.focus_pane = 1; }
                break;
        }
    }

    // Enter selects current sidebar item
    if (input->button_accept && !g_prev_input.button_accept) {
        if (NavState.focus_pane == 0) {
            CEL_Update(NavState) {
                NavState.current_page = NavState.sidebar_selected;
            }
        }
    }

    // Handle scroll input only when content pane is focused AND About page
    if (NavState.focus_pane == 1 && NavState.current_page == 2) {
        clay_ncurses_handle_scroll_input(input, g_frame_state.delta_time);
    }

    memcpy((void*)&g_prev_input, input, sizeof(CELS_Input));
}
```

### CMake Target for Demo

```cmake
# Source: Pattern from root CMakeLists.txt (app target, lines 175-183)
# Added to root CMakeLists.txt, after cels-clay add_subdirectory:

add_executable(cels_clay_demo
    modules/cels-clay/examples/demo/main.c
)
target_link_libraries(cels_clay_demo PRIVATE
    cels cels-clay cels-ncurses flecs::flecs_static
)
target_include_directories(cels_clay_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/cels-clay/examples/demo
)
set_target_properties(cels_clay_demo PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual `_CEL_Provides` render callback | Clay layout functions + ncurses renderer | Phase 4 | No manual draw calls needed |
| `tui_draw_text` direct calls | `CLAY_TEXT` inside `CEL_Clay` blocks | Phase 4 | Layout engine positions text |
| Manual `tui_draw_border` positioning | Clay border config `.border = { .width = ... }` | Phase 4 | Borders are part of layout |
| Custom screen routing with render callback | `ClaySurface` + `CEL_Clay_Layout` + entity tree walk | Phase 2 | Declarative UI structure |

**Important context:** The existing `examples/app.c` uses the OLD approach (manual render provider with tui_draw_* calls). The demo app is the FIRST consumer of the new Clay-based approach. This is the primary reason Phase 5 exists -- to prove the new stack works end-to-end.

## Open Questions

### 1. Aspect Ratio and Text Width Interaction

**What we know:** The renderer uses two conversion functions: `clay_bbox_to_cells` (scales x by 2.0) and `clay_text_bbox_to_cells` (scales x by 2.0 but NOT width). Text measurement returns widths in cell columns (wcwidth). This means text and non-text elements use different scaling rules.

**What's unclear:** Whether a sidebar at `CLAY_SIZING_PERCENT(0.25f)` with text inside will look correct -- the sidebar width is aspect-scaled but text widths are not. There may be visual mismatch where text appears wider than its container.

**Recommendation:** Build a minimal test first (just sidebar + content with text) and visually verify before implementing all three pages. If misalignment occurs, the text bbox converter may need adjustment.

### 2. Border Color for Focus Indication

**What we know:** The ncurses renderer uses `TUI_COLOR_DEFAULT` for all border colors (hardcoded in `render_border`). The CONTEXT.md requires "focused pane indicated by border highlight and title highlight."

**What's unclear:** How to make focused-pane borders visually distinct when the renderer ignores Clay's border color. Options:
- (a) Modify the renderer to use Clay border colors (small renderer change)
- (b) Use Clay rectangle backgrounds instead of borders to indicate focus
- (c) Use text color/attributes to indicate focus (title highlight only)

**Recommendation:** Option (b) is most pragmatic -- add a thin colored rectangle behind the border area, or use a different background color on the focused pane's container. This avoids changing the Phase 4 renderer.

### 3. Theme Toggle Runtime Effect

**What we know:** `clay_ncurses_renderer_set_theme()` changes the renderer theme at runtime. But the theme controls border chars and aspect ratio, not element colors. Clay element colors are specified in layout functions via `Clay_Color`.

**What's unclear:** Whether "Theme A vs Theme B" means changing the renderer theme (border chars) or changing color values used in Clay layout functions.

**Recommendation:** Theme toggle should change color constants used in layout functions. The renderer theme (border chars, aspect ratio) stays constant. Store theme colors in DemoSettings state; layout functions read them.

## Sources

### Primary (HIGH confidence)
- `cels-clay/include/cels-clay/clay_engine.h` -- Module init API
- `cels-clay/include/cels-clay/clay_layout.h` -- Layout macros (CEL_Clay, CEL_Clay_Layout, CEL_Clay_Children, CEL_Clay_Text, ClaySurface)
- `cels-clay/include/cels-clay/clay_render.h` -- Render bridge API (ClayRenderableData)
- `cels-clay/include/cels-clay/clay_ncurses_renderer.h` -- ncurses renderer API + theme + scroll handler
- `cels-clay/src/clay_ncurses_renderer.c` -- Renderer implementation (coordinate mapping, text measurement, scroll input)
- `cels-clay/src/clay_layout.c` -- Layout system (entity tree walk, frame arena, component registration)
- `cels-clay/src/clay_engine.c` -- Engine init (arena, error handler, cleanup)
- `cels-clay/src/clay_render.c` -- Render bridge (Feature/Provider, dispatch system)
- `cels-clay/CMakeLists.txt` -- Build configuration (INTERFACE library, conditional ncurses)
- `examples/app.c` -- Canonical CELS application pattern
- `examples/components.h` -- Component definition pattern
- `examples/render_provider.h` -- Feature/Provider pattern (replaced by Clay)
- `cels/cels.h` -- Full CELS macro API
- `cels-ncurses/include/cels-ncurses/tui_engine.h` -- TUI_Engine_use API
- `cels-ncurses/include/cels-ncurses/tui_input.h` -- Input provider
- `cels-ncurses/src/input/tui_input.c` -- Input key mapping (critical: j/k/h/l -> raw_key)
- `cels-ncurses/include/cels-ncurses/tui_color.h` -- TUI_Style, tui_color_rgb, TUI_COLOR_DEFAULT
- `cels-ncurses/include/cels-ncurses/tui_draw.h` -- Draw primitives (used by renderer, not by demo directly)
- `cels-ncurses/include/cels-ncurses/tui_frame.h` -- Frame pipeline, g_frame_state (delta_time)
- `CMakeLists.txt` (root) -- Build structure, target patterns, module ordering
- `.planning/API-DESIGN.md` -- Architecture decisions, entity tree -> Clay nesting
- `.planning/STATE.md` -- Prior decisions accumulated across phases
- `build/_deps/clay-src/examples/raylib-sidebar-scrolling-container/main.c` -- Clay scroll container pattern

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- All code is internal, directly inspected
- Architecture: HIGH -- Follows existing app.c pattern with Clay substitution
- Pitfalls: HIGH -- Derived from reading actual source code (coordinate mapping, input handling, render pipeline)
- Code examples: HIGH -- Assembled from verified APIs in existing headers/source
- Open questions: MEDIUM -- Three questions about visual behavior require runtime verification

**Research date:** 2026-02-08
**Valid until:** 2026-03-08 (internal codebase, stable within v0.2 development)
