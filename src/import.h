/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BMKD_IMPORT_H
#define BMKD_IMPORT_H

/* Convert exported browser bookmarks (the Netscape Bookmark File format used by
 * Chrome/Firefox/Edge/Safari) into bemarked bookmarks. Hand-written tolerant
 * parser for the <DL>/<DT>/<H3>/<A HREF> structure — no HTML library. */
#include "model.h"
#include <stddef.h>

/* Parse HTML into a malloc'd array of sanitized bookmarks (ids left empty; only
 * valid http/https entries kept). Caller frees *out. *skipped counts dropped
 * (non-http/empty) entries. Returns 0 on success, -1 on allocation failure. */
int import_parse(const char *html, size_t len,
                  struct bookmark **out, int *count, int *skipped);

/* CLI: read Netscape HTML from in_path, write bookmarks.txt (with ids) to
 * out_path. Returns 0 on success, non-zero on error (prints diagnostics). */
int import_file(const char *in_path, const char *out_path);

#endif /* BMKD_IMPORT_H */
