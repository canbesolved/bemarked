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
#include "import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Default bookmarks file: "bookmarks.txt" next to the bmkd binary (via argv[0]).
 * Falls back to the current directory if argv[0] has no path component. */
static void default_bookmarks_file(const char *argv0, char *out, size_t sz) {
    const char *slash = strrchr(argv0, '/');
#ifdef _WIN32
    const char *bslash = strrchr(argv0, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
    if (slash) {
        int dirlen = (int)(slash - argv0) + 1;
        snprintf(out, sz, "%.*sbookmarks.txt", dirlen, argv0);
    } else {
        snprintf(out, sz, "bookmarks.txt");
    }
}

int main(int argc, char **argv) {
    const char *conf = argc > 1 ? argv[1] : "conf.txt";
    struct config cfg;
    struct storage *st;
    struct server *srv;

    /* subcommand: bmkd convert [bookmarks.html] [bookmarks.txt] */
    if (argc >= 2 && strcmp(argv[1], "convert") == 0) {
        const char *in  = argc > 2 ? argv[2] : "bookmarks.html";
        const char *out = argc > 3 ? argv[3] : "bookmarks.txt";
        return import_file(in, out);
    }

    srand((unsigned)(time(NULL) ^ (long)getpid()));  /* for model_new_id */

    if (config_load(conf, &cfg) != 0) {
        fprintf(stderr, "bmkd: config file '%s' not found\n", conf);
        return 1;
    }
    if (cfg.bookmarks_file[0] == '\0')   /* not set in conf -> next to the binary */
        default_bookmarks_file(argv[0], cfg.bookmarks_file, sizeof(cfg.bookmarks_file));

    st = storage_open(cfg.bookmarks_file);
    if (!st) {
        fprintf(stderr, "bmkd: cannot open bookmarks file '%s'\n", cfg.bookmarks_file);
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

    platform_run();

    printf("\nbmkd: shutting down\n");
    server_stop(srv);
    storage_close(st);
    return 0;
}
