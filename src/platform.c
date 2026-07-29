/* SPDX-License-Identifier: GPL-2.0-only */
/* Platform adapter: foreground run loop.
 *
 * platform_run() installs SIGINT/SIGTERM handlers and blocks the main thread
 * until a shutdown signal arrives (the server runs on its own worker thread). */
#include "platform.h"

#include <signal.h>
#include <time.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

void platform_run(void) {
    struct timespec ts = {0, 200 * 1000 * 1000};  /* 200ms */
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    while (!g_stop) nanosleep(&ts, NULL);
}
