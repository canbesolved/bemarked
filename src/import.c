/* SPDX-License-Identifier: GPL-2.0-only */
/* Netscape bookmark HTML -> bemarked bookmarks. Shared by the `bmkd convert`
 * CLI subcommand and the web /import endpoint. Reuses model.c so ids,
 * sanitization and folder normalization match the daemon. */
#include "import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* --- small helpers --- */

static int ci_starts(const char *p, const char *lit) {
    for (; *lit; p++, lit++)
        if (tolower((unsigned char)*p) != tolower((unsigned char)*lit)) return 0;
    return 1;
}
static const char *ci_find(const char *s, const char *e, const char *lit) {
    size_t n = strlen(lit);
    for (; s + n <= e; s++)
        if (ci_starts(s, lit)) return s;
    return NULL;
}
static char *tag_end(char *p) {
    int inq = 0; char qc = 0; char *q;
    for (q = p + 1; *q; q++) {
        if (inq) { if (*q == qc) inq = 0; }
        else if (*q == '"' || *q == '\'') { inq = 1; qc = *q; }
        else if (*q == '>') return q;
    }
    return NULL;
}
static void put_utf8(unsigned cp, char *dst, size_t *di, size_t dstsz) {
    unsigned char t[4]; int n, k;
    if (cp < 0x80) { t[0] = (unsigned char)cp; n = 1; }
    else if (cp < 0x800) { t[0] = 0xC0 | (cp >> 6); t[1] = 0x80 | (cp & 0x3F); n = 2; }
    else if (cp < 0x10000) { t[0] = 0xE0 | (cp >> 12); t[1] = 0x80 | ((cp >> 6) & 0x3F); t[2] = 0x80 | (cp & 0x3F); n = 3; }
    else { t[0] = 0xF0 | (cp >> 18); t[1] = 0x80 | ((cp >> 12) & 0x3F); t[2] = 0x80 | ((cp >> 6) & 0x3F); t[3] = 0x80 | (cp & 0x3F); n = 4; }
    for (k = 0; k < n; k++) if (*di + 1 < dstsz) dst[(*di)++] = (char)t[k];
}
static void decode(const char *s, const char *e, char *dst, size_t dstsz) {
    size_t di = 0;
    while (s < e && di + 1 < dstsz) {
        if (*s == '&') {
            const char *semi = memchr(s, ';', (size_t)(e - s));
            if (semi && semi - s <= 12) {
                if (ci_starts(s, "&amp;"))  { dst[di++] = '&';  s = semi + 1; continue; }
                if (ci_starts(s, "&lt;"))   { dst[di++] = '<';  s = semi + 1; continue; }
                if (ci_starts(s, "&gt;"))   { dst[di++] = '>';  s = semi + 1; continue; }
                if (ci_starts(s, "&quot;")) { dst[di++] = '"';  s = semi + 1; continue; }
                if (ci_starts(s, "&apos;")) { dst[di++] = '\''; s = semi + 1; continue; }
                if (s[1] == '#') {
                    const char *d = s + 2;
                    int hex = (*d == 'x' || *d == 'X'), any = 0, bad = 0;
                    unsigned cp = 0;
                    if (hex) d++;
                    for (; d < semi; d++) {
                        char c = *d; int v;
                        if (c >= '0' && c <= '9') v = c - '0';
                        else if (hex && c >= 'a' && c <= 'f') v = c - 'a' + 10;
                        else if (hex && c >= 'A' && c <= 'F') v = c - 'A' + 10;
                        else { bad = 1; break; }
                        cp = cp * (hex ? 16u : 10u) + (unsigned)v; any = 1;
                    }
                    if (!bad && any && cp) { put_utf8(cp, dst, &di, dstsz); s = semi + 1; continue; }
                }
            }
        }
        dst[di++] = *s++;
    }
    dst[di] = '\0';
}

static int is_http(const char *u) {
    return strncmp(u, "http://", 7) == 0 || strncmp(u, "https://", 8) == 0;
}

static int push_bm(struct bookmark **arr, int *n, int *cap, const struct bookmark *b) {
    if (*n == *cap) {
        int nc = *cap ? *cap * 2 : 64;
        struct bookmark *na = realloc(*arr, (size_t)nc * sizeof(**arr));
        if (!na) return -1;
        *arr = na; *cap = nc;
    }
    (*arr)[(*n)++] = *b;
    return 0;
}

int import_parse(const char *html, size_t len,
                  struct bookmark **out, int *count, int *skipped) {
    char *buf, *p, *end;
    char stack[64][256];
    int depth = 0, cap = 0, n = 0, skip = 0;
    char pending[256] = "";
    int have_pending = 0;
    struct bookmark *arr = NULL;

    *out = NULL; *count = 0; if (skipped) *skipped = 0;
    buf = malloc(len + 1);
    if (!buf) return -1;
    memcpy(buf, html, len);
    buf[len] = '\0';
    end = buf + len;

    p = buf;
    while ((p = strchr(p, '<')) != NULL) {
        char *q = tag_end(p);
        char *name;
        if (!q) break;
        name = p + 1;
        while (*name == ' ' || *name == '\t' || *name == '\n' || *name == '\r') name++;

        if (*name == '/') {
            char *nm = name + 1;
            while (*nm == ' ') nm++;
            if (ci_starts(nm, "dl") && depth > 0) depth--;
            p = q + 1; continue;
        }
        if (ci_starts(name, "dl") && !isalnum((unsigned char)name[2])) {
            if (depth < 64) {
                if (have_pending) snprintf(stack[depth], sizeof(stack[0]), "%s", pending);
                else stack[depth][0] = '\0';
                depth++;
            }
            have_pending = 0; pending[0] = '\0';
            p = q + 1; continue;
        }
        if (ci_starts(name, "h3") && !isalnum((unsigned char)name[2])) {
            const char *ce = ci_find(q + 1, end, "</h3>");
            decode(q + 1, ce ? ce : end, pending, sizeof(pending));
            have_pending = 1;
            p = ce ? (char *)ce + 5 : q + 1; continue;
        }
        if ((name[0] == 'a' || name[0] == 'A') &&
            (name[1] == ' ' || name[1] == '\t' || name[1] == '\n' || name[1] == '\r' || name[1] == '>')) {
            struct bookmark b;
            const char *hp = ci_find(p, q, "href");
            const char *ce;
            char folder[BMKD_FOLDER_MAX + 1];
            size_t pl = 0;
            int i;
            memset(&b, 0, sizeof(b));

            if (hp) {
                const char *eq = memchr(hp, '=', (size_t)(q - hp));
                if (eq) {
                    const char *v = eq + 1, *ve;
                    char quote = 0;
                    while (v < q && (*v == ' ' || *v == '\t')) v++;
                    if (v < q && (*v == '"' || *v == '\'')) { quote = *v; v++; }
                    if (quote) { ve = memchr(v, quote, (size_t)(q - v)); if (!ve) ve = q; }
                    else { ve = v; while (ve < q && *ve != ' ' && *ve != '>') ve++; }
                    decode(v, ve, b.url, sizeof(b.url));
                }
            }
            ce = ci_find(q + 1, end, "</a>");
            decode(q + 1, ce ? ce : end, b.name, sizeof(b.name));

            for (i = 0; i < depth; i++) {
                const char *c;
                if (!stack[i][0]) continue;
                if (pl && pl + 1 < sizeof(folder)) folder[pl++] = '/';
                for (c = stack[i]; *c && pl + 1 < sizeof(folder); c++) folder[pl++] = *c;
            }
            folder[pl] = '\0';
            snprintf(b.folder, sizeof(b.folder), "%s", folder);
            if (b.name[0] == '\0') snprintf(b.name, sizeof(b.name), "%s", b.url);
            model_sanitize(&b);   /* id stays empty; caller assigns */

            if (b.url[0] == '\0' || !is_http(b.url)) {
                skip++;
            } else if (push_bm(&arr, &n, &cap, &b) != 0) {
                free(buf); free(arr); return -1;
            }
            p = ce ? (char *)ce + 4 : q + 1; continue;
        }
        p = q + 1;
    }
    free(buf);
    *out = arr; *count = n; if (skipped) *skipped = skip;
    return 0;
}

int import_file(const char *in_path, const char *out_path) {
    FILE *f, *out;
    long sz;
    char *buf;
    struct bookmark *arr = NULL;
    int count = 0, skipped = 0, i, idn = 0, idc = 0;
    char (*ids)[BMKD_ID_LEN + 1] = NULL;

    f = fopen(in_path, "rb");
    if (!f) { perror(in_path); return 1; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return 1; }
    buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return 1; }
    fclose(f);

    if (import_parse(buf, (size_t)sz, &arr, &count, &skipped) != 0) { free(buf); return 1; }
    free(buf);

    out = fopen(out_path, "wb");
    if (!out) { perror(out_path); free(arr); return 1; }
    fputs("#\tid\tname\tfolder\turl\n", out);
    srand((unsigned)time(NULL));
    for (i = 0; i < count; i++) {
        struct bookmark *b = &arr[i];
        int dup, j;
        do {
            model_new_id(b->id); dup = 0;
            for (j = 0; j < idn; j++) if (strcmp(ids[j], b->id) == 0) { dup = 1; break; }
        } while (dup);
        if (idn == idc) { idc = idc ? idc * 2 : 256; ids = realloc(ids, (size_t)idc * sizeof(*ids)); }
        snprintf(ids[idn++], BMKD_ID_LEN + 1, "%s", b->id);
        fprintf(out, "%s\t%s\t%s\t%s\n", b->id, b->name, b->folder, b->url);
    }
    fclose(out); free(arr); free(ids);
    fprintf(stderr, "bmkd: converted %d bookmark(s) -> %s (%d skipped)\n", count, out_path, skipped);
    return 0;
}
