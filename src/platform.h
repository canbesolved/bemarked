/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BMKD_PLATFORM_H
#define BMKD_PLATFORM_H

/* Platform interface: the main-thread run loop.
 * Blocks until a shutdown signal (SIGINT/SIGTERM) arrives; the HTTP server runs
 * on its own worker thread. */
void platform_run(void);

#endif /* BMKD_PLATFORM_H */
