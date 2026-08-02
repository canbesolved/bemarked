// SPDX-License-Identifier: GPL-2.0-only
// BMKD web UI logic (SPA).
// Works as a thin client: rendering and navigation are handled on the frontend, while the backend is used for data and search.

// UI states
const $ = (id) => document.getElementById(id);
const state = { 
  bookmarks: [],         // all bookmarks loaded from backend
  folder: "",            // currently selected folder
  query: "",             // current search query
  expanded: new Set(),   // set of expanded folder paths in the tree
  openItems: new Set()   // set of open (active) bookmark items
};
let openInNewTab = true;      // open links in a new tab (or same tab if false)
let dragIndex = null;         // bookmark index for dragging
let draggedFolder = null;     // path of dragged folder
let draggedBookmark = null;   // id or data for dragged bookmark
let hoverFolder = null,       // currently spring-loaded (hovered) folder
    hoverTimer = null;        // timer for spring-loading folders

// While dragging, hovering a collapsed folder with children expands it after 1 second delay.
function springLoad(path, hasKids, expanded) {
  if (path === hoverFolder) return;
  hoverFolder = path;
  clearTimeout(hoverTimer);
  hoverTimer = null;
  if (hasKids && !expanded) {
    hoverTimer = setTimeout(() => {
      hoverTimer = null;
      state.expanded.add(path);
      expandRowInPlace(path);
    }, 750);
  }
}

// The tree row for a folder path
function folderRow(path) {
  return $("folderTree").querySelector('.folder-row[data-path="' + CSS.escape(path) + '"]');
}

// Expand `path` by splicing its child rows in rather than re-rendering the tree
function expandRowInPlace(path) {
  const row = folderRow(path);
  if (!row) { renderTree(); return; }
  let map = buildFolderTree();
  for (const part of path.split("/")) {
    if (!map[part]) return;
    map = map[part].children;
  }
  const frag = document.createDocumentFragment();
  renderTreeInto(map, frag, path.split("/").length);
  row.after(frag);
  const ic = row.querySelector(".folder-caret .ic");
  if (ic) ic.className = "ic ic-arrow-down";
}

// Cancel the folder spring-load timer and reset the hover state.
function cancelSpring() {
  clearTimeout(hoverTimer);
  hoverTimer = null;
  hoverFolder = null;
}

// Drag preview: a 30%-transparent clone that follows the cursor
let dragPreview = null, dragDX = 0, dragDY = 0;
let dragSepW = 0;

// Separator span for a folder row
function folderSpan(row) {
  const btn = row.querySelector(".tree-node");
  if (!btn) return { x: 0, w: 0 };
  const caret = row.querySelector(".folder-caret");
  const rr = row.getBoundingClientRect();
  const range = document.createRange();
  range.selectNodeContents(btn);
  const title = range.getBoundingClientRect();
  const hasArrow = caret && !caret.classList.contains("leaf");
  const left = hasArrow ? caret.getBoundingClientRect().left : title.left;
  return { x: left - rr.left, w: title.right - left };
}
const BLANK_IMG = new Image();
BLANK_IMG.src = "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";

function startDragPreview(e, el) {
  const r = el.getBoundingClientRect();
  dragDX = e.clientX - r.left;
  dragDY = e.clientY - r.top;
  endDragPreview();
  const p = el.cloneNode(true);
  p.style.cssText =
    "position:fixed;left:0;top:0;margin:0;z-index:9999;pointer-events:none;opacity:.7;box-sizing:border-box;" +
    "min-width:0;width:" + r.width + "px";
  document.body.appendChild(p);
  dragPreview = p;
  e.dataTransfer.setDragImage(BLANK_IMG, 0, 0);
  moveDragPreview(e);
}
function moveDragPreview(e) {
  if (!dragPreview || (e.clientX === 0 && e.clientY === 0)) return;
  dragPreview.style.transform = "translate(" + (e.clientX - dragDX) + "px," + (e.clientY - dragDY) + "px)";
}
function endDragPreview() {
  if (dragPreview) { dragPreview.remove(); dragPreview = null; }
}
document.addEventListener("dragover", moveDragPreview);
document.addEventListener("dragend", endDragPreview);
document.addEventListener("drop", endDragPreview);
document.addEventListener("mousemove", endDragPreview);

// Opens the given URL in a new tab or the same tab based on openInNewTab setting.
function openLink(url) {
  if (openInNewTab) window.open(url, "_blank", "noopener");
  else window.location.href = url;   // same tab
}

// Makes an HTTP request to the backend API and returns parsed JSON result.
async function api(method, path, body) {
  const options = { 
    method, 
    headers: {} 
  };

  if (body !== undefined) {
    options.headers["Content-Type"] = "application/json";
    options.body = JSON.stringify(body);
  }

  const response = await fetch(path, options);

  let errorText = res => res.json().catch(() => ({}));

  if (!response.ok) {
    // Try to extract error message from the response, fallback to HTTP status code
    throw new Error((await errorText(response)).error || response.status);
  }

  // Only attempt to parse JSON on 200/201
  if (response.status === 200 || response.status === 201) {
    return response.json();
  }
  return null;
}

// Loads all bookmarks from the backend API and triggers a UI render.
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

// Build a nested {name: {path, children, order}} tree. `order` = the file index
// of the folder's first bookmark, so folders keep their manual (file) order.
function buildFolderTree() {
  const root = {};
  state.bookmarks.forEach((b, idx) => {
    if (!b.folder) return;
    let map = root, acc = "";
    for (const part of b.folder.split("/")) {
      acc = acc ? acc + "/" + part : part;
      if (!map[part]) map[part] = { path: acc, children: {}, order: idx };
      else if (idx < map[part].order) map[part].order = idx;
      map = map[part].children;
    }
  });
  return root;
}

// Sorted child names of a folder node, in manual (file) order.
function orderedNames(map) {
  return Object.keys(map).sort((a, b) => map[a].order - map[b].order);
}

// Immediate child folder paths of `parent` ("" = top level), in manual order.
function childFolders(parent) {
  let map = buildFolderTree();
  if (parent) {
    for (const part of parent.split("/")) {
      if (!map[part]) return [];
      map = map[part].children;
    }
  }
  return orderedNames(map).map((k) => map[k].path);
}

function renderTreeInto(map, container, depth) {
  for (const name of orderedNames(map)) {
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
        if (state.expanded.has(path)) state.expanded.delete(path); else state.expanded.add(path);
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

    if (active) {  // '⋯' menu (Rename / Delete) on the selected folder
      const wrap = document.createElement("span");
      wrap.className = "folder-menu-wrap";
      const mbtn = document.createElement("button");
      mbtn.className = "folder-menu-btn";
      mbtn.title = "Folder options";
      mbtn.appendChild(Object.assign(document.createElement("span"), { className: "dots" }));
      const menu = document.createElement("div");
      menu.className = "folder-menu";
      menu.hidden = true;
      menu.onclick = (e) => e.stopPropagation();
      const ren = document.createElement("button"); ren.textContent = "Rename";
      ren.onclick = () => { menu.hidden = true; openFolderForm(path); };
      const del = document.createElement("button"); del.className = "folder-del"; del.textContent = "Delete";
      del.onclick = () => { menu.hidden = true; deleteFolder(path); };
      menu.append(ren, del);
      mbtn.onclick = (e) => {
        e.stopPropagation(); e.preventDefault();
        const wasHidden = menu.hidden;
        closeFolderMenus();
        menu.hidden = !wasHidden;
      };
      wrap.append(mbtn, menu);
      row.appendChild(wrap);
    }

    // drag to reorder: the folder lands exactly where the separator is drawn,
    // and no separator is drawn where nothing would happen
    row.dataset.path = path;
    row.draggable = true;
    row.ondragstart = (e) => {
      e.stopPropagation();
      draggedFolder = path;
      dragSepW = folderSpan(row).w;
      e.dataTransfer.effectAllowed = "move";
      e.dataTransfer.setData("text/plain", path);
      row.classList.add("dragging");
      startDragPreview(e, row);
    };
    row.ondragend = () => { row.classList.remove("dragging"); clearFolderMarks(); cancelSpring(); draggedFolder = null; };
    row.ondragover = (e) => {
      const open = state.expanded.has(path);
      if (draggedBookmark) {
        springLoad(path, hasKids, open);
        clearFolderMarks();
        if (draggedBookmark.folder === path) return;   // already here
        e.preventDefault();
        row.classList.add("drop-into");
        return;
      }
      if (draggedFolder === null) return;
      if (path !== draggedFolder) springLoad(path, hasKids, open);
      const zone = folderZone(e, row, hasKids && open);
      const t = zone && folderTarget(row, zone);
      clearFolderMarks();
      if (!t) return;
      e.preventDefault();
      if (zone === "into") row.classList.add("drop-into");
      else markGap(row, zone);
    };
    row.ondrop = (e) => {
      e.preventDefault();
      clearFolderMarks();
      cancelSpring();
      if (draggedBookmark) {
        const bm = draggedBookmark;
        if (bm.folder !== path) moveBookmark(bm, path);
        return;
      }
      if (draggedFolder === null) return;
      const zone = folderZone(e, row, hasKids && state.expanded.has(path));
      const t = zone && folderTarget(row, zone);
      if (!t) return;
      if (zone === "into") state.expanded.add(path);
      moveFolder(draggedFolder, t.to, t.before);
    };

    container.appendChild(row);

    if (hasKids && expanded) renderTreeInto(node.children, container, depth + 1);
  }
}

// --- drop targeting ---
function parentOf(path) { return path.split("/").slice(0, -1).join("/"); }

function folderZone(e, row, showsKids) {
  const r = row.getBoundingClientRect();
  if (!r.height) return null;
  const band = showsKids ? 0.4 : 0.3;
  const y = e.clientY - r.top;
  if (y < r.height * band) return "before";
  if (y > r.height * (1 - band)) return "after";
  return "into";
}

function gapNext(row, zone) {
  return zone === "before" ? row : row.nextElementSibling;
}

// Resolve a row + zone to the move payload, or null when the drop is not
// allowed or would leave the folder exactly where it already is
function folderTarget(row, zone) {
  const from = draggedFolder;
  const leaf = from.split("/").pop();
  let to, before;
  if (zone === "into") {
    const path = row.dataset.path;
    to = path + "/" + leaf;
    before = childFolders(path).find((p) => p !== from) || "";
  } else {
    const next = gapNext(row, zone);
    if (next && next.dataset.path === from) return null;
    const parent = next ? parentOf(next.dataset.path) : "";
    to = parent ? parent + "/" + leaf : leaf;
    before = next ? next.dataset.path : "";
  }
  if (!canMoveFolder(from, to)) return null;
  if (to === from && before === nextSibling(from)) return null;  // already there: nothing to do
  return { to, before };
}

// Paint the separator in the gap, indented to the level the folder lands ats
function markGap(row, zone) {
  const tree = $("folderTree");
  const next = gapNext(row, zone);
  const above = next ? next.previousElementSibling : tree.lastElementChild;
  const level = next || tree.firstElementChild;
  if (above) { above.classList.add("drop-after"); setSeparatorSpan(above, level); }
  else if (next) { next.classList.add("drop-before"); setSeparatorSpan(next, next); }
}

function canMoveFolder(from, to) {
  if (!to) return false;
  return !to.startsWith(from + "/");     // not into own descendant (to === from is ok: reorder)
}
function clearFolderMarks() {
  document.querySelectorAll(".drop-into, .drop-before, .drop-after, .drop-root")
    .forEach((el) => el.classList.remove("drop-into", "drop-before", "drop-after", "drop-root"));
}

// Position the drop separator
function setSeparatorSpan(row, levelRow) {
  levelRow = levelRow || row;
  if (!levelRow.querySelector(".tree-node")) return;
  const s = folderSpan(levelRow);
  row.style.setProperty("--sep-x", s.x + "px");
  row.style.setProperty("--sep-w", (dragSepW || s.w) + "px");
}

async function moveFolder(from, to, before) {
  try { await api("POST", "/folders/move", { from, to, before }); await load(); }
  catch (err) { alert("Move failed: " + err.message); }
}

async function moveBookmark(b, folder) {
  try {
    await api("PUT", "/bookmarks/" + encodeURIComponent(b.id), { name: b.name, folder, url: b.url });
    await load();
  } catch (err) { alert("Move failed: " + err.message); }
}

// --- web-styled confirmation dialog (returns a Promise<boolean>) ---
let confirmResolve = null;
function confirmDialog(msg, okLabel) {
  $("confirmMsg").textContent = msg;
  $("confirmOk").textContent = okLabel || "Delete";
  $("confirmOverlay").hidden = false;
  return new Promise((resolve) => { confirmResolve = resolve; });
}
function closeConfirm(result) {
  $("confirmOverlay").hidden = true;
  if (confirmResolve) { confirmResolve(result); confirmResolve = null; }
}
$("confirmOk").onclick = () => closeConfirm(true);
$("confirmCancel").onclick = () => closeConfirm(false);
$("confirmOverlay").onclick = (e) => { if (e.target === $("confirmOverlay")) closeConfirm(false); };

// --- folder ⋯ menu: rename / delete ---
function closeFolderMenus() {
  document.querySelectorAll(".folder-menu").forEach((m) => (m.hidden = true));
}
function nextSibling(path) {
  const parent = parentOf(path);
  const sibs = childFolders(parent);
  const i = sibs.indexOf(path);
  return i >= 0 && i + 1 < sibs.length ? sibs[i + 1] : "";
}
function openFolderForm(path) {
  $("fd-path").value = path;
  $("fd-name").value = path.split("/").pop();
  closeNav(); closeFolderMenus();
  $("folderOverlay").hidden = false;
  $("fd-name").focus(); $("fd-name").select();
}
function closeFolderForm() { $("folderOverlay").hidden = true; $("folderForm").reset(); }

async function submitFolderForm(e) {
  e.preventDefault();
  const path = $("fd-path").value;
  const leaf = $("fd-name").value.trim().replace(/^\/+|\/+$/g, "");
  if (!leaf) return;
  const parent = path.split("/").slice(0, -1).join("/");
  const to = parent ? parent + "/" + leaf : leaf;
  if (to === path) { closeFolderForm(); return; }
  try {
    await api("POST", "/folders/move", { from: path, to, before: nextSibling(path) });  // keep position
    if (state.folder === path) state.folder = to;
    else if (state.folder.startsWith(path + "/")) state.folder = to + state.folder.slice(path.length);
    closeFolderForm();
    await load();
    showToast("Folder renamed", "success");
  } catch (err) { showToast("Rename failed: " + err.message, "error"); }
}

async function deleteFolder(path) {
  if (!(await confirmDialog(`Delete folder "${path}" and all its bookmarks?`))) return;
  try {
    await api("POST", "/folders/delete", { folder: path });
    if (state.folder === path || state.folder.startsWith(path + "/")) state.folder = "";
    await load();
    showToast("Folder deleted", "success");
  } catch (err) { showToast("Delete failed: " + err.message, "error"); }
}

// Expand a folder and all its ancestors so it's visible + highlighted in the tree.
function revealInTree(path) {
  const parts = path.split("/");
  let acc = "";
  for (const p of parts) { acc = acc ? acc + "/" + p : p; state.expanded.add(acc); }
}

function renderTree() {
  const tree = $("folderTree");
  tree.textContent = "";
  $("homepage").classList.toggle("active", state.folder === "");
  wireRootDrop($("homepage"), tree);
  renderTreeInto(buildFolderTree(), tree, 0);
}

// Dropping onto Homepage or into the empty space under the tree sends a
// folder to the end of the top level
// Homepage takes subfolders only
function wireRootDrop(home, tree) {
  const rootTarget = () => {
    if (draggedFolder === null) return null;
    const leaf = draggedFolder.split("/").pop();
    if (!canMoveFolder(draggedFolder, leaf)) return null;
    if (leaf === draggedFolder && nextSibling(draggedFolder) === "") return null;
    return { to: leaf, before: "" };
  };
  const homeTarget = () =>
    draggedFolder !== null && draggedFolder.includes("/") ? rootTarget() : null;

  home.ondragover = (e) => {
    const t = homeTarget();
    clearFolderMarks();
    if (!t) return;
    e.preventDefault();
    home.classList.add("drop-root");
  };
  home.ondragleave = (e) => { if (e.target === home) home.classList.remove("drop-root"); };
  home.ondrop = (e) => {
    const t = homeTarget();
    e.preventDefault();
    clearFolderMarks();
    cancelSpring();
    if (t) moveFolder(draggedFolder, t.to, t.before);
  };

  tree.ondragover = (e) => {
    if (e.target.closest(".folder-row")) return;
    const t = rootTarget();
    clearFolderMarks();
    if (!t) return;
    e.preventDefault();
    const last = tree.lastElementChild;
    if (last) markGap(last, "after");
  };
  tree.ondragleave = (e) => { if (!tree.contains(e.relatedTarget)) clearFolderMarks(); };
  tree.ondrop = (e) => {
    if (e.target.closest(".folder-row")) return;
    const t = rootTarget();
    e.preventDefault();
    clearFolderMarks();
    cancelSpring();
    if (t) moveFolder(draggedFolder, t.to, t.before);
  };
}

// Populate the folder autocomplete datalist used by the new/edit form.
// custom folder autocomplete for the new/edit form (looks like the sidebar tree)
function showFolderSuggest() {
  const box = $("folderSuggest");
  const q = $("f-folder").value.toLowerCase();
  const items = folderSet().filter((p) => p.toLowerCase().includes(q)).slice(0, 50);
  box.textContent = "";
  if (!items.length) { box.hidden = true; return; }
  for (const path of items) {
    const it = document.createElement("div");
    it.className = "suggest-item";
    it.appendChild(Object.assign(document.createElement("span"), { className: "ic ic-folder" }));
    it.appendChild(Object.assign(document.createElement("span"), { className: "suggest-name", textContent: path }));
    it.onmousedown = (e) => e.preventDefault();   // keep the input focused
    it.onclick = () => { $("f-folder").value = path; box.hidden = true; $("f-url").focus(); };
    box.appendChild(it);
  }
  box.hidden = false;
}
function hideFolderSuggest() { $("folderSuggest").hidden = true; }

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
  const cfg = await api("GET", "/config").catch(() => null);
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

    // drag to reorder (native DnD) — a plain click still opens the link
    a.draggable = false;
    tile.draggable = true;
    tile.ondragstart = (e) => {
      e.dataTransfer.effectAllowed = "move";
      e.dataTransfer.setData("text/plain", String(i));
      dragIndex = i;
      tile.classList.add("dragging");
      startDragPreview(e, tile);
    };
    tile.ondragover = (e) => { e.preventDefault(); e.dataTransfer.dropEffect = "move"; };
    tile.ondragenter = (e) => {
      e.preventDefault();
      if (dragIndex !== null && dragIndex !== i) tile.classList.add("drop-target");
    };
    tile.ondragleave = () => tile.classList.remove("drop-target");
    tile.ondrop = (e) => {
      e.preventDefault();
      tile.classList.remove("drop-target");
      if (dragIndex !== null && dragIndex !== i) swapShortcuts(dragIndex, i);
    };
    tile.ondragend = () => {
      tile.classList.remove("dragging");
      document.querySelectorAll(".drop-target").forEach((t) => t.classList.remove("drop-target"));
      dragIndex = null;
    };

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
  if (!(await confirmDialog("Delete this shortcut?"))) return;
  try { await api("DELETE", "/shortcuts/" + encodeURIComponent(index)); await renderShortcuts(); }
  catch (err) { alert("Delete failed: " + err.message); }
}

async function swapShortcuts(a, b) {
  try { await api("POST", "/shortcuts/swap", { a, b }); await renderShortcuts(); }
  catch (err) { alert("Swap failed: " + err.message); }
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
    item.onclick = () => { state.folder = path; revealInTree(path); render(); };   // navigate + reveal in sidebar
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
    if (folderView) {
      item.classList.add("collapsible");                              // caret toggles actions/url
      if (state.openItems.has(b.id)) item.classList.add("open");
    }
    if (valid) item.onclick = () => openLink(b.url);   // whole item clickable (mode from config)

    // drag a bookmark onto a sidebar folder to move it there
    item.draggable = true;
    item.ondragstart = (e) => {
      e.stopPropagation();
      draggedBookmark = b;
      e.dataTransfer.effectAllowed = "move";
      e.dataTransfer.setData("text/plain", b.id);
      item.classList.add("dragging");
      startDragPreview(e, item);
    };
    item.ondragend = () => { item.classList.remove("dragging"); clearFolderMarks(); cancelSpring(); draggedBookmark = null; };

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
  endDragPreview();
  renderTree();
  const folderView = !!state.folder;
  const browsing = folderView || !!state.query;
  $("main").classList.toggle("browsing", browsing);
  $("searchWrap").hidden = folderView;   // hide search when viewing a folder
  $("shortcuts").hidden = browsing;
  $("appmenu").hidden = browsing;        // app menu only on the homepage
  $("footer").hidden = browsing;         // footer only on the homepage
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
function closeForm() { $("overlay").hidden = true; $("form").reset(); hideFolderSuggest(); }

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
  if (!(await confirmDialog("Delete this bookmark?"))) return;
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
$("sideClose").onclick = closeNav;   // hamburger inside the drawer (mobile) closes it

// top-right app menu + import bookmarks
$("appmenuBtn").onclick = (e) => { e.stopPropagation(); $("appmenuList").hidden = !$("appmenuList").hidden; };
$("appmenuList").onclick = (e) => e.stopPropagation();
document.addEventListener("click", () => { $("appmenuList").hidden = true; });
$("importBtn").onclick = () => { $("appmenuList").hidden = true; $("importFile").click(); };
$("importFile").onchange = async (e) => {
  const file = e.target.files[0];
  e.target.value = "";                       // allow re-selecting the same file
  if (!file) return;
  try {
    const text = await file.text();
    const res = await fetch("/import", { method: "POST", headers: { "Content-Type": "text/html" }, body: text });
    if (!res.ok) throw new Error((await res.json().catch(() => ({}))).error || res.status);
    const r = await res.json();
    await load();                            // refresh page / folder structure
    showToast(`Imported ${r.added} bookmark(s)` + (r.skipped ? ` · ${r.skipped} skipped` : ""), "success");
  } catch (err) {
    showToast("Import failed: " + err.message, "error");
  }
};

// transient toast notification (top-right)
function showToast(msg, type) {
  const t = document.createElement("div");
  t.className = "toast" + (type ? " " + type : "");
  t.textContent = msg;
  $("toasts").appendChild(t);
  requestAnimationFrame(() => t.classList.add("show"));
  setTimeout(() => {
    t.classList.remove("show");
    setTimeout(() => t.remove(), 250);
  }, type === "error" ? 6000 : 4000);
}

// edge-swipe: drag right from the left edge to open the drawer, left to close (mobile)
(function () {
  const EDGE = 28, DIST = 45;
  let x0 = 0, y0 = 0, track = false;
  addEventListener("touchstart", (e) => {
    if (window.innerWidth > 768) { track = false; return; }
    const t = e.touches[0];
    track = document.body.classList.contains("nav-open") || t.clientX <= EDGE;
    x0 = t.clientX; y0 = t.clientY;
  }, { passive: true });
  addEventListener("touchmove", (e) => {
    if (!track) return;
    const t = e.touches[0], dx = t.clientX - x0, dy = t.clientY - y0;
    if (Math.abs(dx) < DIST || Math.abs(dy) > Math.abs(dx)) return;   // need a clear horizontal swipe
    const open = document.body.classList.contains("nav-open");
    if (dx > 0 && !open) document.body.classList.add("nav-open");
    else if (dx < 0 && open) closeNav();
    track = false;
  }, { passive: true });
  addEventListener("touchend", () => { track = false; });
})();

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
$("f-folder").addEventListener("input", showFolderSuggest);
$("f-folder").addEventListener("focus", showFolderSuggest);
$("f-folder").addEventListener("blur", () => setTimeout(hideFolderSuggest, 150));
$("overlay").onclick = (e) => { if (e.target === $("overlay")) closeForm(); };  // click backdrop to close

// new-shortcut popup wiring + color/hex sync
$("shortcutForm").onsubmit = submitShortcut;
$("sc-cancel").onclick = closeShortcutForm;
$("shortcutOverlay").onclick = (e) => { if (e.target === $("shortcutOverlay")) closeShortcutForm(); };
$("sc-color").oninput = (e) => {
  if (/^#[0-9a-fA-F]{6}$/.test(e.target.value)) $("sc-color-picker").value = e.target.value;
};
$("sc-color-picker").oninput = (e) => { $("sc-color").value = e.target.value; };

$("folderForm").onsubmit = submitFolderForm;
$("fd-cancel").onclick = closeFolderForm;
$("folderOverlay").onclick = (e) => { if (e.target === $("folderOverlay")) closeFolderForm(); };

document.addEventListener("keydown", (e) => {
  if (e.key === "Escape") { closeForm(); closeShortcutForm(); closeFolderForm(); closeConfirm(false); closeShortcutMenus(); closeFolderMenus(); }
});
document.addEventListener("click", () => { closeShortcutMenus(); closeFolderMenus(); });   // click anywhere closes menus

renderShortcuts();
load();
