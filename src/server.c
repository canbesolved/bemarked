/* SPDX-License-Identifier: GPL-2.0-only */
/* Mongoose HTTP handlers, routing, and security enforcement.
 * The embedded SPA is served from the generated web asset byte arrays. */
#include "server.h"
#include "mongoose.h"
/* Generated at build time from web/ (see CMakeLists.txt). Served same-origin:
 *   /            -> web_index_html
 *   /style.css   -> web_style_css
 *   /app.js      -> web_app_js  */
#include "web_index_html.h"
#include "web_style_css.h"
#include "web_app_js.h"
#include "folder_icon_svg.h"
#include "arrow_icon_svg.h"
#include "arrow_down_icon_svg.h"
#include "add_icon_svg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#define MAX_BODY (64 * 1024)

struct server {
    struct mg_mgr mgr;
    pthread_t thread;
    volatile int running;
    struct config *cfg;
    struct storage *st;
    struct mg_connection *listener;
};

/* --- tiny growable string buffer --- */

struct sb { char *buf; size_t len, cap; };

static void sb_ensure(struct sb *s, size_t extra) {
    if (s->len + extra + 1 > s->cap) {
        size_t ncap = s->cap ? s->cap : 256;
        while (s->len + extra + 1 > ncap) ncap *= 2;
        s->buf = realloc(s->buf, ncap);
        s->cap = ncap;
    }
}
static void sb_add(struct sb *s, const char *p, size_t n) {
    sb_ensure(s, n);
    memcpy(s->buf + s->len, p, n);
    s->len += n;
    s->buf[s->len] = '\0';
}
static void sb_str(struct sb *s, const char *p) { sb_add(s, p, strlen(p)); }

/* Case-insensitive "does haystack contain needle" (ASCII). */
static int ci_contains(const char *hay, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0) return 1;
    for (; *hay; hay++) {
        size_t i = 0;
        while (i < nl && hay[i] &&
               tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == nl) return 1;
    }
    return 0;
}

/* Append a JSON-escaped string (without surrounding quotes). */
static void sb_json(struct sb *s, const char *p) {
    for (; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  sb_str(s, "\\\""); break;
            case '\\': sb_str(s, "\\\\"); break;
            case '\n': sb_str(s, "\\n");  break;
            case '\r': sb_str(s, "\\r");  break;
            case '\t': sb_str(s, "\\t");  break;
            default:
                if (c < 0x20) {
                    char u[8];
                    snprintf(u, sizeof(u), "\\u%04x", c);
                    sb_str(s, u);
                } else {
                    sb_add(s, (char *)&c, 1);
                }
        }
    }
}

static void sb_field(struct sb *s, const char *key, const char *val, int last) {
    sb_str(s, "\"");
    sb_str(s, key);
    sb_str(s, "\":\"");
    sb_json(s, val);
    sb_str(s, last ? "\"" : "\",");
}

static void bookmark_json(struct sb *s, const struct bookmark *b) {
    sb_str(s, "{");
    sb_field(s, "id", b->id, 0);
    sb_field(s, "name", b->name, 0);
    sb_field(s, "folder", b->folder, 0);
    sb_field(s, "url", b->url, 1);
    sb_str(s, "}");
}

/* --- helpers --- */

static const char *JSON_HDRS =
    "Content-Type: application/json\r\n"
    "Cache-Control: no-store\r\n";

static void reply_err(struct mg_connection *c, int status, const char *msg) {
    mg_http_reply(c, status, JSON_HDRS, "{\"error\":\"%s\"}\n", msg);
}

/* A Host/Origin header value is loopback if its host part is localhost / 127.* / ::1. */
static int host_is_loopback(struct mg_str h) {
    /* strip scheme if present (Origin looks like "http://127.0.0.1:8989") */
    const char *p = h.buf;
    size_t n = h.len, i;
    for (i = 0; i + 2 < n; i++) {
        if (p[i] == '/' && p[i + 1] == '/') { p += i + 2; n -= i + 2; break; }
    }
    return (n >= 9 && strncmp(p, "localhost", 9) == 0) ||
           (n >= 9 && strncmp(p, "127.0.0.1", 9) == 0) ||
           (n >= 3 && strncmp(p, "::1", 3) == 0) ||
           (n >= 5 && strncmp(p, "[::1]", 5) == 0);
}

/* CSRF / DNS-rebinding guard for mutating requests: Host must be loopback, and
 * if an Origin header is present it must be loopback too. */
static int mutation_allowed(struct mg_http_message *hm) {
    struct mg_str *host = mg_http_get_header(hm, "Host");
    struct mg_str *origin = mg_http_get_header(hm, "Origin");
    if (!host || !host_is_loopback(*host)) return 0;
    if (origin && origin->len && !host_is_loopback(*origin)) return 0;
    return 1;
}

/* Copy a JSON string field into a fixed buffer. Missing field -> empty. */
static void json_field(struct mg_str body, const char *path, char *dst, size_t sz) {
    char *v = mg_json_get_str(body, path);
    dst[0] = '\0';
    if (v) {
        snprintf(dst, sz, "%s", v);
        mg_free(v);
    }
}

static void read_bookmark(struct mg_str body, struct bookmark *b) {
    memset(b, 0, sizeof(*b));
    json_field(body, "$.name", b->name, sizeof(b->name));
    json_field(body, "$.folder", b->folder, sizeof(b->folder));
    json_field(body, "$.url", b->url, sizeof(b->url));
}

/* --- route handlers --- */

static void serve_asset(struct mg_connection *c, const char *ctype,
                        const unsigned char *data, unsigned long len) {
    char hdr[128];
    snprintf(hdr, sizeof(hdr), "Content-Type: %s\r\n", ctype);
    mg_http_reply(c, 200, hdr, "%.*s", (int)len, (const char *)data);
}

static void list_bookmarks(struct mg_connection *c, struct storage *st,
                           const char *query) {
    struct bookmark *items;
    int count, i, first = 1;
    struct sb s = {0};
    char q[256] = {0};
    struct mg_str qs;

    if (query) {  /* /search?q=... : case-insensitive substring over all fields */
        qs = mg_str(query);
        mg_http_get_var(&qs, "q", q, sizeof(q));
    }

    storage_list(st, &items, &count);
    sb_str(&s, "[");
    for (i = 0; i < count; i++) {
        struct bookmark *b = &items[i];
        if (q[0]) {
            /* match if the query is a case-insensitive substring of any field */
            char hay[3200];
            snprintf(hay, sizeof(hay), "%s\n%s\n%s", b->name, b->folder, b->url);
            if (!ci_contains(hay, q)) continue;
        }
        if (!first) sb_str(&s, ",");
        first = 0;
        bookmark_json(&s, b);
    }
    sb_str(&s, "]\n");
    mg_http_reply(c, 200, JSON_HDRS, "%s", s.buf ? s.buf : "[]\n");
    free(s.buf);
}

static void create_bookmark(struct mg_connection *c, struct storage *st,
                            struct mg_http_message *hm) {
    struct bookmark b;
    struct sb s = {0};
    read_bookmark(hm->body, &b);
    do { model_new_id(b.id); } while (storage_id_exists(st, b.id));
    model_sanitize(&b);
    if (model_validate(&b) != 0) { reply_err(c, 400, "invalid bookmark"); return; }
    if (storage_save(st, &b) != 0) { reply_err(c, 500, "save failed"); return; }
    bookmark_json(&s, &b);
    mg_http_reply(c, 201, JSON_HDRS, "%s\n", s.buf);
    free(s.buf);
}

static void update_bookmark(struct mg_connection *c, struct storage *st,
                            const char *id, struct mg_http_message *hm) {
    struct bookmark b, existing;
    struct sb s = {0};
    if (storage_get(st, id, &existing) != 0) { reply_err(c, 404, "not found"); return; }
    read_bookmark(hm->body, &b);
    snprintf(b.id, sizeof(b.id), "%s", id);
    model_sanitize(&b);
    if (model_validate(&b) != 0) { reply_err(c, 400, "invalid bookmark"); return; }
    if (storage_save(st, &b) != 0) { reply_err(c, 500, "save failed"); return; }
    bookmark_json(&s, &b);
    mg_http_reply(c, 200, JSON_HDRS, "%s\n", s.buf);
    free(s.buf);
}

static void config_public(struct mg_connection *c, const struct config *cfg) {
    struct sb s = {0};
    int i, first = 1;
    char num[32];
    sb_str(&s, "{\"shortcuts_per_row\":");
    snprintf(num, sizeof(num), "%d", cfg->shortcuts_per_row); sb_str(&s, num);
    sb_str(&s, ",\"link_open_mode\":\"");
    sb_json(&s, cfg->link_open_mode);
    sb_str(&s, "\",\"shortcuts\":[");
    for (i = 0; i < cfg->shortcut_count; i++) {
        const struct shortcut *sc = &cfg->shortcuts[i];
        if (sc->name[0] == '\0') continue;
        if (!first) sb_str(&s, ",");
        first = 0;
        sb_str(&s, "{");
        sb_field(&s, "name", sc->name, 0);
        sb_field(&s, "color", sc->color, 0);
        sb_field(&s, "url", sc->url, 1);
        sb_str(&s, "}");
    }
    sb_str(&s, "]}\n");
    mg_http_reply(c, 200, JSON_HDRS, "%s", s.buf);
    free(s.buf);
}

static void shortcuts_list(struct mg_connection *c, const struct config *cfg) {
    struct sb s = {0};
    int i, first = 1;
    char num[16];
    sb_str(&s, "[");
    for (i = 0; i < cfg->shortcut_count; i++) {
        const struct shortcut *sc = &cfg->shortcuts[i];
        if (sc->name[0] == '\0') continue;
        if (!first) sb_str(&s, ",");
        first = 0;
        sb_str(&s, "{\"index\":");
        snprintf(num, sizeof(num), "%d", i); sb_str(&s, num);
        sb_str(&s, ",");
        sb_field(&s, "name", sc->name, 0);
        sb_field(&s, "color", sc->color, 0);
        sb_field(&s, "url", sc->url, 1);
        sb_str(&s, "}");
    }
    sb_str(&s, "]\n");
    mg_http_reply(c, 200, JSON_HDRS, "%s", s.buf ? s.buf : "[]\n");
    free(s.buf);
}

static void shortcuts_add(struct mg_connection *c, struct config *cfg,
                          struct mg_http_message *hm) {
    char name[128], color[16], url[1024];
    json_field(hm->body, "$.name", name, sizeof(name));
    json_field(hm->body, "$.color", color, sizeof(color));
    json_field(hm->body, "$.url", url, sizeof(url));
    if (name[0] == '\0') { reply_err(c, 400, "name required"); return; }
    if (config_add_shortcut(cfg, name, color, url) < 0) { reply_err(c, 500, "add failed"); return; }
    shortcuts_list(c, cfg);
}

/* --- dispatcher --- */

static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    struct server *srv = (struct server *)c->fn_data;
    struct mg_http_message *hm;
    struct mg_str caps[2];
    int is_get, is_post, is_put, is_delete, mutating;
    char id[BMKD_ID_LEN + 1];

    if (ev != MG_EV_HTTP_MSG) return;
    hm = (struct mg_http_message *)ev_data;

    is_get    = mg_strcmp(hm->method, mg_str("GET")) == 0;
    is_post   = mg_strcmp(hm->method, mg_str("POST")) == 0;
    is_put    = mg_strcmp(hm->method, mg_str("PUT")) == 0;
    is_delete = mg_strcmp(hm->method, mg_str("DELETE")) == 0;
    mutating  = is_post || is_put || is_delete;

    if (mutating) {
        if (!mutation_allowed(hm)) { reply_err(c, 403, "forbidden origin"); return; }
        if (hm->body.len > MAX_BODY) { reply_err(c, 413, "body too large"); return; }
    }

    /* static assets */
    if (is_get && mg_strcmp(hm->uri, mg_str("/")) == 0) {
        serve_asset(c, "text/html; charset=utf-8", web_index_html, web_index_html_len);
        return;
    }
    if (is_get && mg_strcmp(hm->uri, mg_str("/style.css")) == 0) {
        serve_asset(c, "text/css; charset=utf-8", web_style_css, web_style_css_len);
        return;
    }
    if (is_get && mg_strcmp(hm->uri, mg_str("/app.js")) == 0) {
        serve_asset(c, "application/javascript; charset=utf-8", web_app_js, web_app_js_len);
        return;
    }
    if (is_get && mg_strcmp(hm->uri, mg_str("/assets/folder_icon.svg")) == 0) {
        serve_asset(c, "image/svg+xml", folder_icon_svg, folder_icon_svg_len);
        return;
    }
    if (is_get && mg_strcmp(hm->uri, mg_str("/assets/arrow_icon.svg")) == 0) {
        serve_asset(c, "image/svg+xml", arrow_icon_svg, arrow_icon_svg_len);
        return;
    }
    if (is_get && mg_strcmp(hm->uri, mg_str("/assets/arrow_down_icon.svg")) == 0) {
        serve_asset(c, "image/svg+xml", arrow_down_icon_svg, arrow_down_icon_svg_len);
        return;
    }
    if (is_get && mg_strcmp(hm->uri, mg_str("/assets/add_icon.svg")) == 0) {
        serve_asset(c, "image/svg+xml", add_icon_svg, add_icon_svg_len);
        return;
    }

    /* API */
    if (is_get && mg_strcmp(hm->uri, mg_str("/config")) == 0) {
        config_public(c, srv->cfg);
        return;
    }
    if (mg_strcmp(hm->uri, mg_str("/shortcuts")) == 0) {
        if (is_get)  { shortcuts_list(c, srv->cfg); return; }
        if (is_post) { shortcuts_add(c, srv->cfg, hm); return; }
        reply_err(c, 405, "method not allowed");
        return;
    }
    if (is_post && mg_strcmp(hm->uri, mg_str("/shortcuts/swap")) == 0) {
        long a = mg_json_get_long(hm->body, "$.a", -1);
        long b = mg_json_get_long(hm->body, "$.b", -1);
        if (config_swap_shortcut(srv->cfg, (int)a, (int)b) != 0) { reply_err(c, 400, "bad swap"); return; }
        shortcuts_list(c, srv->cfg);
        return;
    }
    if (mg_match(hm->uri, mg_str("/shortcuts/*"), caps)) {
        char idx[16];
        snprintf(idx, sizeof(idx), "%.*s", (int)caps[0].len, caps[0].buf);
        if (is_put) {
            char name[128], color[16], url[1024];
            json_field(hm->body, "$.name", name, sizeof(name));
            json_field(hm->body, "$.color", color, sizeof(color));
            json_field(hm->body, "$.url", url, sizeof(url));
            if (name[0] == '\0') { reply_err(c, 400, "name required"); return; }
            if (config_update_shortcut(srv->cfg, atoi(idx), name, color, url) != 0) { reply_err(c, 404, "not found"); return; }
            shortcuts_list(c, srv->cfg);
            return;
        }
        if (is_delete) {
            if (config_delete_shortcut(srv->cfg, atoi(idx)) != 0) { reply_err(c, 404, "not found"); return; }
            mg_http_reply(c, 200, JSON_HDRS, "{\"ok\":true}\n");
            return;
        }
        reply_err(c, 405, "method not allowed");
        return;
    }
    if (is_get && mg_strcmp(hm->uri, mg_str("/search")) == 0) {
        char query[300];
        snprintf(query, sizeof(query), "%.*s", (int)hm->query.len, hm->query.buf);
        list_bookmarks(c, srv->st, query);
        return;
    }
    if (mg_strcmp(hm->uri, mg_str("/bookmarks")) == 0) {
        if (is_get)  { list_bookmarks(c, srv->st, NULL); return; }
        if (is_post) { create_bookmark(c, srv->st, hm); return; }
        reply_err(c, 405, "method not allowed");
        return;
    }
    if (mg_match(hm->uri, mg_str("/bookmarks/*"), caps)) {
        snprintf(id, sizeof(id), "%.*s", (int)caps[0].len, caps[0].buf);
        if (is_get) {
            struct bookmark b; struct sb s = {0};
            if (storage_get(srv->st, id, &b) != 0) { reply_err(c, 404, "not found"); return; }
            bookmark_json(&s, &b);
            mg_http_reply(c, 200, JSON_HDRS, "%s\n", s.buf);
            free(s.buf);
            return;
        }
        if (is_put)    { update_bookmark(c, srv->st, id, hm); return; }
        if (is_delete) {
            if (storage_delete(srv->st, id) != 0) { reply_err(c, 404, "not found"); return; }
            mg_http_reply(c, 200, JSON_HDRS, "{\"ok\":true}\n");
            return;
        }
        reply_err(c, 405, "method not allowed");
        return;
    }

    reply_err(c, 404, "not found");
}

/* --- lifecycle --- */

static void *run(void *arg) {
    struct server *s = (struct server *)arg;
    while (s->running) mg_mgr_poll(&s->mgr, 100);
    return NULL;
}

struct server *server_start(struct config *cfg, struct storage *storage) {
    struct server *s = calloc(1, sizeof(*s));
    char url[128];
    if (!s) return NULL;
    s->cfg = cfg;
    s->st = storage;
    s->running = 1;
    mg_log_set(MG_LL_ERROR);  /* silence Mongoose info/debug connection chatter */
    mg_mgr_init(&s->mgr);
    snprintf(url, sizeof(url), "http://%s:%d", cfg->bind, cfg->port);
    s->listener = mg_http_listen(&s->mgr, url, ev_handler, s);
    if (!s->listener) { mg_mgr_free(&s->mgr); free(s); return NULL; }
    if (pthread_create(&s->thread, NULL, run, s) != 0) {
        mg_mgr_free(&s->mgr); free(s); return NULL;
    }
    return s;
}

void server_stop(struct server *s) {
    if (!s) return;
    s->running = 0;
    pthread_join(s->thread, NULL);
    mg_mgr_free(&s->mgr);
    free(s);
}
