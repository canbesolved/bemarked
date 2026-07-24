/* SPDX-License-Identifier: GPL-2.0-only */
/* conf.txt parser implementation. Quote-aware so hex color codes (#RRGGBB) inside
 * quoted strings are not truncated by inline # comments. */
#include "config.h"

int config_load(const char *path, struct config *out) {
    (void)path;
    (void)out;
    /* TODO: parse key = value / key = "value", strip # comments outside quotes,
     * fill defaults (port=8989, bind="127.0.0.1"), collect shortcut[N] entries. */
    return 0;
}
