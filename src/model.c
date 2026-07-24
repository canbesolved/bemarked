/* SPDX-License-Identifier: GPL-2.0-only */
/* Bookmark model: id generation, validation, folder-path normalization. */
#include "model.h"

void model_new_id(char out[BMKD_ID_LEN + 1]) {
    (void)out;
    /* TODO: 8 random hex chars, checked for uniqueness against the index. */
}

int model_validate(const struct bookmark *b) {
    (void)b;
    /* TODO: required id+name; url scheme in {http,https}; normalize folder;
     * reject raw TAB/newline in any field. */
    return 0;
}
