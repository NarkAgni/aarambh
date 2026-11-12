# Writing a theme

A theme only draws. The core already handles the config file, entries,
kernel lists, keyboard navigation, the countdown, the clock, animation state
and booting. Every frame it hands the theme a `ThemeFrame` snapshot and the
theme paints the screen from it.

The bundled themes in `src/themes/` (one file each) are the best
examples. `minimal/minimal.c` is the smallest.

## 1. Create the theme

`src/themes/glow/glow.c`:

```c
#include "aarambh/theme_kit.h"

#define GLOW_ACCENT 0xff4d8d

static Face title_face;
static Image backdrop;
static Image glow_sprite;

static void glow_load(const ThemeContext *context)
{
    const Image *screen = context->screen;

    face_init(&title_face, context->font_bold, kit_s(screen, 80));

    kit_glow_sprite(&glow_sprite);
    if (kit_backdrop(&backdrop, screen, 0x10061a, 0x05020a)) {
        kit_glow(&backdrop, &glow_sprite, screen->width / 2.0f, screen->height / 2.0f,
                 kit_s(screen, 900), kit_s(screen, 900), GLOW_ACCENT, 120);
    }
}

static void glow_render(const ThemeContext *context, const ThemeFrame *frame)
{
    Image *screen = context->screen;
    KitFade fade = kit_fade(frame);
    const Entry *entry = &context->entries[frame->selected];

    kit_draw_backdrop(screen, &backdrop);
    kit_text_center(screen, &title_face, screen->width / 2.0f, (int)kit_y(screen, 420),
                    entry->title, COLOR_WHITE, fade.alpha);

    kit_splash(context, frame, &title_face, &title_face, &backdrop, COLOR_WHITE, GLOW_ACCENT);
    kit_intro_blackout(screen, frame);
}

const Theme glow_theme = {
    .name = "glow",
    .list_rows = KIT_LIST_ROWS,
    .vertical_navigation = 0,
    .down_opens_list = 1,
    .load = glow_load,
    .render = glow_render
};
```

## 2. Register it

In `src/core/theme_registry.c` add `extern const Theme glow_theme;` and
`&glow_theme` to the `themes` array. The Makefile builds every `.c` file under
`src/`, so nothing else changes.

## 3. Select it

```
theme glow
```

An unknown name falls back to the first registered theme (`accordion`).

## Rules that keep a theme fast and correct

- **Build everything static once, in `load`.** Backgrounds, glows, blur,
  patterns and grain go into a cached `Image`; `render` starts with
  `kit_draw_backdrop` (one copy) and draws only what moves.
- **Text is ASCII only.** The font renderer covers characters 32 to 126. Use
  `ui_arrows` for arrow symbols and `-` instead of `·`.
- **Design on 1280x800.** `kit_x`, `kit_y` and `kit_s` scale those numbers to
  the real screen, from 720p to 4K.
- **Long names.** Init a few title sizes and pick one with `kit_fit`, so
  "openSUSE Tumbleweed" fits as well as "Arch".
- **Logos are white silhouettes.** Draw them in any color with `kit_logo`; it
  falls back to the first letter when an OS has no logo.
- **Finish the launch animation in time.** The core boots
  `AARAMBH_LAUNCH_TOTAL_MS` (1.8 s) after Enter.

## Navigation

| `Theme` field         | Value | Behavior                                                |
|-----------------------|-------|---------------------------------------------------------|
| `vertical_navigation` | 0     | Left / Right switch entries                             |
|                       | 1     | Up / Down switch entries, Right opens the kernel list   |
| `down_opens_list`     | 1     | Down opens the kernel list (themes without an options button) |
|                       | 0     | Down focuses "Advanced options", Enter opens it (Accordion) |
| `list_rows`           | n     | Visible kernel rows, used for scrolling (`KIT_LIST_ROWS` with `kit_kernel_list`) |
| `edit`                | `&colors` | Colors of the boot options editor (`EditColors`); `NULL` uses a neutral dark card |

The core draws the boot options editor (E) on top of every theme; a theme only
picks its colors and frame:

```c
static const EditColors glow_edit = {
    0x1a1c20,          /* panel */
    0x0d0e10,          /* text field */
    0x3ddc97,          /* accent: cursor, heading, border (UI_ENTRY_ACCENT for the entry's) */
    0xeafff5,          /* text */
    0x9fb0a9,          /* notes and key hints */
    EDIT_FRAME_CARD    /* or EDIT_FRAME_BRACKETS (HUD), EDIT_FRAME_RULES (Editorial) */
};
```

`kit_hints` adds **E Edit** by itself. A theme that draws its own hints shows
it when `frame->can_edit` is 1.

## What a theme receives

`ThemeContext` (fixed for the whole session)

| Field                          | Description                               |
|--------------------------------|-------------------------------------------|
| `screen`                       | Back buffer; presented after `render`     |
| `entries`, `entry_count`       | Entries with decoded `logo`, `background` (always the size of the screen, never empty) |
| `default_index`                | Entry marked as default                   |
| `font_light/regular/bold/mono` | Loaded fonts for `face_init`              |

`ThemeFrame` (new every frame)

| Field                     | Description                                         |
|---------------------------|-----------------------------------------------------|
| `now`                     | Milliseconds since start                            |
| `intro_elapsed`           | Milliseconds since the intro animation started      |
| `launch_elapsed`          | Milliseconds since launch began (when `launching`)  |
| `position`                | Animated selected index (`1.4` = between 1 and 2)   |
| `focus`                   | `0..1` primary button to "Advanced options"         |
| `list`                    | `0..1` kernel list opening                          |
| `selected`                | Selected entry index                                |
| `launching`               | Non-zero while the launch animation runs            |
| `launch_kernel`           | Kernel picked from the list, `-1` for default       |
| `list_selected`           | Highlighted kernel row                              |
| `list_scroll`             | First visible row                                   |
| `status`                  | Countdown or error text, `""` when idle             |
| `clock_time/clock_period` | `"9:41"`, `"AM"`                                    |

## Theme kit (`aarambh/theme_kit.h`)

| Function                                   | Purpose                                         |
|--------------------------------------------|-------------------------------------------------|
| `kit_x`, `kit_y`, `kit_s`                  | Scale 1280x800 design numbers to the screen     |
| `kit_fade`                                 | Intro and launch progress plus content alpha    |
| `kit_intro_blackout`                       | Fade in from black at the start                 |
| `kit_weight(frame, i)`                     | 0..1 closeness of entry `i` to the selection    |
| `kit_nearest`                              | Entry to describe now and how visible it is     |
| `kit_backdrop`, `kit_draw_backdrop`        | Cached gradient background                      |
| `kit_glow_sprite`, `kit_glow`              | Soft colored light                              |
| `kit_stars`                                | Random star field                               |
| `kit_logo`                                 | Tinted logo or first letter                     |
| `kit_text_center`, `kit_text_right`        | Aligned text                                    |
| `kit_fit`                                  | Largest face that fits a width                  |
| `kit_detail_line`                          | "Default system - Linux ..." (+ status)         |
| `kit_hints`, `kit_hints_center`, `kit_hints_width` | Key hints that follow navigation and list state |
| `kit_kernel_list`                          | Kernel list in the theme's colors               |
| `kit_splash`                               | "Starting Ubuntu" splash on the theme backdrop  |

## Widgets (`aarambh/widgets.h`)

Every widget takes its colors, so themes are not tied to white text.

| Widget                  | Colors                                              |
|-------------------------|-----------------------------------------------------|
| `ui_key_cap`, `ui_hint`, `ui_arrows` | `UiHintStyle { face, color, shadow }`  |
| `ui_button_draw`        | `fill_color`, `text_color`, `active`, `idle_fill`, `border` |
| `ui_clock_draw`         | `time_color`, `period_color`, `shadow`              |
| `ui_option_list_draw`   | `heading_color`, `text_color`, `highlight_color`, `accent_color` (`UI_ENTRY_ACCENT` = the entry's own) |
| `ui_loading_dots`       | `color`                                             |
| `ui_launch_splash_draw` | `backdrop`, `title_color`, `detail_color`, `dot_color`, `darken` |

## Drawing (`aarambh/gfx/`)

`draw_image`, `draw_image_tinted`, `draw_image_slice`, `draw_backdrop`
(frosted glass from a blurred copy), `draw_image_perspective` and
`draw_image_reflection` (a card turned away from the viewer and its mirror
image, see Gallery), `draw_rect`, `draw_round_rect`,
`draw_ring`, `draw_arc`, `draw_line`, `draw_polygon`, `draw_triangle`,
`draw_text`, `draw_text_shadow`, `draw_text_vertical`, `text_width`,
`image_blur`, `image_noise`, `image_blend`, `image_darken`, `image_gradient`,
`image_glow_sprite`, `image_round_corners`.

Opacity values run from `0` to `256` (`ALPHA_OPAQUE`). Colors are `0xRRGGBB`.