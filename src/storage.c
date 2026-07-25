/* SPDX-License-Identifier: GPL-2.0-only */
/* TSV storage adapter: in-memory index + crash-safe atomic writes.
 *
 * File format (TAB-separated, fixed columns), first line is a comment header:
 *   #<TAB>id<TAB>name<TAB>folder<TAB>url
 * Write path: build full contents -> write bookmarks.tsv.tmp (same dir) ->
 * fflush + fsync -> move old to bookmarks.tsv.bak -> rename() over target. */
#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#define TSV_HEADER "#\tid\tname\tfolder\turl\n"

struct storage {
    char path[1024];
    char tmp[1024];
    char bak[1024];
    struct bookmark *items;
    size_t count, cap;
    pthread_mutex_t lock;
};

/* --- TSV parsing --- */

static void copy_field(char *dst, size_t dstsz, const char *src, size_t n) {
    if (n >= dstsz) n = dstsz - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* Parse one TSV line (newline already stripped) into b. Returns 0 on success. */
static int parse_line(char *line, struct bookmark *b) {
    char *f[4];
    int i = 0;
    char *p = line;
    memset(b, 0, sizeof(*b));
    f[i++] = p;
    while (i < 4 && (p = strchr(p, '\t')) != NULL) {
        *p++ = '\0';
        f[i++] = p;
    }
    if (i < 4) return -1;  /* not enough columns */
    copy_field(b->id, sizeof(b->id), f[0], strlen(f[0]));
    copy_field(b->name, sizeof(b->name), f[1], strlen(f[1]));
    copy_field(b->folder, sizeof(b->folder), f[2], strlen(f[2]));
    copy_field(b->url, sizeof(b->url), f[3], strlen(f[3]));
    return b->id[0] ? 0 : -1;
}

static int push(struct storage *s, const struct bookmark *b) {
    if (s->count == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 16;
        struct bookmark *n = realloc(s->items, ncap * sizeof(*n));
        if (!n) return -1;
        s->items = n;
        s->cap = ncap;
    }
    s->items[s->count++] = *b;
    return 0;
}

static void load(struct storage *s) {
    FILE *f = fopen(s->path, "r");
    char line[4096];
    if (!f) return;  /* absent file = empty store */
    while (fgets(line, sizeof(line), f)) {
        struct bookmark b;
        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') line[--len] = '\0';
        if (len && line[len - 1] == '\r') line[--len] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        if (parse_line(line, &b) == 0) push(s, &b);
    }
    fclose(f);
}

/* --- atomic full-file write (caller holds lock) --- */

static int flush_locked(struct storage *s) {
    FILE *f = fopen(s->tmp, "w");
    size_t i;
    if (!f) return -1;
    if (fputs(TSV_HEADER, f) < 0) goto fail;
    for (i = 0; i < s->count; i++) {
        struct bookmark *b = &s->items[i];
        if (fprintf(f, "%s\t%s\t%s\t%s\n", b->id, b->name, b->folder, b->url) < 0)
            goto fail;
    }
    if (fflush(f) != 0) goto fail;
    if (fsync(fileno(f)) != 0) goto fail;
    if (fclose(f) != 0) return -1;

    /* keep previous good copy as .bak, then atomically replace */
    if (access(s->path, F_OK) == 0) rename(s->path, s->bak);
    if (rename(s->tmp, s->path) != 0) return -1;
    return 0;
fail:
    fclose(f);
    unlink(s->tmp);
    return -1;
}

/* --- public API --- */

struct storage *storage_open(const char *dir) {
    struct storage *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    mkdir(dir, 0755);  /* ignore EEXIST */
    snprintf(s->path, sizeof(s->path), "%s/bookmarks.tsv", dir);
    snprintf(s->tmp,  sizeof(s->tmp),  "%s/bookmarks.tsv.tmp", dir);
    snprintf(s->bak,  sizeof(s->bak),  "%s/bookmarks.tsv.bak", dir);
    pthread_mutex_init(&s->lock, NULL);
    load(s);

    /* migrate legacy rows with no folder -> "unsorted" so they show in the tree */
    {
        size_t i, fixed = 0;
        for (i = 0; i < s->count; i++) {
            if (s->items[i].folder[0] == '\0') {
                strcpy(s->items[i].folder, "unsorted");
                fixed++;
            }
        }
        if (fixed > 0) {
            flush_locked(s);  /* single-threaded at open time */
            printf("bmkd: assigned 'unsorted' folder to %zu bookmark(s) that had none\n", fixed);
            fflush(stdout);
        }
    }
    return s;
}

void storage_close(struct storage *s) {
    if (!s) return;
    pthread_mutex_destroy(&s->lock);
    free(s->items);
    free(s);
}

int storage_list(struct storage *s, struct bookmark **out, int *count) {
    *out = s->items;
    *count = (int)s->count;
    return 0;
}

static struct bookmark *find(struct storage *s, const char *id) {
    size_t i;
    for (i = 0; i < s->count; i++)
        if (strcmp(s->items[i].id, id) == 0) return &s->items[i];
    return NULL;
}

int storage_get(struct storage *s, const char *id, struct bookmark *out) {
    struct bookmark *b;
    int rc = -1;
    pthread_mutex_lock(&s->lock);
    if ((b = find(s, id)) != NULL) { *out = *b; rc = 0; }
    pthread_mutex_unlock(&s->lock);
    return rc;
}

int storage_save(struct storage *s, const struct bookmark *b) {
    struct bookmark *existing;
    int rc;
    pthread_mutex_lock(&s->lock);
    existing = find(s, b->id);
    if (existing) {
        *existing = *b;
    } else if (push(s, b) != 0) {
        pthread_mutex_unlock(&s->lock);
        return -1;
    }
    rc = flush_locked(s);
    pthread_mutex_unlock(&s->lock);
    return rc;
}

int storage_delete(struct storage *s, const char *id) {
    size_t i;
    int rc = -1;
    pthread_mutex_lock(&s->lock);
    for (i = 0; i < s->count; i++) {
        if (strcmp(s->items[i].id, id) == 0) {
            s->items[i] = s->items[--s->count];  /* order not significant */
            rc = flush_locked(s);
            break;
        }
    }
    pthread_mutex_unlock(&s->lock);
    return rc;
}

int storage_id_exists(struct storage *s, const char *id) {
    int rc;
    pthread_mutex_lock(&s->lock);
    rc = find(s, id) != NULL;
    pthread_mutex_unlock(&s->lock);
    return rc;
}
