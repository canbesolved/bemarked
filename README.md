<h1 align="center">bemarked <sub>(<code>bmkd</code>)</sub></h1>

<p align="center"><em>Your personal bookmarks manager</em></p>

**bemarked** (short name **`bmkd`**) is a lightweight, offline, local bookmarks manager/daemon:
a single tiny C binary using the Mongoose embedded server, an embedded single-file web
UI, and human-readable plain-text storage. No cloud, no accounts, no external requests.
It follows the **Unix philosophy** — do one thing well, keep data as plain text, and
compose with standard tools (`grep`, `awk`, `cut`).

## Principles
- **Backend authoritative, thin client** — the daemon owns validation, ids, search, and storage.
- **Pure text & offline** — one greppable text file, no external requests.
- **Portable** — desktop (Windows/macOS/Linux) and headless (OpenWRT/Termux). ESP32 is a future profile.


## Features
Cross-platform: Linux, MacOS, Windows, OpenWRT (ARM32/ARM64/MIPS/RISC-V)
Cross-browser: work in any browser
Shortcut reorder by mouse drag-and-drop
Responsive web, mobile supported
Drag and drop support (Shortcuts, Folders, Bookmarks)
Touch/pen support
Light / Dark theme support with auto-switching


10 times smaller size than bookmarks.html

consumes only 2 megabytes of ram
consumes 5 megabyte with 1000 bookmarks

## Roadmap
- Add authentication
- Add TLS support
- Add hotkeys / keyboard navigation + vim style navigation
- Create documentation
- Test on different platforms
- System tray icon (Windows / macOS / Linux) as an optional, build-time module — left-click opens the browser, right-click menu (Open in browser, Exit)
- Add 'autostart' option for all platforms
- Benchmark with 10.000 bookmarks or 30.000 bookmarks, check the ram and cpu load
- Browser addons (Firefox, Chrome)
- Add UI wrapper for android like regular app to run locally



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
src/        main, config, model, storage (text), server (Mongoose), platform (run loop)
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
See `conf.txt`. Binds `127.0.0.1:7773` by default; LAN exposure is opt-in. By default
`bookmarks.txt` lives next to the `bmkd` binary (override with `bookmarks_file`).

## Importing browser bookmarks
`bmkd` reads exported browser bookmarks — the **Netscape Bookmark File** format that Chrome,
Firefox, Edge, and Safari all produce. Titles, folder hierarchy (as `/`-separated paths), and
links are preserved, HTML entities decoded, and ids generated; non-http entries (`javascript:`
bookmarklets, Firefox `place:` queries) are skipped. The parser is pure C — no libraries.

First export from your browser (e.g. Firefox → *Manage Bookmarks → Import and Backup →
Export Bookmarks to HTML…*), then import one of two ways:

**From the web UI** — click the **⋮ menu** (top-right) → **Import bookmarks**, and pick the
exported `.html` file. Entries are added to your current bookmarks.

**From the CLI** — convert an export to a `bookmarks.txt`:

```
./bmkd convert                     # ./bookmarks.html -> ./bookmarks.txt
./bmkd convert my.html             # -> ./bookmarks.txt
./bmkd convert my.html out.txt     # explicit output
```

Then point `bookmarks_file` at the result, or place `bookmarks.txt` next to the `bmkd` binary.

## License
**bemarked is licensed under GPLv2-only** (see `LICENSE`); source files carry
`SPDX-License-Identifier: GPL-2.0-only`. This matches Mongoose, which is dual-licensed
**GPLv2 or commercial** — because the binary links Mongoose, a permissive redistribution
(MIT/Apache) would require either a commercial Mongoose license or swapping Mongoose for a
permissively-licensed server. See `deps/mongoose/README.md`.
