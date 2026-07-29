/* SPDX-License-Identifier: GPL-2.0-only */
/* Portability layer implementation — see compat.h. */
#include "compat.h"

#ifdef _WIN32

#include <windows.h>
#include <io.h>
#include <process.h>

int bmkd_fsync(FILE *f) {
    HANDLE h;
    if (fflush(f) != 0) return -1;
    h = (HANDLE)_get_osfhandle(_fileno(f));
    if (h == INVALID_HANDLE_VALUE) return -1;
    return FlushFileBuffers(h) ? 0 : -1;
}

int bmkd_rename(const char *src, const char *dst) {
    /* REPLACE_EXISTING = overwrite dst; WRITE_THROUGH = flush the rename to disk. */
    return MoveFileExA(src, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
}

void bmkd_sleep_ms(int ms) { Sleep((DWORD)ms); }

long bmkd_pid(void) { return (long)_getpid(); }

#else /* POSIX */

#include <unistd.h>
#include <time.h>

int bmkd_fsync(FILE *f) {
    if (fflush(f) != 0) return -1;
    return fsync(fileno(f));
}

int bmkd_rename(const char *src, const char *dst) {
    return rename(src, dst);   /* POSIX rename() atomically replaces dst */
}

void bmkd_sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

long bmkd_pid(void) { return (long)getpid(); }

#endif
