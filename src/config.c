/* SPDX-License-Identifier: GPL-2.0-only */
/* conf.txt parser implementation. Quote-aware so hex color codes (#RRGGBB) inside
 * quoted strings are not truncated by inline # comments. */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

static void defaults(struct config *c) {
    memset(c, 0, sizeof(*c));
    c->port = 8989;
    snprintf(c->bind, sizeof(c->bind), "127.0.0.1");
    snprintf(c->bookmarks_dir, sizeof(c->bookmarks_dir), "./data");
    c->tray_icon = 1;
    c->shortcuts_per_row = 7;
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
    snprintf(out->conf_path, sizeof(out->conf_path), "%s", path);

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
        } else if (strcmp(key, "shortcuts_per_row") == 0) {
            out->shortcuts_per_row = atoi(val);
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

    /* compact to a dense array (indices 0..n-1, no gaps) */
    {
        int w = 0, r;
        for (r = 0; r < out->shortcut_count; r++) {
            if (out->shortcuts[r].name[0]) {
                if (w != r) out->shortcuts[w] = out->shortcuts[r];
                w++;
            }
        }
        for (r = w; r < out->shortcut_count; r++)
            memset(&out->shortcuts[r], 0, sizeof(struct shortcut));
        out->shortcut_count = w;
    }
    return 0;
}

/* --- shortcut persistence --- */

/* Strip characters that would break the `Name | #hex | url` shortcut syntax. */
static void sc_clean(char *dst, size_t sz, const char *src) {
    char tmp[1024];
    size_t j = 0;
    char *b, *e;
    if (!src) src = "";
    for (; *src && j + 1 < sizeof(tmp); src++) {
        char ch = *src;
        if (ch == '|' || ch == '"' || ch == '\t' || ch == '\n' || ch == '\r') continue;
        tmp[j++] = ch;
    }
    tmp[j] = '\0';
    b = tmp;
    e = tmp + strlen(tmp);
    while (*b == ' ') b++;
    while (e > b && e[-1] == ' ') *--e = '\0';
    snprintf(dst, sz, "%s", b);
}

static void write_shortcuts(FILE *f, const struct config *c) {
    int i;
    for (i = 0; i < c->shortcut_count; i++) {
        const struct shortcut *s = &c->shortcuts[i];
        if (!s->name[0]) continue;
        fprintf(f, "shortcut[%d] = \"%s | %s | %s\"\n", i, s->name, s->color, s->url);
    }
}

/* Rewrite conf.txt: keep every line except the `shortcut[N] = ...` assignments,
 * which are replaced (in place of the first one) by the current dense set. */
static int config_save(const struct config *c) {
    char tmp[600];
    FILE *in, *out;
    char line[2048];
    int wrote = 0;

    if (!c->conf_path[0]) return -1;
    snprintf(tmp, sizeof(tmp), "%s.tmp", c->conf_path);
    out = fopen(tmp, "w");
    if (!out) return -1;

    in = fopen(c->conf_path, "r");
    if (in) {
        while (fgets(line, sizeof(line), in)) {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p != '#' && strncmp(p, "shortcut[", 9) == 0 && strchr(p, '=')) {
                if (!wrote) { write_shortcuts(out, c); wrote = 1; }
                continue;  /* drop old assignment */
            }
            fputs(line, out);
        }
        fclose(in);
    }
    if (!wrote) write_shortcuts(out, c);

    if (fflush(out) != 0 || fsync(fileno(out)) != 0) { fclose(out); unlink(tmp); return -1; }
    if (fclose(out) != 0) { unlink(tmp); return -1; }
    if (rename(tmp, c->conf_path) != 0) { unlink(tmp); return -1; }
    return 0;
}

int config_add_shortcut(struct config *c, const char *name,
                        const char *color, const char *url) {
    struct shortcut *s;
    if (c->shortcut_count >= BMKD_MAX_SHORTCUTS) return -1;
    s = &c->shortcuts[c->shortcut_count];
    memset(s, 0, sizeof(*s));
    sc_clean(s->name, sizeof(s->name), name);
    sc_clean(s->color, sizeof(s->color), color);
    sc_clean(s->url, sizeof(s->url), url);
    if (!s->name[0]) return -1;
    c->shortcut_count++;
    if (config_save(c) != 0) { c->shortcut_count--; return -1; }
    return c->shortcut_count - 1;
}

int config_delete_shortcut(struct config *c, int index) {
    int j;
    if (index < 0 || index >= c->shortcut_count) return -1;
    for (j = index; j < c->shortcut_count - 1; j++)
        c->shortcuts[j] = c->shortcuts[j + 1];
    c->shortcut_count--;
    memset(&c->shortcuts[c->shortcut_count], 0, sizeof(struct shortcut));
    return config_save(c);
}

int config_update_shortcut(struct config *c, int index, const char *name,
                           const char *color, const char *url) {
    struct shortcut *s, old;
    if (index < 0 || index >= c->shortcut_count) return -1;
    s = &c->shortcuts[index];
    old = *s;
    memset(s, 0, sizeof(*s));
    sc_clean(s->name, sizeof(s->name), name);
    sc_clean(s->color, sizeof(s->color), color);
    sc_clean(s->url, sizeof(s->url), url);
    if (!s->name[0] || config_save(c) != 0) { *s = old; return -1; }
    return 0;
}

int config_swap_shortcut(struct config *c, int a, int b) {
    struct shortcut tmp;
    if (a < 0 || a >= c->shortcut_count) return -1;
    if (b < 0 || b >= c->shortcut_count) return -1;
    if (a == b) return 0;
    tmp = c->shortcuts[a];
    c->shortcuts[a] = c->shortcuts[b];
    c->shortcuts[b] = tmp;
    return config_save(c);
}
