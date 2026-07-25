/* SPDX-License-Identifier: GPL-2.0-only */
/* Bookmark model: id generation, sanitization, folder-path normalization. */
#include "model.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void model_new_id(char out[BMKD_ID_LEN + 1]) {
    static const char hex[] = "0123456789abcdef";
    int i;
    for (i = 0; i < BMKD_ID_LEN; i++) out[i] = hex[rand() & 0xf];
    out[BMKD_ID_LEN] = '\0';
}

/* Replace TAB/CR/LF with spaces so a field can never break the TSV layout. */
static void strip_control(char *s) {
    for (; *s; s++) {
        if (*s == '\t' || *s == '\r' || *s == '\n') *s = ' ';
    }
}

/* Collapse a '/'-separated path: drop leading/trailing slashes, empty and
 * whitespace-only segments, and repeated slashes. Rewrites in place. */
static void normalize_folder(char *folder) {
    char out[BMKD_FOLDER_MAX + 1];
    size_t oi = 0;
    char *seg, *save;
    int first = 1;

    for (seg = strtok_r(folder, "/", &save); seg;
         seg = strtok_r(NULL, "/", &save)) {
        /* trim leading/trailing whitespace of the segment */
        char *b = seg;
        char *e = seg + strlen(seg);
        while (*b && isspace((unsigned char)*b)) b++;
        while (e > b && isspace((unsigned char)e[-1])) e--;
        if (e == b) continue;  /* empty/whitespace-only: skip */

        if (!first && oi < sizeof(out) - 1) out[oi++] = '/';
        first = 0;
        while (b < e && oi < sizeof(out) - 1) out[oi++] = *b++;
    }
    out[oi] = '\0';
    memcpy(folder, out, oi + 1);
}

void model_sanitize(struct bookmark *b) {
    strip_control(b->name);
    strip_control(b->folder);
    strip_control(b->url);
    normalize_folder(b->folder);
    if (b->folder[0] == '\0')            /* no folder -> default so it shows in the tree */
        strcpy(b->folder, "unsorted");
}

static int is_http_url(const char *u) {
    return strncmp(u, "http://", 7) == 0 || strncmp(u, "https://", 8) == 0;
}

int model_validate(const struct bookmark *b) {
    if (b->id[0] == '\0' || b->name[0] == '\0') return -1;
    if (b->url[0] != '\0' && !is_http_url(b->url)) return -2;
    return 0;
}
