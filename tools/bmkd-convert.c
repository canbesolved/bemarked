/* SPDX-License-Identifier: GPL-2.0-only */
/* bmkd-convert — convert an exported browser bookmarks file (the Netscape
 * Bookmark File format used by Chrome/Firefox/Edge/Safari) into bemarked's
 * bookmarks.txt (TAB-separated: id  name  folder  url).
 *
 * Self-contained: a hand-written tolerant parser for the well-defined
 * <DL>/<DT>/<H3>/<A HREF> structure — no HTML library, no external tools.
 * Reuses model.c so ids, sanitization and folder normalization match the app.
 *
 *   usage: bmkd-convert <bookmarks.html> [output.txt]   (default output: bookmarks.txt)
 */
#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* --- small helpers --- */

/* Case-insensitive: does p begin with the literal lit? */
static int ci_starts(const char *p, const char *lit) {
    for (; *lit; p++, lit++)
        if (tolower((unsigned char)*p) != tolower((unsigned char)*lit)) return 0;
    return 1;
}

/* Find literal lit (case-insensitive) within [s, e); return pointer or NULL. */
static const char *ci_find(const char *s, const char *e, const char *lit) {
    size_t n = strlen(lit);
    for (; s + n <= e; s++)
        if (ci_starts(s, lit)) return s;
    return NULL;
}

/* p points at '<'. Return pointer to the closing '>' (quote-aware) or NULL. */
static char *tag_end(char *p) {
    int inq = 0; char qc = 0; char *q;
    for (q = p + 1; *q; q++) {
        if (inq) { if (*q == qc) inq = 0; }
        else if (*q == '"' || *q == '\'') { inq = 1; qc = *q; }
        else if (*q == '>') return q;
    }
    return NULL;
}

/* Encode a Unicode code point as UTF-8 into dst. */
static void put_utf8(unsigned cp, char *dst, size_t *di, size_t dstsz) {
    unsigned char t[4]; int n, k;
    if (cp < 0x80) { t[0] = (unsigned char)cp; n = 1; }
    else if (cp < 0x800) { t[0] = 0xC0 | (cp >> 6); t[1] = 0x80 | (cp & 0x3F); n = 2; }
    else if (cp < 0x10000) { t[0] = 0xE0 | (cp >> 12); t[1] = 0x80 | ((cp >> 6) & 0x3F); t[2] = 0x80 | (cp & 0x3F); n = 3; }
    else { t[0] = 0xF0 | (cp >> 18); t[1] = 0x80 | ((cp >> 12) & 0x3F); t[2] = 0x80 | ((cp >> 6) & 0x3F); t[3] = 0x80 | (cp & 0x3F); n = 4; }
    for (k = 0; k < n; k++) if (*di + 1 < dstsz) dst[(*di)++] = (char)t[k];
}

/* Decode HTML entities in [s, e) into NUL-terminated dst. */
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

/* --- generated-id set (guarantee uniqueness across the whole file) --- */

typedef char genid[BMKD_ID_LEN + 1];
static genid *g_ids = NULL;
static size_t g_idn = 0, g_idc = 0;

static int id_seen(const char *id) {
    size_t i;
    for (i = 0; i < g_idn; i++) if (strcmp(g_ids[i], id) == 0) return 1;
    return 0;
}
static void id_add(const char *id) {
    if (g_idn == g_idc) {
        g_idc = g_idc ? g_idc * 2 : 256;
        g_ids = realloc(g_ids, g_idc * sizeof(*g_ids));
    }
    snprintf(g_ids[g_idn++], BMKD_ID_LEN + 1, "%s", id);
}

int main(int argc, char **argv) {
    const char *inpath, *outpath;
    FILE *f, *out;
    long sz;
    char *buf, *p;
    char stack[64][256];        /* folder path stack (per open <DL>) */
    int depth = 0;
    char pending[256] = "";     /* last <H3> name, consumed by the next <DL> */
    int have_pending = 0;
    size_t count = 0, skipped = 0;

    inpath = argc > 1 ? argv[1] : "bookmarks.html";   /* default input */
    outpath = argc > 2 ? argv[2] : "bookmarks.txt";

    f = fopen(inpath, "rb");
    if (!f) {
        if (argc > 1) {
            perror(inpath);
        } else {
            fprintf(stderr,
                "bmkd-convert: no input file given and 'bookmarks.html' was not found "
                "in the current directory.\n"
                "usage: %s [bookmarks.html] [output.txt]\n", argv[0]);
        }
        return 1;
    }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return 1; }
    buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return 1; }
    buf[sz] = '\0';
    fclose(f);

    out = fopen(outpath, "wb");
    if (!out) { perror(outpath); free(buf); return 1; }
    fputs("#\tid\tname\tfolder\turl\n", out);

    srand((unsigned)time(NULL));

    p = buf;
    while ((p = strchr(p, '<')) != NULL) {
        char *q = tag_end(p);
        char *name;
        if (!q) break;
        name = p + 1;
        while (*name == ' ' || *name == '\t' || *name == '\n' || *name == '\r') name++;

        if (*name == '/') {                    /* closing tag: only </DL> matters */
            char *nm = name + 1;
            while (*nm == ' ') nm++;
            if (ci_starts(nm, "dl") && depth > 0) depth--;
            p = q + 1;
            continue;
        }
        if (ci_starts(name, "dl") && !isalnum((unsigned char)name[2])) {   /* open a level */
            if (depth < 64) {
                if (have_pending) snprintf(stack[depth], sizeof(stack[0]), "%s", pending);
                else stack[depth][0] = '\0';   /* the root <DL> maps to no folder */
                depth++;
            }
            have_pending = 0; pending[0] = '\0';
            p = q + 1;
            continue;
        }
        if (ci_starts(name, "h3") && !isalnum((unsigned char)name[2])) {   /* folder name */
            const char *ce = ci_find(q + 1, buf + sz, "</h3>");
            decode(q + 1, ce ? ce : buf + sz, pending, sizeof(pending));
            have_pending = 1;
            p = ce ? (char *)ce + 5 : q + 1;
            continue;
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

            if (hp) {                          /* extract HREF="..." (or '...') */
                const char *eq = memchr(hp, '=', (size_t)(q - hp));
                if (eq) {
                    const char *v = eq + 1;
                    char quote = 0;
                    const char *ve;
                    while (v < q && (*v == ' ' || *v == '\t')) v++;
                    if (v < q && (*v == '"' || *v == '\'')) { quote = *v; v++; }
                    if (quote) { ve = memchr(v, quote, (size_t)(q - v)); if (!ve) ve = q; }
                    else { ve = v; while (ve < q && *ve != ' ' && *ve != '>') ve++; }
                    decode(v, ve, b.url, sizeof(b.url));
                }
            }
            ce = ci_find(q + 1, buf + sz, "</a>");
            decode(q + 1, ce ? ce : buf + sz, b.name, sizeof(b.name));

            for (i = 0; i < depth; i++) {      /* folder = non-empty stack entries joined by '/' */
                const char *c;
                if (!stack[i][0]) continue;
                if (pl && pl + 1 < sizeof(folder)) folder[pl++] = '/';
                for (c = stack[i]; *c && pl + 1 < sizeof(folder); c++) folder[pl++] = *c;
            }
            folder[pl] = '\0';
            snprintf(b.folder, sizeof(b.folder), "%s", folder);

            if (b.name[0] == '\0') snprintf(b.name, sizeof(b.name), "%s", b.url);
            do { model_new_id(b.id); } while (id_seen(b.id));   /* id needed by validate */
            model_sanitize(&b);

            if (b.url[0] == '\0' || model_validate(&b) != 0) {   /* skip non-http / empty */
                skipped++;
            } else {
                id_add(b.id);
                fprintf(out, "%s\t%s\t%s\t%s\n", b.id, b.name, b.folder, b.url);
                count++;
            }
            p = ce ? (char *)ce + 4 : q + 1;
            continue;
        }
        p = q + 1;
    }

    fclose(out);
    free(buf);
    free(g_ids);
    fprintf(stderr, "bmkd-convert: wrote %zu bookmark(s) to %s (%zu skipped)\n",
            count, outpath, skipped);
    return 0;
}
