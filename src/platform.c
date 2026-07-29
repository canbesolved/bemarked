/* SPDX-License-Identifier: GPL-2.0-only */
/* Platform adapter: foreground run loop.
 *
 * platform_run() installs a Ctrl-C / termination handler and blocks the main
 * thread until a shutdown signal arrives (the server runs on its own worker
 * thread). SIGTERM is POSIX-only; on Windows SIGINT covers Ctrl-C. */
#include "platform.h"
#include "compat.h"

#include <signal.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

void platform_run(void) {
    signal(SIGINT, on_signal);
#ifndef _WIN32
    signal(SIGTERM, on_signal);
#endif
    while (!g_stop) bmkd_sleep_ms(200);
}
