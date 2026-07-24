/* SPDX-License-Identifier: GPL-2.0-only */
/* bmkd — entry point.
 *
 * Loads conf.txt, initializes the TSV storage, starts the Mongoose HTTP server on a
 * background worker thread, then runs the desktop tray/UI loop on the main thread.
 */
#include "config.h"
#include "storage.h"
#include "server.h"
#include "platform.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* TODO:
     *   1. config_load(conf.txt)
     *   2. storage_open(bookmarks_dir)  -> build in-memory index
     *   3. server_start(&cfg, &storage) -> spawn worker thread
     *   4. platform_run_tray(...)     -> main-thread UI loop (or block on headless)
     *   5. graceful shutdown
     */
    return 0;
}
