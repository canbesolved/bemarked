<p align="center">
  <img src="assets/app_icon.png" alt="bemarked logo" width="96" height="96">
</p>

<h1 align="center">bemarked (<code>bmkd</code>)</h1>

<p align="center"><em>Your private bookmarks manager that works simply and easily for you.</em></p>

## About project

**bemarked** (short name `bmkd / bookmarks daemon`) is a lightweight, self-hosted, private bookmarks manager:
a single tiny C binary with human-readable plain-text (TSV) storage. 

No cloud, no accounts, no external requests.

It follows the **Unix philosophy** — do one thing well, keep data as plain text, and
compose with standard tools (`grep`, `awk`, `cut`) + HTTP API with JSON output.

## Short story
I’ve wanted to bring this project to life for a long time. In real-world use, when you frequently switch between different browsers and devices, bookmarks eventually get scattered around. On top of that, you need synchronization, quick access, straightforward backups, self-hosting/privacy options, and so on.

I looked into existing bookmark managers, ranging from feature-rich tools to minimalist ones. The comprehensive solutions solved the core problem, but were heavily bloated with extra features: setup required a dedicated server or Docker, and everything was stored in a complex database. The simple ones, on the other hand, often had rather niche implementations.

What I needed was a manager with a familiar interface, a clear and straightforward architecture, and the ability to access a bookmark even with just a plain text editor at hand. Having used Linux and other open-source projects like OpenWrt for years, I knew things could be kept simple. After gathering these ideas and structuring the architecture, this project was born.

## Demo

<p align="center">
  <a href="https://youtu.be/CMpw4XsFpZM">
    <img src="https://img.youtube.com/vi/CMpw4XsFpZM/maxresdefault.jpg" alt="Watch the demo on YouTube" width="640">
  </a>
</p>

<p align="center"><em>▶ Watch the demo on YouTube</em></p>

## Features
- **Small size** — a single binary of just ~300 KB.
- **Tab-separated plain text (TSV)** — bookmarks are stored as plain text in a human-readable format: easy to read, parse, sync (Syncthing, Rclone, Google Drive, Dropbox, etc.), compress, and back up.
- **High-performance & lightweight** — near-zero CPU load; consumes only 5–10 MB of RAM (with 1000+ bookmarks).
- **Cross-platform** — Linux / OpenWRT / SBC / Termux (x86_64, x86, arm, arm64, mips, mipsel, riscv64), Windows (x86_64), macOS (universal). Linux builds are static musl (no libc dependency), so the matching-arch binary drops straight onto an OpenWRT router.
- **User-friendly interface** — a web UI with built-in fuzzy search, a homepage of shortcuts, and everything needed to manage folders and links.
- **Responsive & mobile-friendly** — with light / dark themes (auto-switching).
- **Drag-and-drop** — organize shortcuts, folders, and bookmarks, with touch/pen support.
- **Plain-text & offline** — one greppable `bookmarks.txt`, no external requests, 10× more compact than a browser's exported `bookmarks.html`, with crash-safe atomic writes.
- **Portable** — one binary, one plain-text config file, one bookmarks file. Simple.

## Getting started

### 1. Download & run

Grab the binary for your platform from the [Releases](../../releases) page, unpack it, and run:

```
tar -xzf bmkd-linux-x86_64.tar.gz      # or:  unzip bmkd-windows-x86_64.zip
./bmkd                                  # serves http://127.0.0.1:7773
```

Then open **http://127.0.0.1:7773** in your browser. Each archive bundles the binary, a
sample `conf.txt`, `README.md`, and `LICENSE`. By default bemarked listens on localhost
only — set `bind = "0.0.0.0"` in `conf.txt` to reach it from other devices on your LAN.

Prefer to build from source? Mongoose is already vendored in `deps/mongoose/`:

```
cmake -S . -B build
cmake --build build
./build/bmkd
```

### 2. Configuration — `conf.txt`

Simple `key = value` lines; `#` starts a comment. bemarked reads `conf.txt` from next to the
binary, or pass a path explicitly: `./bmkd /path/to/conf.txt`.

| Key | Default | Meaning |
| --- | --- | --- |
| `bind` | `"127.0.0.1"` | interface to listen on (`"0.0.0.0"` exposes it on the LAN) |
| `port` | `7773` | TCP port |
| `bookmarks_file` | *(next to binary)* | path to `bookmarks.txt` |
| `link_open_mode` | `"new-tab"` | `new-tab` or `same-tab` |
| `shortcuts_per_row` | `7` | homepage shortcut grid width |
| `shortcut[N]` | — | a homepage shortcut: `"name \| #hex_color \| url"` |

Example:

```ini
port = 7773
bind = "127.0.0.1"
link_open_mode = "new-tab"
shortcuts_per_row = 7
shortcut[0] = "DuckDuckGo | #DE5833 | https://duckduckgo.com"
shortcut[1] = "GitHub | #000000 | https://github.com"
```

### 3. Bookmarks file — `bookmarks.txt`

One bookmark per line, TAB-separated, with a `#` comment header:

```
#	id	name	folder	url
7fa93	Linux	dev/sources	https://linux.org
a1b2c	Open Source Initiative	dev/sources/osi	https://opensource.org
```

- **id** — short, auto-generated identifier.
- **name** — the bookmark title.
- **folder** — a `/`-separated path encoding subfolders (case-sensitive); empty defaults to `unsorted`.
- **url** — the link.

It's just text: edit it in any editor, `grep`/`awk` it, sync or back it up. Writes are
crash-safe (temp → fsync → atomic rename, keeping one `.bak`).

### Import browser bookmarks

bemarked reads exported browser bookmarks — the **Netscape Bookmark File** format that Chrome,
Firefox, Edge, and Safari all produce. Two ways to import:

- **Web UI** — **⋮ menu** (top-right) → **Import bookmarks**, then pick the exported `.html` file.
- **CLI** — `./bmkd convert bookmarks.html` writes a `bookmarks.txt`.

## Roadmap

- Browser addons (Firefox, Chrome)
- Add authentication
- Add TLS support
- Add hotkeys / keyboard navigation + vim style navigation
- Create documentation
- Test on different platforms
- System tray icon (Windows / macOS / Linux) as an optional, build-time module — left-click opens the browser, right-click menu (Open in browser, Exit)
- Add 'autostart' option for all platforms (scripts, systemd service file)
- Benchmark with 10k-30k bookmarks, check the RAM and CPU load
- Create native packages for Debian, Fedora, Arch
- Add to Flathub and Snap
- Benchmark memory/disk footprint and publish measured numbers

## Support

bemarked is free and open source. If you find it useful, please consider supporting its
development — it helps a lot and is hugely appreciated:

- ❤️ **[GitHub Sponsors](https://github.com/sponsors/canbesolved)**
- 🧡 **[Patreon](https://www.patreon.com/c/CanBeSolved)**

Thank you!

## License

**bemarked is licensed under GPLv2-only** (see `LICENSE`); source files carry
`SPDX-License-Identifier: GPL-2.0-only`. This matches Mongoose, which is dual-licensed
**GPLv2 or commercial** — because the binary links Mongoose, a permissive redistribution
(MIT/Apache) would require either a commercial Mongoose license or swapping Mongoose for a
permissively-licensed server. See `deps/mongoose/README.md`.
