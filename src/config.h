/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BMKD_CONFIG_H
#define BMKD_CONFIG_H

/* conf.txt parser (quote-aware; supports # comments and hex color values). */

#define BMKD_MAX_SHORTCUTS 64

struct shortcut {
    char name[64];
    char color[8];   /* #RRGGBB */
    char url[512];
};

struct config {
    int  port;              /* default 8989 */
    char bind[64];          /* default "127.0.0.1" */
    char bookmarks_dir[512];
    int  tray_icon;         /* 1 = enable, 0 = disable */
    int  shortcuts_on_row;
    int  rows;
    struct shortcut shortcuts[BMKD_MAX_SHORTCUTS];
    int  shortcut_count;
};

/* Returns 0 on success, non-zero on parse/IO error. Missing keys keep defaults. */
int config_load(const char *path, struct config *out);

#endif /* BMKD_CONFIG_H */
