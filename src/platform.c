/* SPDX-License-Identifier: GPL-2.0-only */
/* Platform adapter: foreground run loop and browser launch.
 *
 * MVP: no native tray yet. platform_run() installs SIGINT/SIGTERM handlers and
 * blocks the main thread until a shutdown signal arrives (the server runs on its
 * own worker thread). Native tray menus are a future desktop-profile addition. */
#include "platform.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

void platform_run(int tray_icon, int port) {
    struct timespec ts = {0, 200 * 1000 * 1000};  /* 200ms */
    (void)tray_icon;
    (void)port;
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    while (!g_stop) nanosleep(&ts, NULL);
}

void platform_open_browser(int port) {
    char cmd[128];
#if defined(_WIN32)
    snprintf(cmd, sizeof(cmd), "start http://127.0.0.1:%d", port);
#elif defined(__APPLE__)
    snprintf(cmd, sizeof(cmd), "open http://127.0.0.1:%d", port);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open http://127.0.0.1:%d >/dev/null 2>&1 &", port);
#endif
    if (system(cmd) != 0) { /* best-effort; ignore failure */ }
}
