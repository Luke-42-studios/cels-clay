/*
 * ClayButton widget implementation — cels-clay-widgets target.
 *
 * Moved from the engine's button source and renamed:
 *   OldButton       → ClayButton
 *   OldButtonProps  → ClayButtonProps
 *   ButtonState         → ClayButtonState
 *   old_color_t     → cels_color_t
 *   OLD_BUTTON_DEFAULT_* → CLAY_BUTTON_DEFAULT_*
 *
 * Theme integration (THEME-01 / D-09):
 *   Reads the active CelsTheme reactively via cel_watch(CelsTheme_Active).
 *   Each color resolves through the 3-level precedence chain:
 *     1. Per-widget prop (if cels_is_default_color() returns false)
 *     2. Active CelsTheme slot (if theme singleton is loaded)
 *     3. CLAY_BUTTON_DEFAULT_* compiled-in fallback (last resort)
 *
 * Semantic slot mapping (D-09):
 *   bg_normal   ← theme.surface
 *   bg_selected ← theme.surface_alt
 *   fg_normal   ← theme.text
 *   fg_done     ← theme.muted
 *
 * Auto-seed (decision A3):
 *   Any omitted color prop field arrives zero-initialised from the C99
 *   designated-initialiser default {0,0,0,0}. Because 0.0f is a valid
 *   color (transparent black), zero is NOT the sentinel. The composition
 *   body checks each color field on entry: if all channels are zero it
 *   replaces the field value with CELS_DEFAULT_COLOR so the caller's
 *   `ClayButton(.label="OK"){}` automatically falls through to the theme
 *   without requiring an explicit CELS_DEFAULT_COLOR at every call site.
 *   A caller that WANTS transparent black should pass
 *   `(cels_color_t){0,0,0,0}` while also passing a non-zero alpha, or
 *   explicitly use CELS_DEFAULT_COLOR to route through the theme.
 *
 * ABI guard (threat T-03-06):
 *   _Static_assert on sizeof(cels_color_t) == sizeof(Clay_Color) is placed
 *   here where Clay_Color is in scope via clay_primitives.h.
 */

#include <cels-clay-widgets/clay_button.h>

/* cels-clay layout compositions — provides ClayRow, ClayText, Clay_Color.
 * NOT re-exported through clay_button.h; only needed in this TU. */
#include <cels-clay/clay_primitives.h>

/* ============================================================================
 * ABI-drift guard (T-03-06)
 * ============================================================================
 *
 * cels_color_t and Clay_Color are ABI-identical: both are { float r, g, b, a; }.
 * This assert fails if the layout engine changes its color struct layout before
 * cels_theme.h is updated.
 *
 * clay_primitives.h also carries this assert; the duplicate here is intentional —
 * it verifies the guard holds in the TU where we perform the actual color cast.
 * The duplicate assert is harmless (same condition, same message).
 */
_Static_assert(sizeof(cels_color_t) == sizeof(Clay_Color),
    "cels_color_t layout drifted from Clay_Color -- update cels_theme.h");

/* ============================================================================
 * Component registrations
 * ============================================================================ */

cels_entity_t ClayButtonState_id = 0;
void ClayButtonState_register(void) {
    if (ClayButtonState_id == 0) {
        ClayButtonState_id = cels_component_register("ClayButtonState",
            sizeof(ClayButtonState), CELS_ALIGNOF(ClayButtonState));
    }
}

cels_entity_t ClayButtonProps_id = 0;
void ClayButtonProps_register(void) {
    if (ClayButtonProps_id == 0) {
        ClayButtonProps_id = cels_component_register("ClayButtonProps",
            sizeof(ClayButtonProps), CELS_ALIGNOF(ClayButtonProps));
    }
}

/* ============================================================================
 * Compiled-in fallback constants — third tier (used only when theme is NULL)
 * ============================================================================
 *
 * These are the last-resort color defaults used when cel_watch(CelsTheme_Active)
 * returns NULL (theme singleton not yet bound or module ordering issue).
 *
 * Values are anchored to the original nucleus_button.c literals (D-12):
 *   NUCLEUS_BUTTON_DEFAULT_BG_NORMAL   = { 30,  30,  45, 255}
 *   NUCLEUS_BUTTON_DEFAULT_BG_SELECTED = { 50,  50,  80, 255}
 *   NUCLEUS_BUTTON_DEFAULT_FG_NORMAL   = {220, 220, 240, 255}
 *   NUCLEUS_BUTTON_DEFAULT_FG_DONE     = {100, 100, 120, 255}
 *
 * Per D-12 these are the reference anchor only — visual parity with the
 * original is NOT a pass criterion. The theme slots override these in normal
 * operation (T-03-08: accept disposition — correct, low-risk fallback).
 */
const cels_color_t CLAY_BUTTON_DEFAULT_BG_NORMAL   = { 30.0f,  30.0f,  45.0f, 255.0f };
const cels_color_t CLAY_BUTTON_DEFAULT_BG_SELECTED = { 50.0f,  50.0f,  80.0f, 255.0f };
const cels_color_t CLAY_BUTTON_DEFAULT_FG_NORMAL   = {220.0f, 220.0f, 240.0f, 255.0f };
const cels_color_t CLAY_BUTTON_DEFAULT_FG_DONE     = {100.0f, 100.0f, 120.0f, 255.0f };

static const nucleus_padding_t CLAY_BUTTON_DEFAULT_PADDING = {8, 8, 6, 6};

/* ============================================================================
 * ClayButton composition body
 * ============================================================================ */

CEL_Composition(ClayButton) {
    nucleus_padding_t pad;
    cels_color_t bg_normal, bg_selected, fg_normal, fg_done;
    uint16_t font_size;
    ClayButtonState* state;
    cels_color_t bg, fg;

    /* ---- Auto-seed zero-initialised color props to CELS_DEFAULT_COLOR ----
     *
     * C99 designated-initialiser omission leaves color fields at {0,0,0,0}.
     * Zero is a valid real color (transparent black), so it is NOT the sentinel.
     * We treat an all-zero color as "not set by caller" and replace it with
     * CELS_DEFAULT_COLOR here, allowing `ClayButton(.label="OK"){}` to fall
     * through to the theme automatically (decision A3).
     *
     * A caller that intentionally wants transparent black must pass the color
     * with a non-zero alpha: (cels_color_t){0,0,0,128} for semi-transparent.
     * In practice, transparent buttons are rare and this trade-off is acceptable
     * for the ergonomic gain at every ordinary call site.
     */
#define _AUTO_SEED(field) \
    if (cel.field.r == 0.0f && cel.field.g == 0.0f && \
        cel.field.b == 0.0f && cel.field.a == 0.0f) { \
        cel.field = CELS_DEFAULT_COLOR; \
    }
    _AUTO_SEED(bg_normal)
    _AUTO_SEED(bg_selected)
    _AUTO_SEED(fg_normal)
    _AUTO_SEED(fg_done)
#undef _AUTO_SEED

    /* 1. Attach ClayButtonState and subscribe to per-entity changes in one call.
     *    Defaults land once on first compose; thereafter the field values
     *    are owned by input/selection systems via cel_mutate(entity, ClayButtonState).
     *    Visual reads come from props (cel.selected, cel.done) below, so the
     *    pointer is unused for rendering -- we keep cel_state for its side
     *    effects (component attach + per-entity reactivity subscription). */
    state = cel_state(ClayButtonState, { .pressed = false });
    (void)state;

    /* 2. Attach props as a component so click-dispatch systems can find
     *    .on_click via cel_get(entity, ClayButtonProps) and dispatch
     *    through cel_action_invoke. Props legitimately update across
     *    recompose (e.g., new .label), so we WRITE every time. */
    cel_has(ClayButtonProps,
        .label       = cel.label,
        .on_click    = cel.on_click,
        .font_size   = cel.font_size,
        .padding     = cel.padding,
        .bg_normal   = cel.bg_normal,
        .bg_selected = cel.bg_selected,
        .fg_normal   = cel.fg_normal,
        .fg_done     = cel.fg_done
    );

    /* 3. Read active theme reactively.
     *    cel_watch(CelsTheme_Active) subscribes this composition to theme-change
     *    notifications: when cels_set_theme() fires, every button recomposes.
     *    Returns NULL if the theme singleton is not yet bound (T-03-08 accept:
     *    widget falls through to the compiled-in CLAY_BUTTON_DEFAULT_* values). */
    const struct CelsTheme_Active* ta = cel_watch(CelsTheme_Active);
    const CelsTheme* theme = ta ? &ta->active : NULL;

    /* 4. Resolve visual props with 3-level precedence chain (D-10):
     *       cels_resolve_color(prop, cels_resolve_color(theme_slot, fallback))
     *
     *    D-09 semantic slot mapping:
     *       bg_normal   ← theme.surface
     *       bg_selected ← theme.surface_alt
     *       fg_normal   ← theme.text
     *       fg_done     ← theme.muted
     *
     *    Padding and font_size use the original 2-level fallback (no theme slot
     *    for these — they remain widget-default controlled). */
    bg_normal = cels_resolve_color(cel.bg_normal,
                    cels_resolve_color(
                        theme ? theme->surface : CELS_DEFAULT_COLOR,
                        CLAY_BUTTON_DEFAULT_BG_NORMAL));

    bg_selected = cels_resolve_color(cel.bg_selected,
                      cels_resolve_color(
                          theme ? theme->surface_alt : CELS_DEFAULT_COLOR,
                          CLAY_BUTTON_DEFAULT_BG_SELECTED));

    fg_normal = cels_resolve_color(cel.fg_normal,
                    cels_resolve_color(
                        theme ? theme->text : CELS_DEFAULT_COLOR,
                        CLAY_BUTTON_DEFAULT_FG_NORMAL));

    fg_done = cels_resolve_color(cel.fg_done,
                  cels_resolve_color(
                      theme ? theme->muted : CELS_DEFAULT_COLOR,
                      CLAY_BUTTON_DEFAULT_FG_DONE));

    pad       = (cel.padding.left || cel.padding.right ||
                 cel.padding.top  || cel.padding.bottom)
                    ? cel.padding
                    : CLAY_BUTTON_DEFAULT_PADDING;
    font_size = cel.font_size ? cel.font_size : 16;

    bg = cel.selected ? bg_selected : bg_normal;
    fg = cel.done     ? fg_done     : fg_normal;

    /* 5. Render.
     *    Pass cels_color_t / nucleus_padding_t directly — the *_impl bodies
     *    convert to the layout engine's internal types via the to_clay_*()
     *    helpers in clay_primitives.h.
     *
     *    In debug builds, forward .debug_name to the layout element's .id slot
     *    so the debug overlay can label this widget. Stripped under NDEBUG. */
#ifndef NDEBUG
    ClayRow(.id = cel.debug_name, .padding = pad, .bg = bg) {
#else
    ClayRow(.padding = pad, .bg = bg) {
#endif
        ClayText(.text      = cel.label ? cel.label : "",
                 .font_size = font_size,
                 .color     = fg) {}
    }
}
