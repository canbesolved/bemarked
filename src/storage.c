/* SPDX-License-Identifier: GPL-2.0-only */
/* TSV storage adapter: in-memory index + crash-safe atomic writes.
 *
 * File format (TAB-separated, fixed columns), first line is a versioned header:
 *   # bmkd v1<TAB>id<TAB>name<TAB>folder<TAB>url
 * Write path: build full contents -> write bookmarks.tsv.tmp (same dir) ->
 * fflush + fsync -> keep bookmarks.tsv.bak -> rename() over target. */
#include "storage.h"

struct storage *storage_open(const char *dir) {
    (void)dir;
    /* TODO: open/parse TSV, record mtime, build index, init write mutex. */
    return 0;
}

void storage_close(struct storage *s) { (void)s; }

int storage_list(struct storage *s, struct bookmark **out, int *count) {
    (void)s; (void)out; (void)count; return 0;
}

int storage_get(struct storage *s, const char *id, struct bookmark *out) {
    (void)s; (void)id; (void)out; return 0;
}

int storage_save(struct storage *s, const struct bookmark *b) {
    (void)s; (void)b;
    /* TODO: validate -> mtime check -> update index -> atomic rewrite. */
    return 0;
}

int storage_delete(struct storage *s, const char *id) {
    (void)s; (void)id; return 0;
}
