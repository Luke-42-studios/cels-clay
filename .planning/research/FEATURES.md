# Feature Landscape

**Domain:** ECS + Immediate-Mode UI Layout Integration (CELS + Clay)
**Researched:** 2026-02-07
**Overall confidence:** HIGH (based on direct source analysis of Clay v0.14 header, CELS v0.1 API, existing cels-ncurses module patterns, and Clay's terminal renderers)

## Table Stakes

Features the module must have or it is unusable. Missing any one of these means Clay+CELS integration does not work.

### 1. Clay Initialization and Arena Ownership

| Aspect | Detail |
|--------|--------|
| What | cels-clay owns `Clay_Initialize()`, arena allocation via `Clay_MinMemorySize()` + `Clay_CreateArenaWithCapacityAndMemory()`, and `Clay_SetMeasureTextFunction()` setup |
| Why expected | Clay requires initialization before any `CLAY()` macros can execute. Without this, nothing renders. The module must own this so apps don't manage Clay internals. |
| Complexity | Low |
| Notes | Arena is a one-time malloc. `Clay_SetMeasureTextFunction` must be backend-pluggable -- terminal measures in cells, SDL measures in pixels. Provide a registration hook, ship a default cell-based MeasureText. |

### 2. CEL_Clay() Scope Macro

| Aspect | Detail |
|--------|--------|
| What | A macro that opens a Clay layout scope inside a CELS composition. Wraps `Clay_BeginLayout()` ... `Clay_EndLayout()` and exposes the render command array. |
| Why expected | This is the core integration point. Without it, there is no bridge between CELS compositions and Clay layout trees. |
| Complexity | Medium |
| Notes | Must handle: (1) setting layout dimensions from `CELS_Window`, (2) calling `Clay_BeginLayout()`, (3) executing the user's CLAY() block, (4) calling `Clay_EndLayout()` to get `Clay_RenderCommandArray`, (5) storing commands for the Provider to consume. The for-loop trick used by both CELS and Clay macros can be composed. |

**Design decision:** CEL_Clay() wraps a single Clay tree per composition. All active CEL_Clay() blocks contribute to one global Clay tree per frame (Clay's model is one tree per `BeginLayout/EndLayout` pair), so CEL_Clay must coordinate a single begin/end cycle per frame, not per composition.

### 3. ClayRenderable Feature + Provider Contract

| Aspect | Detail |
|--------|--------|
| What | A CELS Feature (`ClayRenderable`) defined at `CELS_Phase_OnRender` or `OnStore`. Provider callback receives `Clay_RenderCommandArray` and iterates render commands. |
| Why expected | Follows the established CELS Feature/Provider pattern (same as the existing TUI Renderable). Without this, there is no rendering pipeline. |
| Complexity | Medium |
| Notes | The Provider callback must receive the full `Clay_RenderCommandArray` -- not individual commands. Clay outputs commands pre-sorted by z-index, and the renderer must process them in order. Store the array on an ECS component or pass it through a global after `Clay_EndLayout()`. |

**Render command types the Provider must handle:**

| Command Type | What it does | Terminal mapping |
|---|---|---|
| `CLAY_RENDER_COMMAND_TYPE_RECTANGLE` | Solid color fill | ncurses: fill cells with spaces + background color pair |
| `CLAY_RENDER_COMMAND_TYPE_TEXT` | Text string at position | ncurses: `mvprintw()` with color pair from `textColor` |
| `CLAY_RENDER_COMMAND_TYPE_BORDER` | Bordered box (left/right/top/bottom widths) | ncurses: Unicode box-drawing chars or ASCII `+`/`-`/`|` |
| `CLAY_RENDER_COMMAND_TYPE_SCISSOR_START` | Begin clipping region | ncurses: track scissor box, skip draws outside |
| `CLAY_RENDER_COMMAND_TYPE_SCISSOR_END` | End clipping region | ncurses: restore full window bounds |
| `CLAY_RENDER_COMMAND_TYPE_CUSTOM` | Opaque user data passthrough | Ignored in v1 -- future extension point |
| `CLAY_RENDER_COMMAND_TYPE_IMAGE` | Image with data pointer | Out of scope for terminal v1 |

### 4. Float-to-Cell Coordinate Mapping

| Aspect | Detail |
|--------|--------|
| What | Convert Clay's float pixel coordinates (`Clay_BoundingBox` with float x, y, width, height) to integer terminal cell coordinates. Includes a configurable cell pixel size and a `MeasureText` function that returns dimensions in Clay's pixel space. |
| Why expected | Clay operates in float pixel space. Terminals operate in integer cell space. Every render command needs this conversion. Without it, layout is garbled. |
| Complexity | Medium |
| Notes | Clay's terminal renderer uses `columnWidth` (a single int) for square cells. The termbox2 renderer uses separate `cell_width` and `cell_height` (non-square cells: typical 9x21 pixels). The ncurses renderer should use separate width/height for correct aspect ratio. Snapping strategy matters: the termbox2 renderer snaps using `roundf(box.x / cell_width)` for position and `roundf((box.x + box.width) / cell_width) - roundf(box.x / cell_width)` for width (accounts for sub-cell offsets). Use this approach. |

### 5. Text Measurement Callback

| Aspect | Detail |
|--------|--------|
| What | A `Clay_Dimensions MeasureText(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData)` function that returns text dimensions in Clay's coordinate space. |
| Why expected | Clay calls this to compute layout. If measurement is wrong, text overflows or gets clipped incorrectly. The module must provide a terminal-correct implementation. |
| Complexity | Low-Medium |
| Notes | For monospace terminals: `width = char_count * cell_pixel_width`, `height = line_count * cell_pixel_height`. Must handle multi-byte UTF-8 (use `wcwidth()` or equivalent for CJK wide characters). Clay's termbox2 renderer does this correctly via `tb_utf8_char_to_unicode()` + `tb_wcwidth()`. The ANSI renderer does it naively (1 char = 1 cell). Ship correct UTF-8 measurement. |

### 6. Color Mapping (RGBA to Terminal Color Pairs)

| Aspect | Detail |
|--------|--------|
| What | Convert Clay's `Clay_Color` (float RGBA 0-255) to ncurses color pairs (or ncurses extended colors if available). |
| Why expected | Every RECTANGLE, TEXT, and BORDER command includes color. Without mapping, everything is monochrome. |
| Complexity | Medium |
| Notes | ncurses has limited colors: 8 base (COLOR_BLACK..COLOR_WHITE), potentially 256 extended via `can_change_color()`. Ship a nearest-color lookup for 8/16 colors (LUT approach like termbox2 renderer). Design the API so 256-color and true-color can be added later without breaking. Use `assume_default_colors(-1, -1)` for terminal theme compatibility (already a CELS convention). |

### 7. Layout Dimension Sync from Window State

| Aspect | Detail |
|--------|--------|
| What | Each frame, read `CELS_Window` (or `TUI_WindowState`) dimensions and call `Clay_SetLayoutDimensions()` with the terminal size in Clay's pixel space: `width_cells * cell_pixel_width`, `height_cells * cell_pixel_height`. |
| Why expected | Clay needs current viewport size for layout. Terminal resizes are common (`SIGWINCH`). Without sync, layout is wrong after resize. |
| Complexity | Low |
| Notes | Already handled by cels-ncurses window provider (`TUI_WindowState.width`, `.height`). cels-clay reads these and converts to Clay pixel space. |

### 8. CLAY() Macro Passthrough

| Aspect | Detail |
|--------|--------|
| What | Inside a `CEL_Clay()` block, all Clay macros work as-is: `CLAY()`, `CLAY_AUTO_ID()`, `CLAY_TEXT()`, `CLAY_ID()`, `CLAY_SIZING_GROW()`, etc. |
| Why expected | The hybrid API model means Clay's DSL is used directly for UI subtrees. If any Clay macro is wrapped or broken, developers lose access to Clay's documented API. |
| Complexity | Low (just don't break it) |
| Notes | This is an anti-feature to avoid: do NOT re-wrap Clay macros. Let developers use `CLAY()` directly. The only CELS macro is `CEL_Clay()` to open the scope. |

### 9. Module Definition via _CEL_DefineModule

| Aspect | Detail |
|--------|--------|
| What | cels-clay is a proper CELS module with `_CEL_DefineModule(Clay_Layout)` (or similar), exposable via `Clay_Layout_init()`. Follows the cels-ncurses pattern. |
| Why expected | Consistent with CELS module architecture. Without it, initialization is ad-hoc. |
| Complexity | Low |
| Notes | Module owns: Clay arena init, MeasureText registration, ClayRenderable feature definition. Does NOT own window or input -- those are cels-ncurses's job. |

### 10. CMake FetchContent for Clay

| Aspect | Detail |
|--------|--------|
| What | Clay fetched automatically via CMake FetchContent from its GitHub repo. No manual file copying. |
| Why expected | Constraint from PROJECT.md. Also consistent with how CELS fetches flecs. |
| Complexity | Low |
| Notes | Clay is a single header. FetchContent fetches the repo, module sets include path to `clay.h`. `CLAY_IMPLEMENTATION` defined in exactly one `.c` file in cels-clay. |

---

## Differentiators

Features that make the DX great. Not expected, but valued. Build after table stakes are solid.

### 1. Automatic Clay ID Generation from CELS Context

| Aspect | Detail |
|--------|--------|
| What | Optionally auto-generate Clay IDs from the CELS composition name + entity position, so developers don't have to manually specify `CLAY_ID("sidebar")` for every element. |
| Value proposition | Reduces boilerplate. In the current Clay API, duplicate IDs cause errors (`CLAY_ERROR_TYPE_DUPLICATE_ID`). Auto-generation from CELS context (composition name + call index) prevents collisions. |
| Complexity | Medium |
| Notes | Could provide a `CEL_CLAY_ID(label)` macro that hashes the composition name + label. Or use `CLAY_IDI_LOCAL()` with the CELS entity ID as the index. Not needed for v1 -- `CLAY_AUTO_ID()` exists for cases where IDs are not needed. |

### 2. Cell-Aware Sizing Helpers

| Aspect | Detail |
|--------|--------|
| What | Convenience macros: `CLAY_CELL_WIDTH(n)` expands to `CLAY_SIZING_FIXED(n * cell_pixel_width)`, `CLAY_CELL_HEIGHT(n)` expands to `CLAY_SIZING_FIXED(n * cell_pixel_height)`, `CLAY_CELL_PADDING(l,r,t,b)` converts cell counts to pixel padding. |
| Value proposition | Terminal developers think in cells, not pixels. Writing `CLAY_SIZING_FIXED(3 * 9)` is error-prone. `CLAY_CELL_WIDTH(3)` is readable and correct. |
| Complexity | Low |
| Notes | The termbox2 demo already does this manually: `.padding = { 6 * Clay_Termbox_Cell_Width(), ... }`. Providing macros codifies this pattern. Macros need access to the cell size, which can be a global or a function call. |

### 3. Scroll Container Support for Keyboard Navigation

| Aspect | Detail |
|--------|--------|
| What | Wire keyboard input (Page Up/Down, arrow keys) to `Clay_UpdateScrollContainers()` so scroll containers work in keyboard-only terminal mode. |
| Value proposition | Clay's scroll support assumes mouse wheel. Terminal v1 is keyboard-only. Without this, scroll containers are declared but non-functional. |
| Complexity | Medium |
| Notes | Need to: (1) track which scroll container has "focus", (2) translate keyboard input to `scrollDelta` in `Clay_UpdateScrollContainers()`, (3) handle scroll position via `Clay_GetScrollContainerData()`. This is where CELS state (focused element) bridges to Clay scroll state. |

### 4. Unicode Box-Drawing Border Renderer

| Aspect | Detail |
|--------|--------|
| What | Render Clay border commands using Unicode box-drawing characters (single/double line) with corner detection, instead of just colored cells. |
| Value proposition | Terminal UIs look significantly better with proper box-drawing characters (U+250C, U+2510, U+2514, U+2518, U+2502, U+2500). The termbox2 renderer already implements this. |
| Complexity | Low-Medium |
| Notes | Must detect corners (top-left, top-right, bottom-left, bottom-right) vs straight edges. Can offer ASCII fallback (`+`, `-`, `|`) for terminals without Unicode. |

### 5. betweenChildren Border Support

| Aspect | Detail |
|--------|--------|
| What | Clay's `Clay_BorderWidth.betweenChildren` generates separator lines between child elements. Render these as horizontal or vertical lines depending on layout direction. |
| Value proposition | Very common UI pattern (dividers between list items, sidebar sections). Clay supports it natively, just need to render it. |
| Complexity | Low |
| Notes | Clay generates individual RECTANGLE render commands for `betweenChildren` borders. The renderer already handles rectangles, so this may work for free. Verify. |

### 6. Error Handler Integration

| Aspect | Detail |
|--------|--------|
| What | Wire Clay's `Clay_ErrorHandler` to CELS's logging/error infrastructure. Surface Clay layout errors (duplicate IDs, arena exceeded, etc.) through CELS's debug facilities. |
| Value proposition | Without this, Clay errors are silent or crash. With it, developers see actionable messages in the terminal or cels-debug. |
| Complexity | Low |
| Notes | Clay provides structured error data (`Clay_ErrorData` with `errorType` and `errorText`). Map to a CELS log function or stderr. |

### 7. Floating Element Support

| Aspect | Detail |
|--------|--------|
| What | Support Clay's floating elements (dropdowns, tooltips, overlays) in the terminal renderer. Floating elements have z-index and attach to parent/root/element-by-ID. |
| Value proposition | Enables dropdown menus, modals, and status overlays -- common TUI patterns. Clay handles the layout math; the renderer just needs to respect z-order (which it gets for free since Clay pre-sorts commands by z-index). |
| Complexity | Low (rendering) / Medium (interaction) |
| Notes | Rendering is straightforward -- Clay outputs commands in z-order. The harder part is interaction: floating elements with `CLAY_POINTER_CAPTURE_MODE_CAPTURE` block click-through, but terminal v1 is keyboard-only so this is less relevant. |

### 8. Aspect Ratio Element Support

| Aspect | Detail |
|--------|--------|
| What | Support Clay's `Clay_AspectRatioElementConfig` for maintaining element proportions. |
| Value proposition | Keeps layout proportional during terminal resizes. |
| Complexity | Low (Clay handles the math) |
| Notes | Clay computes the dimensions; the renderer just draws the resulting bounding box. No special terminal handling needed. |

---

## Anti-Features

Things to deliberately NOT build. Common mistakes in this domain that would add complexity without value or would violate architectural boundaries.

### 1. Re-wrapping Clay's CLAY() Macro

| Anti-Feature | Creating a `CEL_Layout()` or `CEL_Box()` that wraps `CLAY()` with CELS-specific syntax |
|---|---|
| Why avoid | Clay's macro DSL is well-designed, well-documented, and battle-tested. Wrapping it creates a translation layer that: (a) breaks when Clay updates, (b) prevents developers from using Clay docs directly, (c) adds maintenance burden for zero benefit. |
| What to do instead | Provide `CEL_Clay()` as the scope opener only. Inside, developers use `CLAY()` directly. |

### 2. Widget Library in the Module

| Anti-Feature | Building Button, TextInput, Slider, etc. as part of cels-clay |
|---|---|
| Why avoid | Widgets are app-level compositions, not layout-level concerns. The current CELS demo already builds Button/Slider as compositions combining atomic components. Putting widgets in the layout module conflates layout with behavior. |
| What to do instead | Layout module provides primitives (rect, text, border). Widgets are CELS compositions that use Clay layout internally. Example widget library could be a separate module (`cels-clay-widgets`) or just app code. |

### 3. Mouse/Pointer Interaction System

| Anti-Feature | Building mouse hover/click handling into cels-clay for terminal v1 |
|---|---|
| Why avoid | PROJECT.md explicitly marks mouse input as out of scope. ncurses v1 is keyboard-only. Clay's pointer system (`Clay_SetPointerState`, `Clay_Hovered()`, `Clay_OnHover()`) requires mouse coordinates. Adding this prematurely creates untestable code paths. |
| What to do instead | Design the API so `Clay_SetPointerState` can be called if a mouse backend exists, but don't build the mouse backend. The ncurses input provider currently doesn't emit mouse events. |

### 4. Image/Bitmap Rendering

| Anti-Feature | Supporting `CLAY_RENDER_COMMAND_TYPE_IMAGE` in the terminal renderer |
|---|---|
| Why avoid | PROJECT.md marks images as out of scope. Terminal image rendering (as seen in termbox2 renderer) requires stb_image + complex character mask matching. Massive complexity for a niche feature. |
| What to do instead | Renderer skips IMAGE commands with a no-op. Document that IMAGE support is future/backend-specific. If someone needs terminal images, they can add a CUSTOM element handler. |

### 5. Animation/Tweening System

| Anti-Feature | Building animation state machines or interpolation into cels-clay |
|---|---|
| Why avoid | Animation is an application concern, not a layout module concern. Clay is immediate-mode -- layout is recomputed each frame. CELS state drives what's visible. Animation is just changing CELS state over time, which apps can do with systems. |
| What to do instead | Apps use CELS systems with delta time to animate state changes. Layout responds naturally since Clay rebuilds every frame. |

### 6. Multi-Context Clay (Multiple Clay Trees)

| Anti-Feature | Supporting multiple independent Clay contexts for different UI regions |
|---|---|
| Why avoid | Clay v0.14 supports `Clay_GetCurrentContext()` / `Clay_SetCurrentContext()` for multi-context, but this adds significant complexity for zero benefit in v1. One global tree is the correct model: all compositions contribute to one layout, Clay handles the full UI for correct layout math. |
| What to do instead | One `Clay_BeginLayout()` / `Clay_EndLayout()` per frame. All CEL_Clay() blocks contribute to this single tree. |

### 7. Text Input Fields

| Anti-Feature | Building text editing, cursor management, selection in cels-clay |
|---|---|
| Why avoid | PROJECT.md marks text editing as out of scope. Text input is a widget-level concern requiring cursor state, selection ranges, clipboard, IME -- none of which Clay handles. This is a full widget library problem. |
| What to do instead | A future `cels-clay-widgets` module could build text input as a composition using Clay's CUSTOM element type for the editing area. |

### 8. Cross-Module Dependency on cels-ncurses

| Anti-Feature | Making cels-clay import or depend on cels-ncurses directly |
|---|---|
| Why avoid | PROJECT.md constraint: sibling modules with no cross-deps. cels-clay defines the layout contract (Feature + render command array). cels-ncurses (or future cels-sdl) provides the renderer. The app wires them together. If cels-clay depends on ncurses, you can't swap backends. |
| What to do instead | cels-clay defines `ClayRenderable` feature and a component holding `Clay_RenderCommandArray`. A separate "clay-ncurses renderer" (which lives in cels-ncurses or in the app layer) registers as Provider for `ClayRenderable`. |

### 9. True Color Support in v1

| Anti-Feature | Building true color (24-bit) rendering into the initial terminal renderer |
|---|---|
| Why avoid | PROJECT.md says "design for future, 8/256 color for now." True color requires detecting terminal capabilities (`COLORTERM=truecolor`), using ncurses extended colors API, and fallback logic. Adds testing complexity. |
| What to do instead | Ship 8-color nearest-match in v1. Abstract color mapping behind a function pointer so 256/truecolor can be swapped in later. The termbox2 renderer's LUT approach is a good reference. |

### 10. Clay Configuration GUI

| Anti-Feature | Building a visual tool to configure Clay layout parameters |
|---|---|
| Why avoid | PROJECT.md: "this is a code-first API." Clay's built-in debug tools (when compiled with `CLAY_DEBUG`) provide layout inspection. Adding a configuration GUI is scope creep. |
| What to do instead | Use Clay's built-in debug mode. Expose it via a flag in the cels-clay module config. |

---

## Feature Dependencies

```
                    +------------------+
                    | CMake FetchContent|
                    | for Clay (10)    |
                    +--------+---------+
                             |
                    +--------v---------+
                    | Clay Init +      |
                    | Arena (1)        |
                    +--------+---------+
                             |
              +--------------+--------------+
              |                             |
    +---------v----------+       +----------v---------+
    | Text Measurement   |       | Layout Dim Sync    |
    | Callback (5)       |       | from Window (7)    |
    +--------------------+       +--------------------+
              |                             |
              +--------------+--------------+
                             |
                    +--------v---------+
                    | CEL_Clay() Scope |
                    | Macro (2)        |
                    +--------+---------+
                             |
              +--------------+--------------+
              |                             |
    +---------v----------+       +----------v---------+
    | Float-to-Cell      |       | CLAY() Macro       |
    | Coord Mapping (4)  |       | Passthrough (8)    |
    +--------------------+       +--------------------+
              |
    +---------v----------+
    | Color Mapping      |
    | RGBA->Terminal (6)  |
    +--------------------+
              |
    +---------v----------+
    | ClayRenderable     |
    | Feature+Provider(3)|
    +--------------------+
              |
    +---------v----------+
    | Module Def via     |
    | _CEL_DefineModule(9)|
    +--------------------+
```

**Critical path:** 10 -> 1 -> 5,7 -> 2 -> 4,8 -> 6 -> 3 -> 9

**Dependency notes:**
- Clay Init (1) must happen before any Clay macros execute
- Text Measurement (5) must be registered before `Clay_BeginLayout()` or text layout is wrong
- CEL_Clay() (2) depends on init and text measurement being done
- Coordinate mapping (4) and color mapping (6) are needed by the renderer (3), not by layout
- Module def (9) bundles everything and is the last step
- Differentiators (cell helpers, scroll, box-drawing) all depend on the full table stakes path

---

## MVP Recommendation

For MVP, prioritize in this order:

1. **CMake FetchContent + Clay Init + Arena** (table stakes 10, 1) -- Get Clay compiling and initializing
2. **Text Measurement Callback** (table stakes 5) -- Correct layout math for terminal
3. **Layout Dimension Sync** (table stakes 7) -- Clay knows the terminal size
4. **CEL_Clay() Scope Macro** (table stakes 2) -- The core integration point
5. **Float-to-Cell Mapping** (table stakes 4) -- Render commands become drawable
6. **Color Mapping** (table stakes 6) -- Visible output with color
7. **ClayRenderable Feature + Provider** (table stakes 3) -- Full rendering pipeline
8. **Module Definition** (table stakes 9) -- Clean init story
9. **CLAY() Passthrough** (table stakes 8) -- Verified by demo app

Defer to post-MVP:
- **Cell-aware sizing helpers** (differentiator 2): Nice but developers can do `n * cell_width` manually
- **Scroll keyboard support** (differentiator 3): Requires focus tracking system, build after basic rendering works
- **Unicode box-drawing** (differentiator 4): Can ship ASCII borders first, upgrade to Unicode
- **Floating elements** (differentiator 7): Works for free in rendering, interaction can wait
- **Auto Clay IDs** (differentiator 1): CLAY_AUTO_ID() exists as a workaround
- **Error handler integration** (differentiator 6): Clay prints to stderr by default, good enough for v1

---

## Terminal-Specific Concerns

These cross-cutting concerns affect multiple features and need consistent handling.

### Cell Pixel Size Convention

Clay works in pixels. Terminals work in cells. The "cell pixel size" is the bridge constant.

| Source | Default Cell Size | Notes |
|--------|------------------|-------|
| Clay ANSI terminal renderer | 16x16 (square) | Incorrect for most terminals, causes aspect ratio distortion |
| Clay termbox2 renderer | 9x21 (measured on Debian 12) | Correct non-square ratio, much better layout |
| Typical terminal | ~8-10 wide x 18-24 tall | Varies by font. Non-square is the norm. |

**Recommendation:** Default to 9x21 (termbox2's measured value). Make configurable. Non-square cells are critical -- the ANSI renderer's square assumption makes layouts look squished.

### Color Mapping Strategy

| Color Mode | Colors Available | Strategy |
|------------|-----------------|----------|
| 8-color (ncurses base) | 8 (BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE) | Nearest-color LUT (Euclidean distance in RGB space). Penalize pure black/white like termbox2 does. |
| 16-color (with BOLD/BRIGHT) | 16 | Same LUT with bright variants |
| 256-color (if supported) | 256 (16 standard + 216 cube + 24 gray) | 6x6x6 color cube mapping |
| True color (future) | 16M | Direct RGB passthrough |

**Recommendation:** Ship 8/16-color LUT in v1 behind a function pointer. The termbox2 renderer's `clay_tb_color_convert()` is the reference implementation.

### Scissor/Clipping in Terminal

Clay uses SCISSOR_START/SCISSOR_END to clip content (especially scroll containers). In terminal:

- Track a `scissor_box` (cell coordinates)
- Before drawing any cell, check if `(x, y)` is inside the scissor box
- SCISSOR_END restores to full terminal bounds

Both existing Clay terminal renderers implement this identically. Follow the same pattern.

### Text Rendering Specifics

- `Clay_TextRenderData.stringContents` is NOT null-terminated (it's a `Clay_StringSlice`)
- Multi-line text: Clay breaks text into individual line commands via the text measurement callback
- Wide characters (CJK): must use `wcwidth()` to get correct cell count (2 cells per wide char)
- The text render command gives a bounding box -- position text at `(box.x, box.y)` in cell space

---

## Sources

- **Clay v0.14 header** (direct analysis): `/home/cachy/workspaces/libs/clay/clay.h` -- All types, macros, API functions, render command structures
- **Clay ANSI terminal renderer** (direct analysis): `/home/cachy/workspaces/libs/clay/renderers/terminal/clay_renderer_terminal_ansi.c` -- Simple terminal rendering, square cell assumption, basic text/rect/border/scissor
- **Clay termbox2 terminal renderer** (direct analysis): `/home/cachy/workspaces/libs/clay/renderers/termbox2/clay_renderer_termbox2.c` -- Production-quality terminal rendering with non-square cells, color mode support, Unicode borders, transparency, image rendering, scissor clipping
- **Clay termbox2 demo** (direct analysis): `/home/cachy/workspaces/libs/clay/examples/termbox2-demo/main.c` -- Real-world Clay terminal usage patterns, cell-based sizing, floating elements, scroll containers
- **CELS v0.1 API** (direct analysis): `/home/cachy/workspaces/libs/cels/include/cels/cels.h` -- Full macro DSL, Feature/Provider system, Module pattern, composition model
- **cels-ncurses module** (direct analysis): All headers and sources in `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/` -- TUI_Engine pattern, renderer provider registration, color pair conventions, window state
- **CELS demo app** (direct analysis): `/home/cachy/workspaces/libs/cels/examples/app.c` -- Real CELS application showing compositions, state, systems, Feature/Provider wiring
- **cels-clay PROJECT.md** (direct analysis): `/home/cachy/workspaces/libs/cels/modules/cels-clay/.planning/PROJECT.md` -- Constraints, scope, architecture decisions
