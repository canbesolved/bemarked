/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BMKD_MODEL_H
#define BMKD_MODEL_H

/* Bookmark domain model.
 * Schema (TSV columns, fixed order): id  name  folder  url
 * Folder is a '/'-separated path encoding subfolders; matching is case-sensitive. */

#define BMKD_ID_LEN     8      /* 8 hex chars */
#define BMKD_NAME_MAX   200
#define BMKD_FOLDER_MAX 512
#define BMKD_URL_MAX    2048

struct bookmark {
    char id[BMKD_ID_LEN + 1];
    char name[BMKD_NAME_MAX + 1];
    char folder[BMKD_FOLDER_MAX + 1];  /* e.g. "dev/sources/github" */
    char url[BMKD_URL_MAX + 1];
};

/* Generate a new stable 8-hex-char id (not derived from name). */
void model_new_id(char out[BMKD_ID_LEN + 1]);

/* Validate a bookmark for write: required id+name, url scheme allowlist,
 * folder normalization (no leading/trailing slash, no empty segments),
 * reject raw TAB/newline. Returns 0 if valid. */
int model_validate(const struct bookmark *b);

#endif /* BMKD_MODEL_H */
