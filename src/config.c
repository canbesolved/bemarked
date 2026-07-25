/* SPDX-License-Identifier: GPL-2.0-only */
/* conf.txt parser implementation. Quote-aware so hex color codes (#RRGGBB) inside
 * quoted strings are not truncated by inline # comments. */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void defaults(struct config *c) {
    memset(c, 0, sizeof(*c));
    c->port = 8989;
    snprintf(c->bind, sizeof(c->bind), "127.0.0.1");
    snprintf(c->bookmarks_dir, sizeof(c->bookmarks_dir), "./data");
    c->tray_icon = 1;
    c->shortcuts_on_row = 4;
    c->rows = 3;
    snprintf(c->link_open_mode, sizeof(c->link_open_mode), "new-tab");
}

/* Trim leading/trailing ASCII whitespace in place, return pointer to start. */
static char *trim(char *s) {
    char *e;
    while (*s && isspace((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';
    return s;
}

/* Drop an unquoted # comment: NUL-terminate at the first # outside quotes. */
static void strip_comment(char *s) {
    int in_q = 0;
    for (; *s; s++) {
        if (*s == '"') in_q = !in_q;
        else if (*s == '#' && !in_q) { *s = '\0'; return; }
    }
}

/* Remove surrounding double quotes if present. */
static char *unquote(char *s) {
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        s[n - 1] = '\0';
        return s + 1;
    }
    return s;
}

/* Parse `Name | #HEX | URL` into a shortcut. */
static void parse_shortcut(char *val, struct shortcut *sc) {
    char *parts[3] = {val, NULL, NULL};
    int i = 1;
    char *p = val;
    while (i < 3 && (p = strchr(p, '|')) != NULL) {
        *p++ = '\0';
        parts[i++] = p;
    }
    memset(sc, 0, sizeof(*sc));
    if (parts[0]) snprintf(sc->name, sizeof(sc->name), "%s", trim(parts[0]));
    if (parts[1]) snprintf(sc->color, sizeof(sc->color), "%s", trim(parts[1]));
    if (parts[2]) snprintf(sc->url, sizeof(sc->url), "%s", trim(parts[2]));
}

int config_load(const char *path, struct config *out) {
    FILE *f;
    char line[1024];
    defaults(out);

    f = fopen(path, "r");
    if (!f) return -1;  /* missing file is fatal (see main) */

    while (fgets(line, sizeof(line), f)) {
        char *eq, *key, *val;
        strip_comment(line);
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        key = trim(line);
        val = unquote(trim(eq + 1));
        if (key[0] == '\0') continue;

        if (strcmp(key, "port") == 0) {
            out->port = atoi(val);
        } else if (strcmp(key, "bind") == 0) {
            snprintf(out->bind, sizeof(out->bind), "%s", val);
        } else if (strcmp(key, "bookmarks_dir") == 0) {
            snprintf(out->bookmarks_dir, sizeof(out->bookmarks_dir), "%s", val);
        } else if (strcmp(key, "tray_icon") == 0) {
            out->tray_icon = (strcmp(val, "enable") == 0);
        } else if (strcmp(key, "shortcuts_on_row") == 0) {
            out->shortcuts_on_row = atoi(val);
        } else if (strcmp(key, "rows") == 0) {
            out->rows = atoi(val);
        } else if (strcmp(key, "link_open_mode") == 0) {
            snprintf(out->link_open_mode, sizeof(out->link_open_mode), "%s", val);
        } else if (strncmp(key, "shortcut[", 9) == 0) {
            int idx = atoi(key + 9);
            if (idx >= 0 && idx < BMKD_MAX_SHORTCUTS) {
                parse_shortcut(val, &out->shortcuts[idx]);
                if (idx + 1 > out->shortcut_count) out->shortcut_count = idx + 1;
            }
        }
    }
    fclose(f);
    return 0;
}
