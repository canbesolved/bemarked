/* SPDX-License-Identifier: GPL-2.0-only */
/* Mongoose HTTP handlers, routing, and security enforcement.
 * The embedded SPA is served from the generated web_assets.h byte array. */
#include "server.h"
#include "mongoose.h"
/* Generated at build time from web/ (see CMakeLists.txt). Served same-origin:
 *   /            -> web_index_html
 *   /style.css   -> web_style_css
 *   /app.js      -> web_app_js  */
#include "web_index_html.h"
#include "web_style_css.h"
#include "web_app_js.h"

struct server *server_start(const struct config *cfg, struct storage *storage) {
    (void)cfg; (void)storage;
    (void)web_index_html; (void)web_index_html_len;
    (void)web_style_css;  (void)web_style_css_len;
    (void)web_app_js;     (void)web_app_js_len;
    /* TODO: mg_mgr_init, bind cfg->bind:cfg->port, register handlers, spawn
     * worker thread running the poll loop. */
    return 0;
}

void server_stop(struct server *s) { (void)s; }
