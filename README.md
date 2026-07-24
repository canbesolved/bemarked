# bmkd

A lightweight, offline, local bookmark daemon. Single C binary using the Mongoose
embedded server, an embedded single-file web UI, and human-readable **TSV** storage.

## Principles
- **Backend authoritative, thin client** — the daemon owns validation, ids, search, and storage.
- **Pure text & offline** — one greppable TSV file, no external requests.
- **Portable** — desktop (Windows/macOS/Linux) and headless (OpenWRT/Termux). ESP32 is a future profile.

## Storage schema
One bookmark per line, TAB-separated, versioned header:

```
# bmkd v1	id	name	folder	url
7fa93d2c	Linux	dev/sources	https://linux.org
a1b2c3d4	Open Source Initiative	dev/sources/osi	https://opensource.org
```

`folder` is a `/`-separated path encoding subfolders (case-sensitive). Writes are
crash-safe (temp → fsync → atomic rename, one `.bak`).

## Layout
```
src/        main, config, model, storage (TSV), server (Mongoose), platform (tray)
web/        SPA source split into index.html + style.css + app.js (embedded at build time)
deps/       Mongoose (deps/mongoose/mongoose.c/.h)
cmake/      embed_asset.cmake — web/* -> generated C headers
conf.txt    sample configuration
```

## License
**bmkd is licensed under GPLv2-only** (see `LICENSE`); source files carry
`SPDX-License-Identifier: GPL-2.0-only`. This matches Mongoose, which is dual-licensed
**GPLv2 or commercial** — because the binary links Mongoose, a permissive redistribution
(MIT/Apache) would require either a commercial Mongoose license or swapping Mongoose for a
permissively-licensed server. See `deps/mongoose/README.md`.

## Build
```
cmake -S . -B build
cmake --build build
./build/bmkd
```
Mongoose is already vendored in `deps/mongoose/`.

## Config
See `conf.txt`. Binds `127.0.0.1:8989` by default; LAN exposure is opt-in.
