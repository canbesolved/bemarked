/* SPDX-License-Identifier: GPL-2.0-only */
/* Platform adapter: tray icon and browser launch.
 * Implementation is per-OS (Win32 Shell_NotifyIcon / macOS Cocoa / Linux
 * libappindicator or none). Headless builds provide a blocking no-op tray. */
#include "platform.h"

void platform_run(int tray_icon, int port) {
    (void)tray_icon; (void)port;
    /* TODO: if tray supported -> native menu loop; else block until shutdown. */
}

void platform_open_browser(int port) {
    (void)port;
    /* TODO: ShellExecute (Windows) / "open" (macOS) / "xdg-open" (Linux). */
}
