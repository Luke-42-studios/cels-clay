/*
 * Demo App Pages - Layout Functions and Compositions
 *
 * Defines the entire UI structure as Clay layout functions attached to
 * CELS compositions. The entity tree maps to Clay's nested layout:
 *
 *   AppShell (top-to-bottom)
 *     TitleBar (fixed height 3)
 *     MainBody (left-to-right, grow)
 *       Sidebar (25% width, top-to-bottom)
 *         NavItem x3
 *       ContentArea (grow, top-to-bottom)
 *         ContentRouter (switches page)
 *           HomePage / SettingsPage / AboutPage
 *     StatusBar (fixed height 1)
 *
 * Layout functions are pure declarations: read component data, emit
 * CLAY() calls, no side effects. CELS reactivity updates component data
 * when state changes; the next layout frame picks up new values.
 *
 * Patterns used:
 *   CEL_Clay(...)        - auto-ID Clay element (NOT bare CLAY())
 *   CEL_Clay_Children()  - emit child entities at this point in tree
 *   CEL_Clay_Text(b, l)  - per-frame arena copy for dynamic strings
 *   CLAY_STRING("...")    - static string literal (no arena needed)
 */

#ifndef DEMO_PAGES_H
#define DEMO_PAGES_H

#include <cels/cels.h>
#include <cels-clay/clay_layout.h>
#include <clay.h>
#include <flecs.h>
#include "components.h"
#include "theme.h"

#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Layout Functions
 * ============================================================================
 *
 * Each function is called per-frame by the Clay layout system. They
 * receive (world, self) and read component data via ecs_get_id().
 * State is accessed directly via CEL_State globals (NavState, DemoSettings).
 */

/* -- App Shell: top-level container ---------------------------------------- */

CEL_Clay_Layout(app_shell_layout) {
    (void)world; (void)self;
    const DemoTheme* theme = demo_get_theme(DemoSettings.color_mode);
    CEL_Clay(
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_GROW(0)
            }
        },
        .backgroundColor = theme->content_bg
    ) {
        CEL_Clay_Children();
    }
}

/* -- Title Bar: app name, fixed 3 rows ------------------------------------- */

CEL_Clay_Layout(title_bar_layout) {
    (void)world; (void)self;
    const DemoTheme* theme = demo_get_theme(DemoSettings.color_mode);

    CEL_Clay(
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_FIXED(3)
            },
            .padding = { .left = 2, .right = 2, .top = 0, .bottom = 0 },
            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
        },
        .backgroundColor = theme->title_bar_bg,
        .border = {
            .color = theme->focused_border,
            .width = CLAY_BORDER_OUTSIDE(1)
        }
    ) {
        CLAY_TEXT(CLAY_STRING("cels-clay demo"),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_accent }));
    }
}

/* -- Status Bar: key hints, fixed 1 row ------------------------------------ */

CEL_Clay_Layout(status_bar_layout) {
    (void)world; (void)self;
    const DemoTheme* theme = demo_get_theme(DemoSettings.color_mode);

    CEL_Clay(
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_FIXED(1)
            },
            .padding = { .left = 1, .right = 1, .top = 0, .bottom = 0 },
            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
        },
        .backgroundColor = theme->status_bar_bg,
        .border = {
            .color = theme->text_secondary,
            .width = CLAY_BORDER_OUTSIDE(1)
        }
    ) {
        CLAY_TEXT(
            CLAY_STRING("Arrows/jk: navigate  Arrows/hl: pane  Enter: select  q: quit"),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_secondary }));
    }
}

/* -- Main Body: horizontal split between sidebar and content --------------- */

CEL_Clay_Layout(main_body_layout) {
    (void)world; (void)self;
    const DemoTheme* theme = demo_get_theme(DemoSettings.color_mode);
    CEL_Clay(
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_GROW(0)
            }
        },
        .backgroundColor = theme->content_bg
    ) {
        CEL_Clay_Children();
    }
}

/* -- Sidebar: 25% width, navigation items ---------------------------------- */

CEL_Clay_Layout(sidebar_layout) {
    (void)world; (void)self;
    const DemoTheme* theme = demo_get_theme(DemoSettings.color_mode);

    /* Right border separates sidebar from content */
    CEL_Clay(
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                .width = CLAY_SIZING_PERCENT(0.25f),
                .height = CLAY_SIZING_GROW(0)
            },
            .padding = CLAY_PADDING_ALL(1)
        },
        .backgroundColor = theme->sidebar_bg,
        .border = {
            .color = theme->text_secondary,
            .width = { .right = 1 }
        }
    ) {
        CEL_Clay_Children();
    }
}

/* -- Nav Item: sidebar navigation entry with highlight --------------------- */

CEL_Clay_Layout(nav_item_layout) {
    const NavItemData* item =
        (const NavItemData*)ecs_get_id(world, self, NavItemDataID);
    if (!item) return;

    const DemoTheme* theme = demo_get_theme(DemoSettings.color_mode);

    bool is_selected = (item->index == NavState.sidebar_selected);
    bool sidebar_focused = (NavState.focus_pane == 0);

    /* Selected item gets highlight background; brighter if sidebar is focused */
    Clay_Color bg = theme->sidebar_bg;
    Clay_Color text_color = theme->text_primary;
    if (is_selected) {
        bg = theme->selected_bg;
        if (sidebar_focused) {
            text_color = theme->text_accent;
        }
    }

    CEL_Clay(
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_FIXED(1)
            },
            .padding = { .left = 1, .right = 1, .top = 0, .bottom = 0 }
        },
        .backgroundColor = bg
    ) {
        CLAY_TEXT(CEL_Clay_Text(item->label, (int)strlen(item->label)),
            CLAY_TEXT_CONFIG({ .textColor = text_color }));
    }
}

/* -- Content Area: main content pane with optional border ------------------ */

CEL_Clay_Layout(content_area_layout) {
    (void)world; (void)self;
    const DemoTheme* theme = demo_get_theme(DemoSettings.color_mode);

    /* Page title text */
    const char* page_names[] = {"Home", "Settings", "About"};
    int page = NavState.current_page;
    if (page < 0 || page > 2) page = 0;

    Clay_BorderWidth border_width = {0, 0, 0, 0, 0};
    if (DemoSettings.show_borders) {
        border_width = (Clay_BorderWidth){1, 1, 1, 1, 0};
    }

    CEL_Clay(
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_GROW(0)
            },
            .padding = CLAY_PADDING_ALL(1),
            .childGap = 1
        },
        .backgroundColor = theme->content_bg,
        .border = {
            .color = (NavState.focus_pane == 1)
                ? theme->focused_border
                : theme->text_secondary,
            .width = border_width
        }
    ) {
        /* Content title showing current page name */
        CLAY_TEXT(CEL_Clay_Text(page_names[page], (int)strlen(page_names[page])),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_accent }));

        /* Page content injected here */
        CEL_Clay_Children();
    }
}

/* -- Home Page: feature showcase ------------------------------------------- */

CEL_Clay_Layout(home_page_layout) {
    (void)world; (void)self;
    const DemoTheme* theme = demo_get_theme(DemoSettings.color_mode);

    CEL_Clay(
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_GROW(0)
            },
            .childGap = 1
        },
        .backgroundColor = theme->content_bg
    ) {
        /* Welcome text */
        CLAY_TEXT(CLAY_STRING("Welcome to cels-clay"),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_primary }));
        CLAY_TEXT(CLAY_STRING("Declarative UI: CELS state + Clay layout + ncurses rendering"),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_secondary }));

        /* Feature row: two boxes side by side */
        CEL_Clay(
            .layout = {
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .sizing = { .width = CLAY_SIZING_GROW(0) },
                .childGap = 1
            },
            .backgroundColor = theme->content_bg
        ) {
            /* Box 1: Reactive state */
            CEL_Clay(
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { .width = CLAY_SIZING_PERCENT(0.5f) },
                    .padding = CLAY_PADDING_ALL(1)
                },
                .backgroundColor = theme->sidebar_bg,
                .border = {
                    .color = theme->text_secondary,
                    .width = CLAY_BORDER_OUTSIDE(1)
                }
            ) {
                CLAY_TEXT(CLAY_STRING("Reactive State"),
                    CLAY_TEXT_CONFIG({ .textColor = theme->text_accent }));
                CLAY_TEXT(CLAY_STRING("CELS compositions observe state."),
                    CLAY_TEXT_CONFIG({ .textColor = theme->text_secondary }));
                CLAY_TEXT(CLAY_STRING("Changes trigger recomposition."),
                    CLAY_TEXT_CONFIG({ .textColor = theme->text_secondary }));
            }

            /* Box 2: Flexbox layout */
            CEL_Clay(
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { .width = CLAY_SIZING_PERCENT(0.5f) },
                    .padding = CLAY_PADDING_ALL(1)
                },
                .backgroundColor = theme->sidebar_bg,
                .border = {
                    .color = theme->text_secondary,
                    .width = CLAY_BORDER_OUTSIDE(1)
                }
            ) {
                CLAY_TEXT(CLAY_STRING("Flexbox Layout"),
                    CLAY_TEXT_CONFIG({ .textColor = theme->text_accent }));
                CLAY_TEXT(CLAY_STRING("Clay computes sizing, padding,"),
                    CLAY_TEXT_CONFIG({ .textColor = theme->text_secondary }));
                CLAY_TEXT(CLAY_STRING("alignment, and grow/shrink."),
                    CLAY_TEXT_CONFIG({ .textColor = theme->text_secondary }));
            }
        }

        /* Centered highlight section */
        CEL_Clay(
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0) },
                .padding = CLAY_PADDING_ALL(1),
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER }
            },
            .backgroundColor = theme->selected_bg,
            .border = {
                .color = theme->focused_border,
                .width = CLAY_BORDER_OUTSIDE(1)
            }
        ) {
            CLAY_TEXT(CLAY_STRING("Try: h/l to switch panes, j/k to navigate, Enter to select"),
                CLAY_TEXT_CONFIG({ .textColor = theme->text_primary }));
        }
    }
}

/* -- Settings Page: toggles for borders and color mode --------------------- */

CEL_Clay_Layout(settings_page_layout) {
    (void)world; (void)self;
    const DemoTheme* theme = demo_get_theme(DemoSettings.color_mode);

    /* Settings items: show_borders toggle (idx 0), color_mode toggle (idx 1) */
    bool content_focused = (NavState.focus_pane == 1);

    CEL_Clay(
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_GROW(0)
            },
            .childGap = 1
        },
        .backgroundColor = theme->content_bg
    ) {
        /* Toggle 0: Show Borders */
        {
            bool item_selected = content_focused
                && (NavState.sidebar_selected == 0);
            Clay_Color bg = item_selected
                ? theme->selected_bg : theme->content_bg;
            Clay_Color tc = item_selected
                ? theme->text_accent : theme->text_primary;

            char buf[64];
            int len = snprintf(buf, sizeof(buf), "Show borders: %s",
                               DemoSettings.show_borders ? "ON" : "OFF");

            CEL_Clay(
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIXED(1) },
                    .padding = { .left = 1, .right = 1, .top = 0, .bottom = 0 }
                },
                .backgroundColor = bg
            ) {
                CLAY_TEXT(CEL_Clay_Text(buf, len),
                    CLAY_TEXT_CONFIG({ .textColor = tc }));
            }
        }

        /* Toggle 1: Color Mode */
        {
            bool item_selected = content_focused
                && (NavState.sidebar_selected == 1);
            Clay_Color bg = item_selected
                ? theme->selected_bg : theme->content_bg;
            Clay_Color tc = item_selected
                ? theme->text_accent : theme->text_primary;

            char buf[64];
            int len = snprintf(buf, sizeof(buf), "Color mode: %s",
                               DemoSettings.color_mode == 0
                               ? "Theme A" : "Theme B");

            CEL_Clay(
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIXED(1) },
                    .padding = { .left = 1, .right = 1, .top = 0, .bottom = 0 }
                },
                .backgroundColor = bg
            ) {
                CLAY_TEXT(CEL_Clay_Text(buf, len),
                    CLAY_TEXT_CONFIG({ .textColor = tc }));
            }
        }

        /* Hint text */
        CLAY_TEXT(CLAY_STRING("Press Enter to toggle selected setting"),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_secondary }));
    }
}

/* -- About Page: scroll container with long-form text ---------------------- */

CEL_Clay_Layout(about_page_layout) {
    (void)world; (void)self;
    const DemoTheme* theme = demo_get_theme(DemoSettings.color_mode);

    CEL_Clay(
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_GROW(0)
            },
            .padding = CLAY_PADDING_ALL(1),
            .childGap = 1
        },
        .backgroundColor = theme->content_bg,
        .clip = {
            .vertical = true,
            .childOffset = Clay_GetScrollOffset()
        }
    ) {
        CLAY_TEXT(CLAY_STRING("About cels-clay"),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_accent }));

        CLAY_TEXT(CLAY_STRING(
            "cels-clay is a module that integrates Clay, a high-performance "
            "flexbox-style layout engine, with the CELS declarative application "
            "framework."),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_primary }));

        CLAY_TEXT(CLAY_STRING(
            "Architecture: CELS compositions declare the UI tree structure and "
            "manage reactive state. Clay computes spatial layout (sizing, "
            "padding, alignment, grow/shrink) each frame. An ncurses renderer "
            "translates Clay render commands into terminal output."),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_primary }));

        CLAY_TEXT(CLAY_STRING(
            "The layout system walks the CELS entity hierarchy depth-first. "
            "Each entity's layout function opens a CLAY() scope, calls "
            "CEL_Clay_Children() to recurse into children, then the scope "
            "closes. Entity order equals Clay nesting order."),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_primary }));

        CLAY_TEXT(CLAY_STRING(
            "Reactivity bridge: CELS compositions are reactive and re-run on "
            "state change. Clay is immediate-mode and rebuilds every frame. "
            "Compositions update component data reactively; layout functions "
            "read that data every frame. The ECS is the shared state layer."),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_primary }));

        CLAY_TEXT(CLAY_STRING(
            "Key features: Automatic Clay element IDs via __COUNTER__, "
            "per-frame arena for dynamic string lifetime management, "
            "ClaySurface composition for reactive layout dimensions, "
            "and a Feature/Provider bridge to renderer backends."),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_primary }));

        CLAY_TEXT(CLAY_STRING(
            "The ncurses renderer handles five Clay command types: "
            "RECTANGLE (filled backgrounds), TEXT (string output), "
            "BORDER (box-drawing characters), SCISSOR_START/END (clip "
            "regions for scroll containers). It compensates for terminal "
            "cell aspect ratio (2:1 width-to-height)."),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_primary }));

        CLAY_TEXT(CLAY_STRING(
            "This demo application serves as the canonical example for "
            "building a cels-clay app. It demonstrates sidebar navigation, "
            "reactive page routing, live settings toggles, scroll containers, "
            "and theme switching."),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_primary }));

        CLAY_TEXT(CLAY_STRING(
            "Scroll this page with j/k or Ctrl-U/Ctrl-D. "
            "Press G to jump to bottom, gg to jump to top."),
            CLAY_TEXT_CONFIG({ .textColor = theme->text_secondary }));
    }
}

/* ============================================================================
 * Compositions
 * ============================================================================
 *
 * Compositions define the entity tree structure. Each attaches a ClayUI
 * component with a layout function pointer. The layout system calls these
 * functions per-frame during the Clay layout pass.
 *
 * Ordering: leaf compositions first, parents after, so #define macros
 * (shorthand) are visible before use in parent compositions.
 */

/* -- TitleBar -------------------------------------------------------------- */

#define TitleBar(...) CEL_Init(TitleBar, __VA_ARGS__)
CEL_Composition(TitleBar) {
    (void)props;
    CEL_Has(ClayUI, .layout_fn = title_bar_layout);
}

/* -- StatusBar ------------------------------------------------------------- */

#define StatusBar(...) CEL_Init(StatusBar, __VA_ARGS__)
CEL_Composition(StatusBar) {
    (void)props;
    CEL_Has(ClayUI, .layout_fn = status_bar_layout);
}

/* -- NavItem: sidebar entry with label and index --------------------------- */

#define NavItem(...) CEL_Init(NavItem, __VA_ARGS__)
CEL_Composition(NavItem, const char* label; int index;) {
    CEL_Has(ClayUI, .layout_fn = nav_item_layout);
    CEL_Has(NavItemData, .label = props.label, .index = props.index);
}

/* -- Sidebar --------------------------------------------------------------- */

#define Sidebar(...) CEL_Init(Sidebar, __VA_ARGS__)
CEL_Composition(Sidebar) {
    (void)props;
    CEL_Has(ClayUI, .layout_fn = sidebar_layout);
}

/* -- HomePage -------------------------------------------------------------- */

#define HomePage(...) CEL_Init(HomePage, __VA_ARGS__)
CEL_Composition(HomePage) {
    (void)props;
    CEL_Has(ClayUI, .layout_fn = home_page_layout);
}

/* -- SettingsPage ---------------------------------------------------------- */

#define SettingsPage(...) CEL_Init(SettingsPage, __VA_ARGS__)
CEL_Composition(SettingsPage) {
    (void)props;
    /* Watch DemoSettings so layout updates when toggles change */
    CEL_Watch(DemoSettings);
    CEL_Has(ClayUI, .layout_fn = settings_page_layout);
}

/* -- AboutPage ------------------------------------------------------------- */

#define AboutPage(...) CEL_Init(AboutPage, __VA_ARGS__)
CEL_Composition(AboutPage) {
    (void)props;
    CEL_Has(ClayUI, .layout_fn = about_page_layout);
}

/* -- ContentRouter: watches NavState and mounts active page ---------------- */

#define ContentRouter(...) CEL_Init(ContentRouter, __VA_ARGS__)
CEL_Composition(ContentRouter) {
    (void)props;
    NavState_t* nav = CEL_Watch(NavState);

    switch (nav->current_page) {
        case 0:  HomePage() {}     break;
        case 1:  SettingsPage() {} break;
        case 2:  AboutPage() {}    break;
        default: HomePage() {}     break;
    }
}

/* -- ContentArea ----------------------------------------------------------- */

#define ContentArea(...) CEL_Init(ContentArea, __VA_ARGS__)
CEL_Composition(ContentArea) {
    (void)props;
    CEL_Has(ClayUI, .layout_fn = content_area_layout);
}

/* -- MainBody: horizontal split -------------------------------------------- */

#define MainBody(...) CEL_Init(MainBody, __VA_ARGS__)
CEL_Composition(MainBody) {
    (void)props;
    CEL_Has(ClayUI, .layout_fn = main_body_layout);
}

/* -- AppShell: root layout container --------------------------------------- */

#define AppShell(...) CEL_Init(AppShell, __VA_ARGS__)
CEL_Composition(AppShell) {
    (void)props;
    CEL_Has(ClayUI, .layout_fn = app_shell_layout);

    TitleBar() {}
    MainBody() {
        Sidebar() {
            NavItem(.label = "Home",     .index = 0) {}
            NavItem(.label = "Settings", .index = 1) {}
            NavItem(.label = "About",    .index = 2) {}
        }
        ContentArea() {
            ContentRouter() {}
        }
    }
    StatusBar() {}
}

#endif /* DEMO_PAGES_H */
