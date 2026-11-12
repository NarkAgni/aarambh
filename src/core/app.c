#include "core/app.h"

#include "aarambh/timing.h"
#include "aarambh/widgets.h"
#include "boot/launcher.h"
#include "boot/plymouth_overlay.h"
#include "core/app_state.h"
#include "core/editor.h"
#include "core/shutdown_splash.h"
#include "core/input.h"
#include "core/navigation.h"
#include "core/theme_registry.h"
#include "efi/system.h"
#include "efi/timer.h"
#include "efi/video.h"
#include "lib/str.h"

static AppState app;

static void build_status(float now)
{
    int seconds_left;

    app.status[0] = '\0';

    if (now - app.message_time < AARAMBH_MESSAGE_MS) {
        str_copy(app.status, app.message, sizeof(app.status));
        return;
    }

    if (!entries_has_system(&app.entries)) {
        str_copy(app.status, "No bootable system found", sizeof(app.status));
        return;
    }

    if (!app.countdown_active || app.launching) {
        return;
    }

    seconds_left = (int)((app.countdown_deadline - now) / 1000.0f) + 1;
    if (seconds_left < 0) {
        seconds_left = 0;
    }

    str_copy(app.status, "Starting in ", sizeof(app.status));
    str_append_int(app.status, seconds_left, sizeof(app.status));
}

static void build_frame(ThemeFrame *frame, float now)
{
    clock_update(&app.clock, now);
    build_status(now);

    frame->now = now;
    frame->intro_elapsed = now - app.intro_start;
    frame->launch_elapsed = now - app.launch_start;
    frame->position = tween_value(&app.position, now);
    frame->focus = tween_value(&app.focus, now);
    frame->list = tween_value(&app.list, now);
    frame->selected = app.selected;
    frame->launching = app.launching;
    frame->launch_kernel = app.launch_kernel;
    frame->plymouth_still = 0;
    frame->splash_title = NULL;
    frame->can_edit = editor_can_open(&app);
    frame->list_selected = app.list_selected;
    frame->list_scroll = app.list_scroll;
    frame->status = app.status;
    frame->clock_time = app.clock.time;
    frame->clock_period = app.clock.period;
}

static void draw_frame(void)
{
    ThemeFrame frame;

    build_frame(&frame, timer_now_ms());
    app.theme->render(&app.context, &frame);
    editor_draw(&app, frame.now);
    video_present(&app.screen);
}

static void setup_context(void)
{
    app.context.screen = &app.screen;
    app.context.entries = app.entries.items;
    app.context.entry_count = app.entries.count;
    app.context.default_index = app.selected;
    app.context.font_light = &app.fonts.light;
    app.context.font_regular = &app.fonts.regular;
    app.context.font_bold = &app.fonts.bold;
    app.context.font_mono = &app.fonts.mono;
}

EFI_STATUS app_init(void)
{
    EFI_STATUS status;

    timer_calibrate();

    status = video_init(&app.screen);
    if (EFI_ERROR(status)) {
        return status;
    }
    video_clear(&app.screen);

    config_load(&app.config, &app.entries);
    if (app.entries.count == 0) {
        return EFI_NOT_FOUND;
    }

    assets_load_fonts(&app.fonts, &app.config);
    if (!assets_load_entries(&app.entries, &app.screen)) {
        return EFI_OUT_OF_RESOURCES;
    }

    video_enable_write_combining(app.config.write_combining);

    clock_init(&app.clock, app.config.utc_offset_minutes);
    app.theme = theme_registry_find(app.config.theme);

    nav_init(&app, timer_now_ms());
    setup_context();
    app.theme->load(&app.context);
    editor_load(&app.context);

    return EFI_SUCCESS;
}

/*
 * Renders the finished launch splash without its dots into the back buffer
 * (never shown) and packs it for Plymouth, so the OS's boot splash continues
 * the current theme instead of the one installed with setup.sh.
 */
static void render_still(const char *title)
{
    ThemeFrame frame;

    build_frame(&frame, timer_now_ms());
    frame.launch_elapsed = AARAMBH_LAUNCH_TOTAL_MS;
    frame.plymouth_still = 1;
    frame.splash_title = title;
    app.theme->render(&app.context, &frame);
}

static void build_plymouth_overlay(PlymouthOverlay *overlay, const Entry *entry)
{
    overlay->data = NULL;
    overlay->size = 0;

    if (!launcher_boots_direct(entry, app.launch_kernel)) {
        return;
    }

    render_still(NULL);
    plymouth_overlay_build(overlay, &app.screen, ui_launch_splash_dot_color());
}

static const char *launch_error_message(LaunchError error)
{
    switch (error) {
    case LAUNCH_BLOCKED:
        return "Blocked by Secure Boot";
    case LAUNCH_CHOICE_NOT_SAVED:
        return "Could not save the kernel choice, nothing was started";
    case LAUNCH_FAILED:
        break;
    }
    return "Could not start this system";
}

static void launch_selected(void)
{
    Entry *entry = nav_selected_entry(&app);
    PlymouthOverlay overlay;
    LaunchError error;
    float now;

    shutdown_splash_update(entry, app.theme->name, &app.screen, render_still);
    build_plymouth_overlay(&overlay, entry);
    error = launcher_run(entry, app.launch_kernel, editor_cmdline(&app), &overlay, draw_frame);
    plymouth_overlay_free(&overlay);

    video_restore_mode();
    system_hide_cursor();

    now = timer_now_ms();
    nav_show_message(&app, launch_error_message(error), now);
    nav_reset_after_failure(&app, now);
}

static void update(float now)
{
    input_poll(&app, now);

    if (app.countdown_active && now >= app.countdown_deadline) {
        nav_launch(&app, -1, now);
    }

    if (app.launching && now - app.launch_start >= AARAMBH_LAUNCH_TOTAL_MS) {
        launch_selected();
    }
}

void app_run(void)
{
    for (;;) {
        float frame_start = timer_now_ms();
        float elapsed;

        update(frame_start);
        draw_frame();

        elapsed = timer_now_ms() - frame_start;
        timer_sleep_ms(AARAMBH_FRAME_MS - elapsed);
    }
}