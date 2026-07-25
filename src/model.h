/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BMKD_MODEL_H
#define BMKD_MODEL_H

/* Bookmark domain model.
 * Schema (TSV columns, fixed order): id  name  folder  url
 * Folder is a '/'-separated path encoding subfolders; matching is case-sensitive. */

#define BMKD_ID_LEN     5      /* 5 hex chars (~1M unique ids) */
#define BMKD_NAME_MAX   200
#define BMKD_FOLDER_MAX 512
#define BMKD_URL_MAX    2048

struct bookmark {
    char id[BMKD_ID_LEN + 1];
    char name[BMKD_NAME_MAX + 1];
    char folder[BMKD_FOLDER_MAX + 1];  /* e.g. "dev/sources/github" */
    char url[BMKD_URL_MAX + 1];
};

/* Generate a new stable 5-hex-char id (not derived from name). */
void model_new_id(char out[BMKD_ID_LEN + 1]);

/* Clean a bookmark in place for storage: strip raw TAB/CR/LF from every field
 * (they would corrupt the TSV) and normalize the folder path — no leading/
 * trailing slash, no empty or whitespace-only segments. */
void model_sanitize(struct bookmark *b);

/* Validate for write. Returns 0 if valid, non-zero otherwise:
 *   -1 missing id or name
 *   -2 url scheme not http/https (empty url is allowed) */
int model_validate(const struct bookmark *b);

#endif /* BMKD_MODEL_H */
