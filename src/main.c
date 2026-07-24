/* SPDX-License-Identifier: GPL-2.0-only */
/* bmkd — entry point.
 *
 * Loads conf.txt, initializes the TSV storage, starts the Mongoose HTTP server on
 * a background worker thread, then blocks on the main thread until shutdown.
 */
#include "config.h"
#include "storage.h"
#include "server.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *conf = argc > 1 ? argv[1] : "conf.txt";
    struct config cfg;
    struct storage *st;
    struct server *srv;

    srand((unsigned)(time(NULL) ^ (long)getpid()));  /* for model_new_id */

    if (config_load(conf, &cfg) != 0) {
        fprintf(stderr, "bmkd: config file '%s' not found\n", conf);
        return 1;
    }

    st = storage_open(cfg.bookmarks_dir);
    if (!st) {
        fprintf(stderr, "bmkd: cannot open storage dir '%s'\n", cfg.bookmarks_dir);
        return 1;
    }

    srv = server_start(&cfg, st);
    if (!srv) {
        fprintf(stderr, "bmkd: cannot listen on %s:%d (port in use?)\n",
                cfg.bind, cfg.port);
        storage_close(st);
        return 1;
    }

    printf("bmkd listening on http://%s:%d  (Ctrl-C to quit)\n", cfg.bind, cfg.port);
    fflush(stdout);

    platform_run(cfg.tray_icon, cfg.port);

    printf("\nbmkd: shutting down\n");
    server_stop(srv);
    storage_close(st);
    return 0;
}
