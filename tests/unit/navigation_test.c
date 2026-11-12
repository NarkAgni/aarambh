/* The countdown may only ever start an operating system. */

#include <string.h>

#include "core/navigation.h"
#include "support/efi_mock.h"
#include "support/test.h"

static AppState app;

static void make_menu(int systems, int default_index, int timeout)
{
    int i;

    memset(&app, 0, sizeof(app));
    app.entries.count = systems + 2;
    app.config.default_index = default_index;
    app.config.timeout_seconds = timeout;

    for (i = 0; i < systems; i++) {
        app.entries.items[i].action = ENTRY_ACTION_EFI;
    }

    app.entries.items[systems].action = ENTRY_ACTION_REBOOT;
    app.entries.items[systems + 1].action = ENTRY_ACTION_SHUTDOWN;

    nav_init(&app, 0.0f);
}

int main(void)
{
    mock_efi_init();

    make_menu(2, 1, 10);
    CHECK(app.selected == 1 && app.countdown_active, "a default system counts down");

    make_menu(2, 0, 0);
    CHECK(!app.countdown_active, "timeout 0 never counts down");

    make_menu(0, 0, 10);
    CHECK(app.selected == 0 && !app.countdown_active,
          "no system found: Restart is selected but never starts by itself");

    make_menu(2, 2, 10);
    CHECK(app.selected == 2 && !app.countdown_active,
          "Restart as the default waits for a key");

    make_menu(2, 3, 10);
    CHECK(!app.countdown_active, "Power Off as the default waits for a key");

    make_menu(2, 99, 10);
    CHECK(app.selected == 0 && app.countdown_active,
          "a default index past the last entry falls back to the first system");

    make_menu(2, -4, 10);
    CHECK(app.selected == 0 && app.countdown_active, "a negative default index does too");

    return test_result();
}
