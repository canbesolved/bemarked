// SPDX-License-Identifier: GPL-2.0-only
// bmkd SPA logic. Served same-origin at /app.js (embedded in the binary).
// Thin client: renders + navigates; the backend is authoritative for data/search.

const $ = (id) => document.getElementById(id);
const state = { bookmarks: [], folder: "", query: "", expanded: new Set(), openItems: new Set() };
let openInNewTab = true;   // set from config's link_open_mode

function openLink(url) {
  if (openInNewTab) window.open(url, "_blank", "noopener");
  else window.location.href = url;   // same tab
}

// --- data ---
async function api(method, path, body) {
  const opt = { method, headers: {} };
  if (body !== undefined) { opt.headers["Content-Type"] = "application/json"; opt.body = JSON.stringify(body); }
  const res = await fetch(path, opt);
  if (!res.ok) throw new Error((await res.json().catch(() => ({}))).error || res.status);
  return res.status === 200 || res.status === 201 ? res.json() : null;
}

async function load() {
  state.bookmarks = await api("GET", "/bookmarks");
  render();
}

// --- folder tree ---
function folderSet() {
  const set = new Set();
  for (const b of state.bookmarks) {
    if (!b.folder) continue;
    const parts = b.folder.split("/");
    for (let i = 0; i < parts.length; i++) set.add(parts.slice(0, i + 1).join("/"));
  }
  return [...set].sort();
}

// Immediate child folders of `parent` ("" = top level).
function childFolders(parent) {
  return folderSet().filter((p) => p.split("/").slice(0, -1).join("/") === parent);
}

// Build a nested {name: {path, children}} tree from all folder paths.
function buildFolderTree() {
  const root = {};
  for (const p of folderSet()) {
    let map = root, acc = "";
    for (const part of p.split("/")) {
      acc = acc ? acc + "/" + part : part;
      if (!map[part]) map[part] = { path: acc, children: {} };
      map = map[part].children;
    }
  }
  return root;
}

function renderTreeInto(map, container, depth) {
  for (const name of Object.keys(map).sort()) {
    const node = map[name];
    const path = node.path;
    const hasKids = Object.keys(node.children).length > 0;
    const active = state.folder === path;
    const expanded = state.expanded.has(path);

    const row = document.createElement("div");
    row.className = "folder-row" + (active ? " active" : "");
    row.style.paddingLeft = 0.25 + depth * 0.85 + "rem";

    const caret = document.createElement("span");
    caret.className = "folder-caret" + (hasKids ? "" : " leaf");
    if (hasKids) {
      const cic = document.createElement("span");
      cic.className = "ic " + (expanded ? "ic-arrow-down" : "ic-arrow");
      caret.appendChild(cic);
      caret.onclick = (e) => {
        e.stopPropagation();
        if (expanded) state.expanded.delete(path); else state.expanded.add(path);
        renderTree();
      };
    }
    row.appendChild(caret);

    const btn = document.createElement("button");
    btn.className = "tree-node";
    const fic = document.createElement("span");
    fic.className = "ic ic-folder";
    btn.appendChild(fic);
    btn.appendChild(document.createTextNode(" " + name));
    btn.title = path;
    btn.onclick = () => { state.folder = path; state.query = ""; $("search").value = ""; closeNav(); render(); };
    row.appendChild(btn);

    if (active) {  // '+' to add a bookmark directly into the selected folder
      const add = document.createElement("button");
      add.className = "folder-add";
      add.textContent = "+";
      add.title = "New bookmark in " + path;
      add.onclick = (e) => { e.stopPropagation(); openForm(null, path); };
      row.appendChild(add);
    }
    container.appendChild(row);

    if (hasKids && expanded) renderTreeInto(node.children, container, depth + 1);
  }
}

function renderTree() {
  const tree = $("folderTree");
  tree.textContent = "";
  $("homepage").classList.toggle("active", state.folder === "");
  renderTreeInto(buildFolderTree(), tree, 0);
}

// Populate the folder autocomplete datalist used by the new/edit form.
function renderFolderOptions() {
  const dl = $("folderOptions");
  dl.textContent = "";
  for (const path of folderSet()) {
    const opt = document.createElement("option");
    opt.value = path;
    dl.appendChild(opt);
  }
}

// Pick a readable text color for a hex background (YIQ contrast).
function textOn(bg) {
  let h = bg.replace("#", "");
  if (h.length === 3) h = h.split("").map((c) => c + c).join("");
  if (h.length < 6) return "#fff";
  const r = parseInt(h.slice(0, 2), 16), g = parseInt(h.slice(2, 4), 16), b = parseInt(h.slice(4, 6), 16);
  return (r * 299 + g * 587 + b * 114) / 1000 >= 128 ? "#111" : "#fff";
}

// --- shortcuts (small square cards) ---
async function renderShortcuts() {
  const cfg = await api("GET", "/config/public").catch(() => null);
  if (cfg) openInNewTab = cfg.link_open_mode !== "same-tab";
  const el = $("shortcuts");
  el.textContent = "";
  el.style.setProperty("--cols", (cfg && cfg.shortcuts_per_row) || 7);
  ((cfg && cfg.shortcuts) || []).forEach((s, i) => {
    const tile = document.createElement("div");
    tile.className = "shortcut-tile";

    const a = document.createElement("a");
    a.className = "shortcut";
    const label = document.createElement("span");
    label.className = "shortcut-label";
    label.textContent = s.name;
    a.appendChild(label);
    a.href = /^https?:\/\//.test(s.url) ? s.url : "#";
    if (openInNewTab) { a.target = "_blank"; a.rel = "noopener noreferrer"; }  // else same tab
    const bg = /^#[0-9a-fA-F]{3,8}$/.test(s.color) ? s.color : "#666";
    a.style.background = bg;
    const light = textOn(bg) === "#fff";
    a.style.color = light ? "#fff" : "#111";
    a.style.textShadow = light ? "0 1px 2px #0006" : "none";   // shadow only helps white text
    tile.appendChild(a);

    // kebab (⋮) options menu — appears on hover
    const kebab = document.createElement("button");
    kebab.className = "kebab";
    kebab.title = "Options";
    const dots = document.createElement("span");
    dots.className = "dots";     // three CSS-drawn dots (centered by geometry)
    kebab.appendChild(dots);
    const menu = document.createElement("div");
    menu.className = "kebab-menu";
    menu.hidden = true;
    menu.onclick = (e) => e.stopPropagation();
    const editB = document.createElement("button");
    editB.textContent = "Edit";
    editB.onclick = () => { menu.hidden = true; openShortcutForm(s, i); };
    const delB = document.createElement("button");
    delB.className = "kebab-del";
    delB.textContent = "Delete";
    delB.onclick = () => { menu.hidden = true; deleteShortcut(i); };
    menu.append(editB, delB);
    kebab.onclick = (e) => {
      e.preventDefault(); e.stopPropagation();
      const wasHidden = menu.hidden;
      closeShortcutMenus();
      menu.hidden = !wasHidden;
    };
    tile.append(kebab, menu);
    el.appendChild(tile);
  });

  // always-last "add shortcut" card
  const add = document.createElement("button");
  add.className = "shortcut shortcut-add";
  add.title = "New shortcut";
  const ic = document.createElement("span");
  ic.className = "ic ic-add";
  add.appendChild(ic);
  add.onclick = () => openShortcutForm();
  el.appendChild(add);
}

function closeShortcutMenus() {
  document.querySelectorAll(".kebab-menu").forEach((m) => (m.hidden = true));
}

async function deleteShortcut(index) {
  if (!confirm("Delete this shortcut?")) return;
  try { await api("DELETE", "/shortcuts/" + encodeURIComponent(index)); await renderShortcuts(); }
  catch (err) { alert("Delete failed: " + err.message); }
}

// --- new/edit-shortcut popup ---
function openShortcutForm(sc, index) {
  const editing = sc != null;
  $("shortcutForm").reset();
  $("shortcutForm").dataset.index = editing ? index : "";
  $("shortcutTitle").textContent = editing ? "Edit shortcut" : "New shortcut";
  $("sc-name").value = editing ? sc.name : "";
  $("sc-url").value = editing ? sc.url : "";
  const color = editing && /^#[0-9a-fA-F]{6}$/.test(sc.color) ? sc.color : "";
  $("sc-color").value = color;
  $("sc-color-picker").value = color || "#4285f4";
  closeNav();
  closeShortcutMenus();
  $("shortcutOverlay").hidden = false;
  $("sc-name").focus();
}
function closeShortcutForm() { $("shortcutOverlay").hidden = true; $("shortcutForm").reset(); }

async function submitShortcut(e) {
  e.preventDefault();
  const idx = $("shortcutForm").dataset.index;
  const body = {
    name: $("sc-name").value,
    color: $("sc-color").value || $("sc-color-picker").value,
    url: $("sc-url").value,
  };
  try {
    if (idx !== "" && idx != null) await api("PUT", "/shortcuts/" + encodeURIComponent(idx), body);
    else await api("POST", "/shortcuts", body);
    closeShortcutForm();
    await renderShortcuts();
  } catch (err) { alert("Save failed: " + err.message); }
}

// --- bookmark table (folder / search view) ---
function visible() {
  const q = state.query.toLowerCase();
  return state.bookmarks.filter((b) => {
    if (state.folder && b.folder !== state.folder) return false;   // this folder only, not subfolders
    if (q && !(`${b.name}\n${b.folder}\n${b.url}`.toLowerCase().includes(q))) return false;
    return true;
  });
}

function renderRows() {
  const box = $("list");
  box.textContent = "";
  const searchMode = !state.folder && !!state.query;   // results span folders -> show folder
  const folderView = !!state.folder;
  const subfolders = folderView ? childFolders(state.folder) : [];
  const list = visible();
  $("list").hidden = subfolders.length + list.length === 0;
  $("empty").hidden = subfolders.length + list.length > 0;

  for (const path of subfolders) {   // subfolders first, then bookmarks
    const item = document.createElement("div");
    item.className = "bm-item folder-item";
    item.onclick = () => { state.folder = path; render(); };   // navigate into subfolder
    const fic = document.createElement("span");
    fic.className = "ic ic-folder";
    const nm = document.createElement("span");
    nm.className = "bm-name";
    nm.textContent = path.split("/").pop();
    item.append(fic, nm);
    box.appendChild(item);
  }

  for (const b of list) {
    const valid = /^https?:\/\//.test(b.url);
    const item = document.createElement("div");
    item.className = "bm-item";
    if (valid) item.onclick = () => openLink(b.url);   // whole item clickable (mode from config)

    if (folderView) {   // expand caret -> reveals the url as subtext
      const open = state.openItems.has(b.id);
      const caret = document.createElement("span");
      caret.className = "bm-caret";
      const cic = document.createElement("span");
      cic.className = "ic " + (open ? "ic-arrow-down" : "ic-arrow");
      caret.appendChild(cic);
      caret.onclick = (e) => {
        e.stopPropagation();
        if (open) state.openItems.delete(b.id); else state.openItems.add(b.id);
        renderRows();
      };
      item.appendChild(caret);
    }

    const main = document.createElement("div");
    main.className = "bm-main";
    const name = document.createElement("div");
    name.className = "bm-name";
    name.textContent = b.name;                    // textContent => no XSS
    main.appendChild(name);
    if (searchMode) {
      if (b.folder) {
        const fsub = document.createElement("div");
        fsub.className = "bm-sub";
        fsub.textContent = "folder: " + b.folder;
        main.appendChild(fsub);
      }
      if (b.url) {
        const usub = document.createElement("div");
        usub.className = "bm-sub";
        usub.textContent = "url: " + b.url;
        main.appendChild(usub);
      }
    } else if (folderView && state.openItems.has(b.id) && b.url) {
      const usub = document.createElement("div");
      usub.className = "bm-sub";
      usub.textContent = b.url;
      main.appendChild(usub);
    }

    const act = document.createElement("div");
    act.className = "row-actions";
    const copy = document.createElement("button"); copy.textContent = "Copy link";
    copy.onclick = (e) => { e.stopPropagation(); copyLink(b.url, copy); };
    const edit = document.createElement("button"); edit.textContent = "Edit";
    edit.onclick = (e) => { e.stopPropagation(); openForm(b); };
    const del = document.createElement("button"); del.textContent = "Delete";
    del.onclick = (e) => { e.stopPropagation(); removeBookmark(b.id); };
    act.append(copy, edit, del);

    item.append(main, act);
    box.appendChild(item);
  }
}

// --- view switching ---
//   homepage        : search + shortcuts
//   search results  : search + table (shortcuts hidden)
//   folder view     : table only (search + shortcuts hidden)
function render() {
  renderTree();
  renderFolderOptions();
  const folderView = !!state.folder;
  const browsing = folderView || !!state.query;
  $("main").classList.toggle("browsing", browsing);
  $("searchWrap").hidden = folderView;   // hide search when viewing a folder
  $("shortcuts").hidden = browsing;
  if (browsing) {
    renderRows();   // toggles list/empty visibility based on results
  } else {
    $("list").hidden = true;
    $("empty").hidden = true;
  }
}

// --- add/edit form (overlay) ---
function openForm(b, presetFolder) {
  $("f-title").textContent = b ? "Edit bookmark" : "New bookmark";
  $("f-id").value = b ? b.id : "";
  $("f-name").value = b ? b.name : "";
  $("f-folder").value = b ? b.folder : (presetFolder || state.folder || "");
  $("f-url").value = b ? b.url : "";
  closeNav();                 // don't leave the mobile drawer over the overlay
  $("overlay").hidden = false;
  $("f-name").focus();
}
function closeForm() { $("overlay").hidden = true; $("form").reset(); }

async function submitForm(e) {
  e.preventDefault();
  const id = $("f-id").value;
  const body = { name: $("f-name").value, folder: $("f-folder").value, url: $("f-url").value };
  try {
    if (id) await api("PUT", "/bookmarks/" + encodeURIComponent(id), body);
    else await api("POST", "/bookmarks", body);
    closeForm();
    await load();
  } catch (err) { alert("Save failed: " + err.message); }
}

async function copyLink(url, btn) {
  const flash = () => {
    const prev = btn.textContent;
    btn.textContent = "Copied!";
    setTimeout(() => { btn.textContent = prev; }, 1200);
  };
  try {
    if (navigator.clipboard && window.isSecureContext) {
      await navigator.clipboard.writeText(url);
    } else {                               // fallback for non-secure (LAN http) contexts
      const ta = document.createElement("textarea");
      ta.value = url; ta.style.position = "fixed"; ta.style.opacity = "0";
      document.body.appendChild(ta); ta.select();
      document.execCommand("copy"); ta.remove();
    }
    flash();
  } catch { alert("Copy failed"); }
}

async function removeBookmark(id) {
  if (!confirm("Delete this bookmark?")) return;
  try { await api("DELETE", "/bookmarks/" + encodeURIComponent(id)); await load(); }
  catch (err) { alert("Delete failed: " + err.message); }
}

// --- theme switcher (Material light/dark; follows OS until overridden) ---
function effectiveTheme() {
  const t = document.documentElement.dataset.theme;
  if (t) return t;
  return matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}
function updateThemeIcon() {
  $("themeToggle").textContent = effectiveTheme() === "dark" ? "☀️" : "🌙";
}
function applyTheme(theme) {
  document.documentElement.dataset.theme = theme;
  localStorage.setItem("bmkd-theme", theme);
  updateThemeIcon();
}
function initTheme() {
  const saved = localStorage.getItem("bmkd-theme");
  if (saved) document.documentElement.dataset.theme = saved;
  updateThemeIcon();
}
$("themeToggle").onclick = () => applyTheme(effectiveTheme() === "dark" ? "light" : "dark");
initTheme();

// --- mobile navigation drawer ---
function closeNav() { document.body.classList.remove("nav-open"); }
$("navToggle").onclick = () => document.body.classList.toggle("nav-open");
$("navBackdrop").onclick = closeNav;

// --- resizable sidebar (desktop): mouse / touch / pen via Pointer Events ---
const SIDE_MIN = 180, SIDE_MAX = 480, SIDE_DEFAULT = 260;
function setSideWidth(px) {
  px = Math.max(SIDE_MIN, Math.min(SIDE_MAX, Math.round(px)));
  document.documentElement.style.setProperty("--side-w", px + "px");
  return px;
}
(function initSidebar() {
  const resizer = $("resizer");
  const saved = parseInt(localStorage.getItem("bmkd-side-w"), 10);
  if (saved) setSideWidth(saved);

  resizer.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    resizer.setPointerCapture(e.pointerId);   // keep receiving moves outside the handle
    document.body.classList.add("resizing");
    const onMove = (ev) => setSideWidth(ev.clientX);   // sidebar starts at x=0
    const onUp = (ev) => {
      localStorage.setItem("bmkd-side-w", setSideWidth(ev.clientX));
      document.body.classList.remove("resizing");
      resizer.removeEventListener("pointermove", onMove);
      resizer.removeEventListener("pointerup", onUp);
      resizer.removeEventListener("pointercancel", onUp);
    };
    resizer.addEventListener("pointermove", onMove);
    resizer.addEventListener("pointerup", onUp);
    resizer.addEventListener("pointercancel", onUp);
  });

  // double-click / double-tap the divider to reset to the default width
  resizer.addEventListener("dblclick", () => {
    localStorage.setItem("bmkd-side-w", setSideWidth(SIDE_DEFAULT));
  });
})();

// --- wire up ---
$("homepage").onclick = () => { state.folder = ""; state.query = ""; $("search").value = ""; closeNav(); render(); };
$("search").oninput = (e) => { state.query = e.target.value; render(); };
$("newBtn").onclick = () => openForm(null);
$("cancelBtn").onclick = closeForm;
$("form").onsubmit = submitForm;
$("overlay").onclick = (e) => { if (e.target === $("overlay")) closeForm(); };  // click backdrop to close

// new-shortcut popup wiring + color/hex sync
$("shortcutForm").onsubmit = submitShortcut;
$("sc-cancel").onclick = closeShortcutForm;
$("shortcutOverlay").onclick = (e) => { if (e.target === $("shortcutOverlay")) closeShortcutForm(); };
$("sc-color").oninput = (e) => {
  if (/^#[0-9a-fA-F]{6}$/.test(e.target.value)) $("sc-color-picker").value = e.target.value;
};
$("sc-color-picker").oninput = (e) => { $("sc-color").value = e.target.value; };

document.addEventListener("keydown", (e) => {
  if (e.key === "Escape") { closeForm(); closeShortcutForm(); closeShortcutMenus(); }
});
document.addEventListener("click", closeShortcutMenus);   // click anywhere closes kebab menus

renderShortcuts();
load();
