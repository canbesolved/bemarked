// SPDX-License-Identifier: GPL-2.0-only
// bmkd SPA logic. Served same-origin at /app.js (embedded in the binary).
// Thin client: renders + navigates; the backend is authoritative for data/search.

// TODO:
//   - fetch /config/public (shortcuts, grid layout)
//   - fetch /bookmarks -> build the folder tree from '/'-separated folder paths (case-sensitive)
//   - wire the search box (name, folder, url) and folder-prefix filtering
//   - CRUD via POST/PUT/DELETE /bookmarks[/{id}]
