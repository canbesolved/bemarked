// SPDX-License-Identifier: GPL-2.0-only
// bmkd SPA logic. Served same-origin at /app.js (embedded in the binary).
// Thin client: renders + navigates; the backend is authoritative for data/search.

const $ = (id) => document.getElementById(id);
const state = { bookmarks: [], folder: "", query: "" };

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

function renderTree() {
  const filter = $("folderFilter").value.toLowerCase();
  const tree = $("folderTree");
  tree.textContent = "";
  $("allBookmarks").classList.toggle("active", state.folder === "");
  for (const path of folderSet()) {
    if (filter && !path.toLowerCase().includes(filter)) continue;
    const depth = path.split("/").length - 1;
    const active = state.folder === path;

    const row = document.createElement("div");
    row.className = "folder-row" + (active ? " active" : "");

    const btn = document.createElement("button");
    btn.className = "tree-node";
    btn.style.paddingLeft = 0.5 + depth * 0.9 + "rem";
    btn.textContent = path.split("/").pop();
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
    tree.appendChild(row);
  }
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

// --- shortcuts (small square cards) ---
async function renderShortcuts() {
  const cfg = await api("GET", "/config/public").catch(() => null);
  const el = $("shortcuts");
  el.textContent = "";
  if (!cfg || !cfg.shortcuts.length) return;
  const cols = cfg.shortcuts_on_row || 4;
  el.style.setProperty("--cols", cols);   /* CSS decides layout; media query can wrap */
  for (const s of cfg.shortcuts.slice(0, cols * (cfg.rows || 3))) {
    const a = document.createElement("a");
    a.className = "shortcut";
    a.textContent = s.name;
    a.href = /^https?:\/\//.test(s.url) ? s.url : "#";
    a.target = "_blank";                 // open in a new tab
    a.rel = "noopener noreferrer";
    a.style.background = /^#[0-9a-fA-F]{3,8}$/.test(s.color) ? s.color : "#666";
    el.appendChild(a);
  }
}

// --- bookmark table (folder / search view) ---
function visible() {
  const q = state.query.toLowerCase();
  return state.bookmarks.filter((b) => {
    if (state.folder && !(b.folder === state.folder || b.folder.startsWith(state.folder + "/"))) return false;
    if (q && !(`${b.name}\n${b.folder}\n${b.url}`.toLowerCase().includes(q))) return false;
    return true;
  });
}

function renderRows() {
  const rows = $("rows");
  rows.textContent = "";
  const list = visible();
  $("empty").hidden = list.length > 0;
  for (const b of list) {
    const tr = document.createElement("tr");
    const name = document.createElement("td");
    name.textContent = b.name;                    // textContent => no XSS
    const folder = document.createElement("td");
    folder.className = "folder"; folder.textContent = b.folder || "—";
    const url = document.createElement("td");
    const a = document.createElement("a");
    a.href = /^https?:\/\//.test(b.url) ? b.url : "#";
    a.textContent = b.url; a.target = "_blank"; a.rel = "noopener noreferrer";
    url.appendChild(a);
    const act = document.createElement("td");
    act.className = "row-actions";
    const edit = document.createElement("button"); edit.textContent = "Edit";
    edit.onclick = () => openForm(b);
    const del = document.createElement("button"); del.textContent = "Delete";
    del.onclick = () => removeBookmark(b.id);
    act.append(edit, del);
    tr.append(name, folder, url, act);
    rows.appendChild(tr);
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
  $("table").hidden = !browsing;
  $("empty").hidden = true;
  if (browsing) renderRows();
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

// --- wire up ---
$("allBookmarks").onclick = () => { state.folder = ""; state.query = ""; $("search").value = ""; closeNav(); render(); };
$("folderFilter").oninput = renderTree;
$("search").oninput = (e) => { state.query = e.target.value; render(); };
$("newBtn").onclick = () => openForm(null);
$("cancelBtn").onclick = closeForm;
$("form").onsubmit = submitForm;
$("overlay").onclick = (e) => { if (e.target === $("overlay")) closeForm(); };  // click backdrop to close
document.addEventListener("keydown", (e) => { if (e.key === "Escape") closeForm(); });

renderShortcuts();
load();
