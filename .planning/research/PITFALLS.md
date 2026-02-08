# Domain Pitfalls: cels-clay

**Domain:** Clay+ECS hybrid UI module (CELS reactive framework + Clay immediate-mode layout)
**Researched:** 2026-02-07
**Overall Confidence:** HIGH (analysis grounded in Clay source code inspection and CELS codebase)

---

## Critical Pitfalls

Mistakes that cause rewrites, silent rendering corruption, or fundamental architecture failures.

---

### P-01: CLAY() Macro Uses a Shared Static Latch -- Nesting Inside Another `for`-Loop Macro Shadows It

**What goes wrong:** The `CLAY()` macro expands to a `for` loop that uses a file-scope `static uint8_t CLAY__ELEMENT_DEFINITION_LATCH` variable as its loop counter (clay.h line 104). If `CEL_Clay()` is also implemented as a `for`-loop macro (which is the standard CELS composition pattern -- see `_CEL_ComposeBlock` at cels.h line 650-667), and the user nests `CLAY()` inside `CEL_Clay()`, both for-loops share the same `CLAY__ELEMENT_DEFINITION_LATCH` variable. The inner `CLAY()` sets the latch to 1 during its increment step, which breaks the outer loop's iteration if it was also using the same latch.

**Why it happens:** The `CLAY()` macro at clay.h:143-148 is:
```c
#define CLAY(id, ...)                                                   \
    for (                                                               \
        CLAY__ELEMENT_DEFINITION_LATCH = (..., 0);                      \
        CLAY__ELEMENT_DEFINITION_LATCH < 1;                             \
        CLAY__ELEMENT_DEFINITION_LATCH=1, Clay__CloseElement()          \
    )
```
The latch is a single `static uint8_t` per translation unit. Nested `CLAY()` calls work because the inner loop completes (setting latch to 1) and then the outer loop's condition re-evaluates -- but this only works because `CLAY()` resets the latch to 0 in its init expression. If `CEL_Clay()` also uses this latch variable as part of its own for-loop construct, the two macros will corrupt each other's iteration state.

**Consequences:**
- `Clay__CloseElement()` called wrong number of times
- `CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE` error at `Clay_EndLayout()` (clay.h:4258-4262)
- Silent tree corruption: children attached to wrong parents
- Extremely confusing because the bug is invisible at the macro call site

**Warning signs:**
- Clay error handler fires with "unbalanced open/close" errors
- Layout tree has wrong nesting structure
- Render commands show elements at wrong positions

**Prevention:**
1. `CEL_Clay()` MUST NOT use `CLAY__ELEMENT_DEFINITION_LATCH` as its loop variable. Use a `__COUNTER__`-based unique variable name (same pattern as `_CEL_ComposeBlock` which uses `_CEL_CAT(_cel_b_, __COUNTER__)`).
2. `CEL_Clay()` should be a `for`-loop macro that manages its own CELS entity lifecycle (begin_entity/end_entity) using a private counter, then the body contains `CLAY()` calls which use the latch independently.
3. Test case: nest `CLAY()` three levels deep inside `CEL_Clay()` to verify the latch resets correctly.
4. NEVER wrap `CLAY()` inside another macro that also expands to a `for` loop using the same latch. Instead, `CEL_Clay()` should call `cels_begin_entity()`/`cels_end_entity()` in its own loop construct, and the user writes `CLAY()` calls in the body.

**Detection:** Static analysis cannot catch this. Write a unit test that declares `CEL_Clay() { CLAY(id, ...) { CLAY(id2, ...) { } } }` and verify the element tree depth matches expectations.

**Which phase should address it:** Phase 1 (CEL_Clay macro design). This is a design-time decision, not a runtime fix.

**Confidence:** HIGH -- verified by reading clay.h lines 104, 136-148.

---

### P-02: Clay Tree Ordering Requires Deterministic Parent-Before-Child Declaration -- ECS Iteration Order Is Not Guaranteed

**What goes wrong:** Clay builds its layout tree through the order of `Clay__OpenElement()` / `Clay__CloseElement()` calls. Parent elements MUST be opened before children and closed after them -- this is enforced by the macro's `for` loop structure. However, if multiple CELS compositions each contribute `CLAY()` subtrees to the same global Clay tree, the order those compositions are iterated by ECS determines the tree structure. ECS (flecs) does not guarantee iteration order of entities. If Composition A (sidebar) iterates after Composition B (content), the sidebar renders as a child of content instead of as a sibling.

**Why it happens:** CELS uses flecs systems to iterate entities. System iteration order depends on archetype table ordering, which depends on entity creation order and component addition patterns. This order is stable within a single run but can change across restarts, after entity deletion/creation, or after archetype changes. Clay has no "attach to parent by ID" mechanism for non-floating elements -- tree structure is purely determined by call order within BeginLayout/EndLayout.

**Consequences:**
- Layout renders correctly during development, breaks after unrelated code changes
- Adding a new composition silently reorders existing compositions
- Sidebar/content/footer end up nested instead of side-by-side
- Bug manifests as visual layout corruption, not as an error

**Warning signs:**
- Layout looks correct initially but breaks after adding new compositions
- Same code produces different layouts on different runs
- Elements appear nested when they should be siblings

**Prevention:**
1. Do NOT rely on ECS iteration order for Clay tree structure. Instead, use an explicit ordering mechanism:
   - Option A: A single "layout orchestrator" composition that calls all Clay subtrees in a defined order. Compositions register their layout functions, and the orchestrator invokes them sequentially during `BeginLayout`/`EndLayout`.
   - Option B: Add a `clay_order` component with an integer priority. Sort compositions by this value before calling their layout functions.
   - Option C (recommended): The `CEL_Clay()` scope defines where in the global tree a subtree attaches. The cels-clay frame system calls `Clay_BeginLayout()`, then iterates compositions in priority order, then calls `Clay_EndLayout()`. This means one system, one iteration, deterministic order.
2. Document clearly: "Clay tree order = composition evaluation order, which must be explicit."
3. Test: create three compositions (header, sidebar, content), verify they render as siblings regardless of entity creation order.

**Detection:** Add a debug assertion that logs the order of `CLAY()` root elements each frame and asserts it matches a declared order.

**Which phase should address it:** Phase 1/2 (core architecture). The frame pipeline design must solve this before any composition can declare CLAY blocks.

**Confidence:** HIGH -- Clay tree ordering verified in source (clay.h:1844, the `openLayoutElementStack` pushes children in call order). Flecs iteration order non-determinism is documented in flecs docs.

---

### P-03: Clay_BeginLayout/EndLayout Must Bracket ALL CLAY() Calls -- Scattered Across CELS Systems Causes Unbalanced Trees

**What goes wrong:** `Clay_BeginLayout()` initializes ephemeral memory and opens a root container (clay.h:4216-4233). `Clay_EndLayout()` closes it and runs layout calculation (clay.h:4236-4266). ALL `CLAY()` macro calls must occur between these two calls, in a single thread. If CELS systems that declare CLAY blocks run at different ECS pipeline phases (e.g., one at `OnUpdate`, another at `OnStore`), the `CLAY()` calls happen outside the `BeginLayout`/`EndLayout` window, causing undefined behavior.

**Why it happens:** CELS Feature/Provider systems run at specific pipeline phases (`CELS_Phase_OnStore`, etc.). If the Clay layout system (which calls `BeginLayout`/`EndLayout`) runs at one phase, but composition evaluation happens at a different phase, the CLAY macros fire when Clay's internal state is not initialized for the current frame.

**Consequences:**
- `Clay__OpenElement()` called before `Clay_BeginLayout()` -- accesses uninitialized/stale ephemeral memory
- Crash or silent memory corruption in Clay's internal arrays
- `Clay_EndLayout()` produces garbage render commands

**Warning signs:**
- Segfault inside `Clay__OpenElement` or `Clay__ConfigureOpenElement`
- `CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED` error despite adequate arena size
- Render commands contain NaN bounding boxes

**Prevention:**
1. cels-clay must own the entire `BeginLayout -> CLAY() declarations -> EndLayout` sequence in a single CELS system callback.
2. Compositions do NOT call `CLAY()` directly from their ECS system iteration. Instead, they register a layout callback (function pointer) that the cels-clay frame system invokes between `BeginLayout` and `EndLayout`.
3. The cels-clay system runs at a single well-defined ECS phase (e.g., `OnStore`), and it is the ONLY system that touches Clay's API.
4. Assert at runtime: if any `CLAY()` macro is called outside the `BeginLayout`/`EndLayout` window, log an error. (Clay already does this implicitly -- `Clay__OpenElement` will access `context->layoutElements` which is reset in `Clay__InitializeEphemeralMemory`.)

**Detection:** Clay's error handler will fire with `CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED` or crash. But the root cause will be obscure without understanding the phase ordering.

**Which phase should address it:** Phase 1/2 (frame pipeline design). This is architectural.

**Confidence:** HIGH -- verified by reading `Clay_BeginLayout` (line 4218: resets ephemeral memory) and `Clay__OpenElement` (line 2003: checks capacity of `layoutElements` which is ephemeral).

---

### P-04: CLAY_ID() Only Works with String Literals -- CEL_Clay() Cannot Generate Dynamic IDs Through It

**What goes wrong:** `CLAY_ID("label")` expands to `CLAY_SID(CLAY_STRING("label"))`. `CLAY_STRING` uses `CLAY__ENSURE_STRING_LITERAL(x)` which expands to `("" x "")` -- string literal concatenation that fails at compile time if `x` is not a literal (clay.h:97-100). This means `CEL_Clay()` cannot programmatically generate Clay IDs based on composition names or entity IDs by passing a variable to `CLAY_ID()`.

**Why it happens:** Clay intentionally enforces string literals for `CLAY_ID()` and `CLAY_STRING()` to guarantee the string data lives for the program's lifetime (`.isStaticallyAllocated = true`). Dynamic strings would require lifetime management that Clay avoids.

**Consequences:**
- If `CEL_Clay()` tries to generate unique Clay IDs automatically (e.g., `CLAY_ID(composition_name)`), it fails to compile
- Developers must manually assign unique `CLAY_ID()` strings to every element across all compositions
- ID collisions across independently-authored compositions cause `CLAY_ERROR_TYPE_DUPLICATE_ID`

**Warning signs:**
- Compile error: "expected string literal" inside `CLAY_ID()`
- At runtime: Clay error handler fires with `CLAY_ERROR_TYPE_DUPLICATE_ID` when two compositions use the same ID string

**Prevention:**
1. Use `CLAY_SID()` (which accepts a `Clay_String`, not a literal) for programmatic ID generation. Build a `Clay_String` manually with `.isStaticallyAllocated = false`.
2. Use `CLAY_ID_LOCAL("name")` which hashes the string with the parent element's ID as seed (clay.h:86-88), providing natural scoping within a CLAY subtree.
3. For composition-scoped uniqueness, have `CEL_Clay()` open a root `CLAY()` element with a composition-specific ID, then all child elements use `CLAY_ID_LOCAL()` to scope within it.
4. For loops inside compositions, use `CLAY_IDI("name", index)` which combines the string hash with an integer index (clay.h:81).
5. Document the ID strategy: "Each CEL_Clay composition gets a root CLAY element with CLAY_ID("CompositionName"), children use CLAY_ID_LOCAL."

**Detection:** Clay fires `CLAY_ERROR_TYPE_DUPLICATE_ID` at runtime. Wire the Clay error handler to CELS logging.

**Which phase should address it:** Phase 1 (CEL_Clay macro API design). The ID strategy must be designed before compositions can be authored.

**Confidence:** HIGH -- verified by reading clay.h lines 76, 97, 100. The `CLAY__ENSURE_STRING_LITERAL` macro will cause compile errors with non-literals.

---

### P-05: Clay's CLAY_IMPLEMENTATION Must Be Defined in Exactly One Translation Unit -- INTERFACE Library Pattern Creates Multiple

**What goes wrong:** Clay is a single-header library. `#define CLAY_IMPLEMENTATION` before `#include "clay.h"` emits the implementation (all function bodies). If cels-clay is an INTERFACE library (like cels-ncurses), its sources compile in the consumer's context. If multiple consumers include cels-clay, or if multiple source files in a single consumer define `CLAY_IMPLEMENTATION`, the linker gets multiple definitions of Clay functions.

**Why it happens:** The INTERFACE library pattern compiles source files in the consumer's translation context. Each consumer that links cels-clay gets the source files compiled into its own binary. If two consumers link cels-clay, each gets a separate copy of Clay's implementation -- which is correct for separate binaries but breaks for shared libraries or when both consumers link into the same executable.

**Consequences:**
- Linker error: "multiple definition of `Clay_Initialize`" (etc.)
- Or worse: two separate Clay contexts exist, one per translation unit, and calls to `CLAY()` go to the wrong context
- If using `Clay_SetCurrentContext()`, the wrong context might be active

**Warning signs:**
- Linker errors about duplicate symbols when adding a second source file
- Clay state (arena, elements) appears reset between function calls in different files

**Prevention:**
1. cels-clay must have exactly ONE `.c` file that defines `CLAY_IMPLEMENTATION` and includes `clay.h`. All other files include `clay.h` WITHOUT `CLAY_IMPLEMENTATION`.
2. The INTERFACE library pattern works: the implementation `.c` file is listed in `target_sources(cels-clay INTERFACE ...)`, and the header-only include is provided via `target_include_directories`.
3. Do NOT put `#define CLAY_IMPLEMENTATION` in a header file that might be included by multiple translation units.
4. Test: build a project with two `.c` files that both use `CLAY()` macros. Verify no linker errors.
5. Consider using CMake's `FetchContent` to download clay.h and place it in a known path, then have the implementation `.c` file reference that path.

**Detection:** Linker errors are immediate and obvious. The subtler bug (two Clay contexts) manifests as layout corruption.

**Which phase should address it:** Phase 1 (CMake/build setup). Must be correct before any code compiles.

**Confidence:** HIGH -- standard single-header library integration pattern. Clay's README confirms: "defining CLAY_IMPLEMENTATION in one file" (line 23-24).

---

## Moderate Pitfalls

Mistakes that cause delays, visual bugs, or technical debt.

---

### P-06: Float-to-Cell Quantization Causes Gaps and Overlaps in Terminal Rendering

**What goes wrong:** Clay computes layout in floating-point pixel coordinates. Terminal cells are integer grid positions. Converting float bounding boxes to cell positions requires rounding, and different rounding strategies produce different artifacts:
- **Truncation** (`(int)x`): elements shrink, creating 1-cell gaps between siblings
- **Rounding** (`roundf(x)`): adjacent elements can overlap by 1 cell when one rounds up and the other rounds down
- **Inconsistent width**: a box at `x=1.2` with `width=1.4` cells should occupy either cells [1,2] or cells [1,2,3] -- the right answer depends on whether you round x and width independently or compute `right = round(x + width) - round(x)`

**Why it happens:** Clay was designed for pixel rendering where sub-pixel differences are invisible. In terminal rendering, every coordinate maps to a discrete cell, and 0.5-cell rounding errors are visually obvious.

**Consequences:**
- 1-cell gaps between sidebar and content area (most visible artifact)
- Borders overlap with content
- Text starts 1 cell too far left/right
- Layout looks broken at certain terminal widths but fine at others

**Prevention:**
1. Study the termbox2 renderer's `cell_snap_bounding_box` function (clay_renderer_termbox2.c:604-614). It computes width as `round(x + width) - round(x)`, not `round(width)`. This is critical -- it prevents gaps by ensuring adjacent elements' cell boundaries align.
2. Use a consistent pixel-to-cell conversion: define `cell_width` and `cell_height` constants (e.g., 9x21 pixels/cell as in termbox2), multiply terminal dimensions by these to get Clay's pixel dimensions, then divide back when converting render commands.
3. Test at multiple terminal sizes (80x24, 120x40, 200x60) to catch size-dependent rounding artifacts.
4. For borders: Clay reports border widths in pixels. A 1-pixel border rounds to 0 cells. Use `CLAY__MAX(1, round(border_width / cell_width))` to ensure borders are always at least 1 cell.

**Detection:** Visual inspection at different terminal sizes. Write a test that renders a sidebar + content and asserts their cell widths sum to the terminal width.

**Which phase should address it:** Renderer implementation phase (the ncurses Clay renderer).

**Confidence:** HIGH -- verified by reading the termbox2 renderer source code. The `cell_snap_bounding_box` vs `cell_snap_pos_ind_bounding_box` distinction (lines 604-633) exists specifically because of this problem.

---

### P-07: Clay Text Measurement for Terminals Must Account for Wide Characters (CJK, Emoji)

**What goes wrong:** Clay calls the user-provided `MeasureText` function to determine text dimensions for wrapping and layout. A naive implementation (`text.length * cell_width`) is wrong for terminals because:
- ASCII characters are 1 cell wide
- CJK characters are 2 cells wide
- Emoji may be 1 or 2 cells wide
- Combining characters (accents) are 0 cells wide
- UTF-8 encoding means `text.length` (in bytes) != number of characters != display width

**Why it happens:** Clay's `Clay_StringSlice` provides `.length` in bytes and `.chars` as a non-null-terminated byte pointer. The termbox2 renderer correctly decodes UTF-8 to codepoints and uses `wcwidth()` to get display width per codepoint (clay_renderer_termbox2.c:1206-1235). A simpler implementation that skips this will produce incorrect layout for any non-ASCII text.

**Consequences:**
- Text overflows its container (measured width too small)
- Text wraps prematurely (measured width too large)
- Layout is correct in English but broken for other languages
- Emoji in labels cause misalignment

**Prevention:**
1. The `MeasureText` callback must: decode UTF-8 bytes to codepoints, call `wcwidth()` (or ncurses `wcswidth()`) for each codepoint, sum the display widths, multiply by cell_width.
2. Use `setlocale(LC_ALL, "")` at program start so `wcwidth()` returns correct values for the current locale.
3. `Clay_StringSlice.chars` is NOT null-terminated. Copy to a buffer or use length-aware decoding.
4. Test with a string containing ASCII, CJK, and emoji characters.
5. Keep the MeasureText function fast -- Clay caches results but calls it frequently during initial layout. Avoid allocation in the hot path.

**Detection:** Render a label containing a wide character (e.g., Chinese text). If it overflows, the measurement is wrong.

**Which phase should address it:** Renderer implementation phase (MeasureText callback).

**Confidence:** HIGH -- verified by reading Clay's MeasureText API (clay.h:580-586, note "this will only work for monospace fonts") and the termbox2 renderer's correct implementation (lines 1206-1235).

---

### P-08: Clay Arena Capacity Exceeded at Runtime -- Silent Degradation, Not a Crash

**What goes wrong:** When Clay runs out of element capacity, `Clay__OpenElement()` sets `context->booleanWarnings.maxElementsExceeded = true` and returns early (clay.h:2003-2005). Subsequent `CLAY()` calls become no-ops -- elements are silently dropped. `Clay__CloseElement()` also returns early (clay.h:1817-1818). The layout completes without error, but the render command array is incomplete -- parts of the UI simply vanish.

**Why it happens:** Clay defaults to a max element count (configurable via `Clay_SetMaxElementCount()`). For simple UIs this is sufficient, but as compositions grow (especially with loops generating many elements), the limit is hit. Clay's error handler fires with `CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED`, but if the error handler only logs to stderr (or is ignored), the user sees a truncated UI without understanding why.

**Consequences:**
- UI partially renders -- some elements missing, no crash
- Hard to debug because the missing elements are silently dropped
- Appears as a "rendering bug" when it's actually a capacity issue
- The error handler fires once per frame, flooding logs

**Prevention:**
1. Wire Clay's `Clay_ErrorHandler` to CELS's logging/error system. Make capacity errors LOUD (e.g., render a visible error overlay).
2. Call `Clay_SetMaxElementCount()` with a generous value at startup. For terminal UIs with relatively few elements, 2048 or 4096 is safe. Monitor with a frame counter.
3. Add a debug mode that logs the element count each frame: `context->layoutElements.length` after `EndLayout()`.
4. Consider a startup assertion: if the number of compositions * estimated elements per composition > max element count, warn at init time.
5. Remember: `Clay_SetMaxElementCount()` must be called BEFORE `Clay_MinMemorySize()` and `Clay_Initialize()` -- it changes the arena size requirement.

**Detection:** Clay error handler callback with `CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED`. Also visible as partial UI rendering.

**Which phase should address it:** Phase 1 (Clay initialization). Set up the error handler and capacity early.

**Confidence:** HIGH -- verified by reading `Clay__OpenElement` (line 2003) and the error handling path.

---

### P-09: CELS Feature/Provider Callback Runs After Composition Evaluation -- Render Commands Must Already Exist

**What goes wrong:** In CELS, a Feature/Provider callback (e.g., `CEL_Provides(TUI, Renderable, ClayCanvas, render_callback)`) runs as a flecs system during the ECS pipeline. The Clay render commands (`Clay_RenderCommandArray`) must be fully computed before this callback fires. If the Clay layout system and the rendering system run at the same pipeline phase, flecs does not guarantee which runs first. The renderer may attempt to iterate render commands that don't exist yet.

**Why it happens:** CELS providers use `_CEL_Provides()` which calls `cels_provider_register()` with a phase (e.g., `CELS_Phase_OnStore`). The Clay frame system (which runs `BeginLayout` -> compositions -> `EndLayout`) and the renderer provider must run at different phases or have an explicit ordering dependency.

**Consequences:**
- Renderer iterates an empty or stale `Clay_RenderCommandArray`
- Screen flickers: alternating between last frame's commands and current frame's commands
- Renderer crashes if `renderCommands.internalArray` is NULL (between arena resets)

**Warning signs:**
- Blank frames interspersed with correct frames
- Screen flickers on every other frame
- Render callback receives zero-length command array

**Prevention:**
1. The Clay frame system (BeginLayout/EndLayout) must run at an earlier phase than the renderer. For example, Clay layout at `CELS_Phase_OnUpdate`, renderer at `CELS_Phase_OnStore`.
2. Store the `Clay_RenderCommandArray` returned by `Clay_EndLayout()` in a module-level variable that the renderer reads. The renderer's provider callback reads this variable.
3. The `Clay_RenderCommandArray` points into Clay's internal arena. It is valid until the NEXT call to `Clay_BeginLayout()` (which resets ephemeral memory). So the renderer must consume it before the next frame's `BeginLayout`.
4. Document the phase ordering contract: "Clay layout runs at Phase X, renderer runs at Phase Y where Y > X."

**Detection:** If the renderer gets a zero-length command array on the first frame (before any layout has run), this indicates ordering is wrong.

**Which phase should address it:** Phase 2 (frame pipeline integration). The phase ordering must be explicit.

**Confidence:** HIGH -- verified by reading `Clay__InitializeEphemeralMemory` (line 2184) which resets `renderCommands` at the start of each `BeginLayout`.

---

### P-10: Clay_RenderCommandArray Pointers Are Ephemeral -- Storing References Across Frames Causes Use-After-Free

**What goes wrong:** `Clay_EndLayout()` returns a `Clay_RenderCommandArray` whose `internalArray` points into Clay's internal arena (clay.h:2213 -- `renderCommands` is ephemeral). The next call to `Clay_BeginLayout()` resets this arena (clay.h:2188 -- `arena->nextAllocation = context->arenaResetOffset`). Any pointer into the render command array, or any pointer to config data within a render command, becomes dangling after the next `BeginLayout`.

**Why it happens:** Clay uses a frame-reset arena allocator. All ephemeral data (layout elements, configs, render commands) lives in this arena and is invalidated every frame. This is efficient but means you cannot cache render command pointers.

**Consequences:**
- If the renderer stores `Clay_RenderCommand*` pointers for retained-mode rendering, they become dangling next frame
- If text or config pointers from render commands are saved for later use, they point to recycled memory
- Intermittent corruption: the memory is reused, not freed, so the data looks valid but is wrong

**Prevention:**
1. Consume the entire `Clay_RenderCommandArray` within the same frame it was produced. Do not store pointers into it.
2. If retained-mode rendering is needed, COPY the render command data (bounding boxes, colors, text) into your own persistent storage.
3. `Clay_String` within render commands: if `isStaticallyAllocated == false`, the chars pointer may also be ephemeral. Copy the text data.
4. The ncurses renderer should process all commands immediately in its callback and not retain references.

**Detection:** ASan/Valgrind will catch use-after-free if the arena memory is actually reused. Without sanitizers, the bug manifests as garbled text or wrong colors that change each frame.

**Which phase should address it:** Renderer implementation phase. Enforce "process immediately, retain nothing" in the renderer design.

**Confidence:** HIGH -- verified by reading `Clay__InitializeEphemeralMemory` (line 2188: arena reset).

---

## Minor Pitfalls

Mistakes that cause annoyance but are fixable.

---

### P-11: Clay CMakeLists.txt Does Not Export an INTERFACE Target -- FetchContent Requires Manual Setup

**What goes wrong:** Clay's `CMakeLists.txt` has the library target creation commented out (lines 61-62: `#add_library(${PROJECT_NAME} INTERFACE)`). Using `FetchContent_Declare` + `FetchContent_MakeAvailable` will not produce a `clay` target that cels-clay can `target_link_libraries` against.

**Why it happens:** Clay is primarily distributed as a single `clay.h` file meant to be copied into your project. The CMake support is secondary and not fully integrated.

**Prevention:**
1. After `FetchContent_MakeAvailable(clay)`, manually create an INTERFACE target:
   ```cmake
   add_library(clay_lib INTERFACE)
   target_include_directories(clay_lib INTERFACE ${clay_SOURCE_DIR})
   ```
2. Or skip FetchContent entirely and use `file(DOWNLOAD ...)` to get `clay.h` directly.
3. The cels-clay implementation `.c` file handles `#define CLAY_IMPLEMENTATION` -- the INTERFACE target only needs the include path.

**Detection:** CMake configure error: "target clay not found" or similar.

**Which phase should address it:** Phase 1 (CMake setup). Straightforward to handle.

**Confidence:** HIGH -- verified by reading Clay's CMakeLists.txt (lines 61-62 are commented out).

---

### P-12: CLAY_STRING() Requires String Literals -- Dynamic Text From CELS State Requires CLAY_STRING_DEFAULT or Manual Construction

**What goes wrong:** `CLAY_TEXT(CLAY_STRING(variable), config)` fails to compile because `CLAY_STRING()` enforces string literals via `CLAY__ENSURE_STRING_LITERAL` (the `"" x ""` concatenation trick, clay.h:97). CELS compositions that want to display dynamic text (state values, computed labels) cannot use `CLAY_STRING()`.

**Why it happens:** Clay wants statically allocated strings to avoid lifetime management. Dynamic strings need manual `Clay_String` construction.

**Prevention:**
1. For dynamic text, construct `Clay_String` manually:
   ```c
   Clay_String dynamic = { .isStaticallyAllocated = false, .length = strlen(buf), .chars = buf };
   ```
2. Ensure the buffer (`buf`) lives at least until `Clay_EndLayout()` is called. Stack buffers in the composition function are fine since the function doesn't return until after `EndLayout`.
3. Consider providing a `CELS_CLAY_STRING(ptr, len)` convenience macro in cels-clay.
4. `isStaticallyAllocated = false` tells Clay the string may not persist. Clay will copy it to its internal dynamic string data array if needed for text wrapping/measurement caching.

**Detection:** Compile error inside `CLAY_STRING()` macro.

**Which phase should address it:** Phase 1 (API design). Provide a dynamic string helper.

**Confidence:** HIGH -- verified in clay.h lines 97-100.

---

### P-13: Border Rendering in Terminal Cells -- 1-Pixel Borders Round to Zero Cells

**What goes wrong:** Clay's border widths are specified in pixels. In terminal rendering, a 1-pixel border rounds to 0 cells (`round(1.0 / 9.0) = 0`), making the border invisible. Similarly, corner radius values have no meaning in cell-based rendering.

**Why it happens:** Clay's border model assumes sub-pixel precision. Terminal cells are large (typically 9x21 pixels). Any border width less than `cell_width / 2` rounds to zero.

**Prevention:**
1. Use `CLAY__MAX(1, round(width / cell_width))` for border widths, as the termbox2 renderer does with `CLAY_TB_BORDER_MODE_MINIMUM` (clay_renderer_termbox2.c:1496-1509).
2. Ignore `cornerRadius` entirely in the terminal renderer -- cells cannot represent rounded corners. (Could approximate with Unicode box-drawing corner characters.)
3. Design Clay styles for terminal rendering with border widths >= `cell_width` (e.g., if cell_width=9, use border width >= 9 for a 1-cell border).
4. Alternatively, define border widths in cell units and multiply by cell_size before passing to Clay.

**Detection:** Borders don't appear. Visual inspection.

**Which phase should address it:** Renderer implementation phase.

**Confidence:** HIGH -- verified in termbox2 renderer border handling (lines 1474-1612).

---

### P-14: Clay's Single-Threaded Constraint -- cels-clay Must Not Allow Parallel Composition Evaluation

**What goes wrong:** Clay's documentation states: "Do not render instances across different threads simultaneously" (README line 516-517). Clay uses global/context state (`Clay_GetCurrentContext()`) that is not thread-safe. If CELS ever evaluates compositions in parallel (e.g., worker threads), `CLAY()` calls from different threads will corrupt shared Clay state.

**Why it happens:** Clay's internal state (element arrays, open element stack, latch variable) is per-context but not protected by synchronization. Even with separate `Clay_Context` instances, the `static uint8_t CLAY__ELEMENT_DEFINITION_LATCH` variable at clay.h:104 is shared across all contexts within a translation unit.

**Prevention:**
1. cels-clay's frame system must run on a single thread. All composition layout callbacks must execute sequentially.
2. If CELS v0.2 introduces parallel system execution, mark the Clay frame system as non-parallelizable.
3. The `CLAY__ELEMENT_DEFINITION_LATCH` being `static` (not `_Thread_local`) means even separate Clay contexts in separate threads will corrupt each other via the latch. This is a Clay limitation that cannot be worked around without modifying Clay's source.
4. Document: "Clay layout is single-threaded. Never call CLAY() from a thread other than the main render thread."

**Detection:** Data races visible under ThreadSanitizer (TSan). Without sanitizers, intermittent layout corruption.

**Which phase should address it:** Architecture documentation. Not a code change needed for v1 (single-threaded), but must be documented for future-proofing.

**Confidence:** HIGH -- verified in clay.h (static latch at line 104) and README (line 516-517).

---

### P-15: Scissor/Clip Commands From Clay Require Terminal Clipping Implementation

**What goes wrong:** Clay emits `CLAY_RENDER_COMMAND_TYPE_SCISSOR_START` and `CLAY_RENDER_COMMAND_TYPE_SCISSOR_END` render commands for scrollable/clipped containers. The terminal renderer must implement clipping by checking every cell write against the scissor rectangle. If scissor handling is skipped, scrollable containers will render their full content overflowing into neighboring elements.

**Why it happens:** ncurses does not have a built-in scissor/clipping mechanism. Each `mvwaddch()` call must be manually bounds-checked against the current scissor rectangle. This is easy to forget when initially implementing the renderer.

**Prevention:**
1. Maintain a scissor state stack (or single active scissor). On `SCISSOR_START`, push the bounding box. On `SCISSOR_END`, pop.
2. Every cell-write function must check `if (x >= scissor.x && x < scissor.x + scissor.width && y >= scissor.y && y < scissor.y + scissor.height)` before writing.
3. The termbox2 renderer implements this correctly (clay_renderer_termbox2.c:384-386, 751-756). Study and replicate for ncurses.
4. Scrollable containers are a stretch goal for v1 -- but the scissor infrastructure should be in place from the start to avoid a renderer rewrite later.

**Detection:** Scroll a container. If content appears outside the container bounds, scissor is broken.

**Which phase should address it:** Renderer implementation phase.

**Confidence:** HIGH -- verified in termbox2 renderer (lines 384-386, 751-756, 1758-1770).

---

### P-16: Module Sibling Architecture -- cels-clay Must Not Include cels-ncurses Headers

**What goes wrong:** The project constraint states cels-clay and cels-ncurses are sibling modules with no cross-dependencies. If the ncurses Clay renderer is placed inside cels-clay, it will need to `#include` cels-ncurses headers for drawing primitives. This creates a circular or unwanted dependency.

**Why it happens:** The Clay renderer needs to draw to the terminal (ncurses), but cels-clay should only know about Clay, not about ncurses. The actual rendering must live in the application layer (or in a third bridge module/file).

**Prevention:**
1. The ncurses Clay renderer is NOT part of cels-clay. It lives either:
   - In the application (simplest: the app has a `.c` file that includes both cels-clay and cels-ncurses and implements the renderer)
   - In a separate bridge module (cels-clay-ncurses) that depends on both
2. cels-clay exposes the `Clay_RenderCommandArray` via a Feature/Provider pattern. The renderer is a Provider that consumes this Feature.
3. cels-clay's public API is: `CEL_Clay()` macro, Clay initialization, layout frame management, render command output. It does NOT include any terminal-specific code.
4. The demo app wires them together: it registers a Provider callback that receives Clay render commands and calls cels-ncurses drawing functions.

**Detection:** `#include` analysis. If cels-clay's source files include any cels-ncurses headers, the architecture is violated.

**Which phase should address it:** Phase 1 (module boundary design). Must be clear before code is written.

**Confidence:** HIGH -- directly from PROJECT.md constraint: "No cross-module dependencies."

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| CEL_Clay macro design | P-01 (latch collision), P-04 (ID literals) | Use __COUNTER__ for loop var, use CLAY_SID for dynamic IDs |
| Frame pipeline | P-02 (tree ordering), P-03 (BeginLayout/EndLayout bracketing), P-09 (phase ordering) | Single orchestrator system, explicit phase ordering |
| CMake/build setup | P-05 (CLAY_IMPLEMENTATION ODR), P-11 (no Clay INTERFACE target) | One implementation TU, manual INTERFACE target |
| ncurses renderer | P-06 (quantization), P-07 (text measurement), P-13 (borders), P-15 (scissor) | Study termbox2 renderer, cell_snap_bounding_box pattern |
| Clay initialization | P-08 (capacity), P-10 (ephemeral pointers) | Generous max element count, process commands immediately |
| Module architecture | P-16 (sibling deps) | Renderer lives in app layer, not in cels-clay |
| Threading | P-14 (single-threaded) | Document constraint, mark system as non-parallel |

---

## Sources

All findings verified by direct source code inspection:

- `clay.h` -- Clay source code at `/home/cachy/workspaces/libs/clay/clay.h`
  - Macro definitions: lines 57-159
  - Error types: lines 780-798
  - Element open/close: lines 1815-2035
  - Hash functions: lines 1371-1407
  - Ephemeral memory: lines 2184-2220
  - BeginLayout/EndLayout: lines 4215-4266
- `clay_renderer_termbox2.c` -- Terminal renderer reference at `/home/cachy/workspaces/libs/clay/renderers/termbox2/clay_renderer_termbox2.c`
  - Cell snapping: lines 604-633
  - Text measurement: lines 1206-1235
  - Border rendering: lines 1474-1612
  - Scissor handling: lines 384-386, 1758-1770
- `cels.h` -- CELS framework API at `/home/cachy/workspaces/libs/cels/include/cels/cels.h`
  - ComposeBlock macro pattern: lines 650-667
  - Feature/Provider macros: lines 460-484
- `tui_renderer.c` -- CELS ncurses renderer at `/home/cachy/workspaces/libs/cels/modules/cels-ncurses/src/renderer/tui_renderer.c`
  - Provider pattern: lines 222-230
- `cels-ncurses/CMakeLists.txt` -- INTERFACE library pattern reference
- Clay README -- Known limitations, API lifecycle, multi-instance warnings
