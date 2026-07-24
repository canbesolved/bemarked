/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BMKD_PLATFORM_H
#define BMKD_PLATFORM_H

/* Platform interface: native tray + open-browser.
 * Desktop adapter registers a tray icon (Open Dashboard / Quit) and runs the
 * main-thread UI loop. On headless platforms this degrades to a plain block
 * until shutdown, and tray requests are ignored gracefully. */

/* Run the main-thread loop. If tray_icon is enabled and supported, shows the
 * tray menu; otherwise blocks until the server is stopped. */
void platform_run(int tray_icon, int port);

/* Open the system default browser at http://127.0.0.1:<port>/. */
void platform_open_browser(int port);

#endif /* BMKD_PLATFORM_H */
