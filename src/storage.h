/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BMKD_STORAGE_H
#define BMKD_STORAGE_H

/* Storage interface (v1 adapter: single-file TSV + in-memory index).
 *
 * Writes are crash-safe: full temp file -> fsync -> atomic rename, keeping one
 * .bak. A single writer is serialized via an internal mutex. External edits are
 * detected via mtime before overwrite. Future adapters (append-log for ESP32)
 * implement this same interface. */
#include "model.h"

struct storage;  /* opaque */

/* Open bookmarks.txt under dir, build the in-memory index. Returns NULL on error. */
struct storage *storage_open(const char *dir);
void          storage_close(struct storage *s);

/* Read side (served from the in-memory index). */
int storage_list(struct storage *s, struct bookmark **out, int *count);
int storage_get(struct storage *s, const char *id, struct bookmark *out);

/* Mutations -> validate, update index, atomic rewrite of the whole file. */
int storage_save(struct storage *s, const struct bookmark *b);   /* create or update by id */
int storage_delete(struct storage *s, const char *id);

/* True if a bookmark with this id exists (used to keep generated ids unique). */
int storage_id_exists(struct storage *s, const char *id);

#endif /* BMKD_STORAGE_H */
