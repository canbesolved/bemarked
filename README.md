<h1 align="center">bemarked <sub>(<code>bmkd</code>)</sub></h1>

<p align="center"><em>Your personal bookmarks manager/daemon.</em></p>

**bemarked** (short name **`bmkd`**) is a lightweight, offline, local bookmark daemon:
a single tiny C binary using the Mongoose embedded server, an embedded single-file web
UI, and human-readable plain-text storage. No cloud, no accounts, no external requests.
It follows the **Unix philosophy** — do one thing well, keep data as plain text, and
compose with standard tools (`grep`, `awk`, `cut`).

> The project's official name is **bemarked**; **`bmkd`** is the short form used for the
> binary, source, config keys, and API — they refer to the same thing.

## Principles
- **Backend authoritative, thin client** — the daemon owns validation, ids, search, and storage.
- **Pure text & offline** — one greppable text file, no external requests.
- **Portable** — desktop (Windows/macOS/Linux) and headless (OpenWRT/Termux). ESP32 is a future profile.

## Storage schema
One bookmark per line in `bookmarks.txt`, TAB-separated, with a `#` comment header:

```
#	id	name	folder	url
7fa93	Linux	dev/sources	https://linux.org
a1b2c	Open Source Initiative	dev/sources/osi	https://opensource.org
```

`folder` is a `/`-separated path encoding subfolders (case-sensitive); an empty folder
defaults to `unsorted`. Writes are crash-safe (temp → fsync → atomic rename, one `.bak`).

## Layout
```
src/        main, config, model, storage (text), server (Mongoose), platform (tray)
web/        SPA source split into index.html + style.css + app.js (embedded at build time)
deps/       Mongoose (deps/mongoose/mongoose.c/.h)
cmake/      embed_asset.cmake — web/* -> generated C headers
conf.txt    sample configuration
```

## Build
```
cmake -S . -B build
cmake --build build
./build/bmkd
```
Mongoose is already vendored in `deps/mongoose/`.

## Config
See `conf.txt`. Binds `127.0.0.1:8989` by default; LAN exposure is opt-in. By default
`bookmarks.txt` lives next to the `bmkd` binary (override with `bookmarks_file`).

## License
**bemarked is licensed under GPLv2-only** (see `LICENSE`); source files carry
`SPDX-License-Identifier: GPL-2.0-only`. This matches Mongoose, which is dual-licensed
**GPLv2 or commercial** — because the binary links Mongoose, a permissive redistribution
(MIT/Apache) would require either a commercial Mongoose license or swapping Mongoose for a
permissively-licensed server. See `deps/mongoose/README.md`.
