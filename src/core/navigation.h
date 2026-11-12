#ifndef AARAMBH_CORE_NAVIGATION_H
#define AARAMBH_CORE_NAVIGATION_H

#include "core/app_state.h"

void nav_init(AppState *app, float now);

Entry *nav_selected_entry(AppState *app);

void nav_select(AppState *app, int index, float now);
void nav_set_focus(AppState *app, int focused, float now);

void nav_open_list(AppState *app, float now);
void nav_close_list(AppState *app, float now);
void nav_move_list(AppState *app, int delta);

void nav_launch(AppState *app, int kernel, float now);
void nav_cancel_countdown(AppState *app);
void nav_show_message(AppState *app, const char *message, float now);
void nav_reset_after_failure(AppState *app, float now);

#endif
