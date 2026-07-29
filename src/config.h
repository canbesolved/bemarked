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
    int  port;              /* default 7773 */
    char bind[64];          /* default "127.0.0.1" */
    char bookmarks_file[512];   /* path to bookmarks.txt (default: next to the binary) */
    int  shortcuts_per_row;
    char link_open_mode[16];  /* "new-tab" (default) | "same-tab" */
    struct shortcut shortcuts[BMKD_MAX_SHORTCUTS];
    int  shortcut_count;
    char conf_path[512];      /* remembered so shortcuts can be written back */
};

/* Returns 0 on success, non-zero on parse/IO error. Missing keys keep defaults. */
int config_load(const char *path, struct config *out);

/* Append a shortcut and persist to conf.txt. Returns its index, or -1 on error. */
int config_add_shortcut(struct config *c, const char *name, const char *color, const char *url);

/* Delete shortcut at index, renumber the rest densely, persist. 0 on success. */
int config_delete_shortcut(struct config *c, int index);

/* Update the shortcut at index and persist. Returns 0 on success, -1 on error. */
int config_update_shortcut(struct config *c, int index, const char *name,
                           const char *color, const char *url);

/* Swap the two shortcuts at indices a and b (nothing else moves), then persist. */
int config_swap_shortcut(struct config *c, int a, int b);

#endif /* BMKD_CONFIG_H */
