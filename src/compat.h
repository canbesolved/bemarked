/* SPDX-License-Identifier: GPL-2.0-only */
/* Small portability layer for the handful of places where POSIX and Windows
 * disagree. Threads stay on <pthread.h> (on Windows we build against
 * mingw-w64's winpthreads), so only these four primitives need a shim. */
#ifndef BMKD_COMPAT_H
#define BMKD_COMPAT_H

#include <stdio.h>

/* Flush stdio buffers and the OS write-back cache for f's fd. Returns 0 on success. */
int bmkd_fsync(FILE *f);

/* Atomically replace `dst` with `src`, overwriting `dst` if it already exists.
 * POSIX rename() already does this; Windows rename() fails on an existing dst,
 * so there we use MoveFileEx(REPLACE_EXISTING). Returns 0 on success. */
int bmkd_rename(const char *src, const char *dst);

/* Sleep for `ms` milliseconds. */
void bmkd_sleep_ms(int ms);

/* Current process id (used only to seed rand()). */
long bmkd_pid(void);

#endif /* BMKD_COMPAT_H */
