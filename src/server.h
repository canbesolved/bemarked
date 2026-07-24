/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BMKD_SERVER_H
#define BMKD_SERVER_H

/* Mongoose HTTP server: application-core boundary.
 *
 * Serves the embedded SPA and the internal JSON endpoints (v1, not yet a stable
 * public contract):
 *   GET/POST/PUT/DELETE /bookmarks[/{id}]
 *   GET /search?q=...   GET /config/public
 * Owns authoritative search and all security checks: loopback bind, Host/Origin
 * validation on mutating requests, HTML-escaping, CSP, URL scheme allowlist,
 * body/field size limits. Runs on a background worker thread. */
#include "config.h"
#include "storage.h"

struct server;  /* opaque */

struct server *server_start(const struct config *cfg, struct storage *storage);
void           server_stop(struct server *s);

#endif /* BMKD_SERVER_H */
