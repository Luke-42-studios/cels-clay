/*
 * ClayButton — Compose-style button widget for cels-clay-widgets.
 *
 * ClayButton is the canonical reactive button widget. Each instance:
 *   - Auto-attaches its own ClayButtonState component (default: all false)
 *     AND subscribes to per-entity changes in one call via cel_state.
 *     Only that entity's ClayButtonState mutations re-run this composition.
 *   - Auto-attaches a ClayButtonProps component so a click dispatcher
 *     can resolve the on_click action by reading the entity's props.
 *   - Reads the active CelsTheme reactively (cel_watch) to resolve colors
 *     through the 3-level precedence chain:
 *       per-widget prop > active theme slot > compiled-in fallback
 *   - Renders a ClayRow containing a ClayText.
 *     bg responds to cel.selected; fg responds to cel.done.
 *
 * Usage:
 *
 *   CEL_Action(on_play_click, self) { (void)self; cels_quit(); }
 *
 *   CEL_Compose(MyMenu) {
 *       ClayButton(.label = "Play", .on_click = on_play_click) {}
 *       ClayButton(.label = "Quit", .on_click = on_quit_click) {}
 *   }
 *
 * .on_click is a `const cels_action_t*` — the registry slot pointer
 * emitted by CEL_Action. Bare identifier, no `&`. See <cels/cel_action.h>.
 * This is a hot-reload constraint: slot pointers survive .so swaps because
 * the registry overwrites the slot's .fn in place; the address is stable.
 *
 * Mutating one button from a system:
 *
 *   cel_mutate(entity, ClayButtonState) {
 *       ClayButtonState->pressed = true;
 *   }
 *
 * (.selected and .done are parent-controlled props — pass them at the
 *  ClayButton(...) call site, not by mutating ClayButtonState.)
 *
 * Theme override — per-widget prop wins over active theme:
 *
 *   ClayButton(.label = "OK", .bg_normal = (cels_color_t){20,20,50,255}) {}
 *   ClayButton(.label = "OK") {}   // fully theme-resolved
 */

#ifndef CLAY_BUTTON_H
#define CLAY_BUTTON_H

#include <cels/cels.h>
#include <cels-theme/cels_theme.h>       /* cels_color_t, CELS_DEFAULT_COLOR, CelsTheme_Active */
#include <cels-clay/clay_primitives.h>   /* ClayRow, ClayText, nucleus_padding_t */
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * ClayButtonState — purely-internal per-entity state mutated by input systems
 * ============================================================================
 *
 * Only .pressed is owned by the widget: input systems flip it on mouse-down
 * and mouse-up via cel_mutate(entity, ClayButtonState). All other visual state
 * (.selected, .done) is parent-controlled and flows through ClayButtonProps
 * instead — read those from cel.selected / cel.done in the render path.
 *
 * Default (.pressed = false) is applied exactly once per entity by cel_state
 * inside ClayButton's composition body; subsequent compositions preserve
 * whatever the system last wrote.
 */
typedef struct ClayButtonState {
    bool pressed;
} ClayButtonState;

extern cels_entity_t ClayButtonState_id;
extern void ClayButtonState_register(void);

/* ============================================================================
 * ClayButtonProps — props attached as a component on each button entity
 * ============================================================================
 *
 * Carries the label + on_click + visual overrides so a downstream click
 * dispatcher can read them via cels_entity_get_component(...) on the
 * clicked entity and resolve the action.
 *
 * on_click is a registry slot pointer (CEL_Action), not a raw function
 * pointer — it survives .so hot-reload because cels_action_register
 * overwrites the slot's .fn in place, leaving the slot address stable.
 * Dispatchers should invoke via cel_action_invoke, which is null-safe
 * on both the slot pointer and its .fn.
 *
 * Color fields use cels_color_t. Pass CELS_DEFAULT_COLOR (or omit and
 * auto-seed — see auto-seed note in clay_button.c) to resolve from the
 * active theme.
 */
typedef struct ClayButtonProps {
    const char*           label;
    const cels_action_t*  on_click;   /* slot pointer — survives .so swap */
    uint16_t              font_size;
    nucleus_padding_t     padding;
    cels_color_t          bg_normal;
    cels_color_t          bg_selected;
    cels_color_t          fg_normal;
    cels_color_t          fg_done;
} ClayButtonProps;

extern cels_entity_t ClayButtonProps_id;
extern void ClayButtonProps_register(void);

/* ============================================================================
 * Compiled-in fallback constants (third-tier — used only when no theme active)
 * ============================================================================
 *
 * These are the last-resort defaults. The resolution chain is:
 *   1. Per-widget prop (if not CELS_DEFAULT_COLOR)
 *   2. Active CelsTheme slot (if theme is loaded and slot is set)
 *   3. CLAY_BUTTON_DEFAULT_* (these constants)
 *
 * Defined in clay_button.c; declared here for consumers that want to
 * reference the compiled-in fallback values.
 */
extern const cels_color_t CLAY_BUTTON_DEFAULT_BG_NORMAL;
extern const cels_color_t CLAY_BUTTON_DEFAULT_BG_SELECTED;
extern const cels_color_t CLAY_BUTTON_DEFAULT_FG_NORMAL;
extern const cels_color_t CLAY_BUTTON_DEFAULT_FG_DONE;

/* ============================================================================
 * ClayButton composition
 * ============================================================================ */

CEL_Define_Composition(ClayButton,
    const char*          label;
    const cels_action_t* on_click;     /* slot pointer — survives .so swap */
    uint16_t             font_size;
    nucleus_padding_t    padding;
    cels_color_t         bg_normal;
    cels_color_t         bg_selected;
    cels_color_t         fg_normal;
    cels_color_t         fg_done;
    /* Compose-style controlled props. The parent passes the desired visual
     * state every recomposition; the button's body writes them into
     * ClayButtonState (preserving .pressed for hover/click systems). Reads from
     * cel.selected / cel.done for the bg/fg so the visual is a pure
     * function of props. Default false. */
    bool selected;
    bool done;
#ifndef NDEBUG
    /* debug_name is non-persistent, debug-only, and stripped from release
     * builds (NDEBUG). Do NOT use it as a stable identifier — it is not
     * persisted, not serialised, and not visible to any system outside this
     * composition's render pass. Its sole purpose is to label the layout
     * element in debug tooling / layout inspectors.
     *
     * Usage: ClayButton(.label = "Play", .debug_name = "btn_play") */
    const char* debug_name;
#endif
);

#define ClayButton(...) cel_init(ClayButton, __VA_ARGS__)

#endif /* CLAY_BUTTON_H */
