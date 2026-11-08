# 09 · Frontend Plan
Plain **HTML + CSS + JavaScript** (no framework), multi-page, light theme, rupee amounts in Indian grouping (₹1,65,080), dates as `07 Nov 2026`. Look and layout are fixed by the mockups (clean light dashboard, left sidebar, **Live SQL** toggle at top right).

## 1. Scope decision: what the frontend shows of the database
- **Frontend:** only the queries **caused by the action the user just performed**, each with its **output**, in the Live SQL panel (and on the Database Model page the ER diagram).
- **Aiven + DBMS report:** the **complete set** of creation scripts with outputs: CREATE TABLE, constraints, triggers, views, INSERT of all data, indexes, users/grants. These are not browsed in the frontend.
- Therefore the earlier "Query Explorer / Triggers / Transactions / Constraints / DCL / Index" lab pages are **removed from the frontend**. Their demonstrations are shown by normal actions (a failed sale shows the constraint/trigger error and ROLLBACK in the panel) and documented in the report.

## 2. Pages
| # | File | Roles | Purpose |
|---|---|---|---|
| 1 | `login.html` | all | Login, role redirect |
| 2 | `dashboard.html` | all | Dashboard: 4 stat cards, restock alerts (priority queue), recent stock transactions, Undo last |
| 3 | `products.html` | admin: full; employee: view/search | Table with search (name/category/id), sort, filters, add/edit/delete modal, photo upload/preview |
| 4 | `categories.html` | admin | List with product count; add/edit/delete (delete blocked if used → shows FK error) |
| 5 | `parties.html` | admin: full; employee: view | Tabs **Customers** / **Suppliers**: list, add, edit, delete |
| 6 | `purchases.html` | admin, employee | Supplier + multi-item form → stock in; history list |
| 7 | `sales.html` | admin, employee | Customer + multi-item form; FIFO order queue panel; history list |
| 8 | `reports.html` | all | Report picker + table: stock value, top sellers, sales per customer, purchases per supplier, never-sold products, low stock |
| 9 | `users.html` | admin | Add/remove users, set role, reset password |
| 10 | `model.html` | admin | Database Model: ER diagram image, table list with columns/keys from `information_schema` |

## 3. Global layout and behaviour
**Shell (all pages except login):** left sidebar (logo, nav, user box with role and Logout), top bar (page title, "Database connected" badge, **Live SQL toggle**), content area, optional right SQL panel. Sidebar items hidden when the role lacks access.

**Live SQL toggle**
- Top-right switch; **default off**; state stored in `localStorage` (`liveSql=on|off`).
- Panel (dark, 440 px by default) slides in from the right; content area shrinks; switch back to off hides it. Collapse button in the panel footer does the same. Drag its left edge to resize it between 340 px and 60 % of the window (900 px at most); arrow keys on the edge do the same, double-click resets. The width is remembered.
- The backend records SQL for every request regardless of the switch, so opening the panel shows the last action(s) immediately. The panel lists actions oldest first, newest at the bottom, scrolls to each new statement, and keeps the last 80 statements for the browser tab (sessionStorage), so the log continues across pages.
- Panel header: "Live SQL · last action N ms"; each action shows time, method and path, statement count and time; each statement shows its number, kind (DQL/DML/TCL), the SQL with one clause per line, then the result table (scrolls sideways) or "Query OK, N rows affected"; footer: "N statements", Clear, Collapse.

**Panel content per statement (what "with output" means)**
1. Sequence number and SQL text (keywords coloured). 2. **Output:** for SELECT a small result table (up to 5 rows + "N rows"), for INSERT/UPDATE/DELETE "N row(s) affected" and generated id, for TCL "OK". 3. **Trigger notes** under the statement that fired them (green = allowed/passed, amber = action taken). 4. **Before/After** boxes for changed rows (stock, totals). 5. **Error** (red) with MySQL error number and message, followed by `ROLLBACK`. 6. Time per request.

**Common UI parts:** toast messages (success/error), modal dialogs for add/edit, confirm dialog for delete, loading spinner on buttons, empty-state text ("No products yet"), inline field validation, table sort by clicking headers, pagination (10 rows) on long tables.

## 4. Page details
**Login:** username, password, Sign in; on failure show "Invalid username or password"; on success store session (HTTP-only cookie set by server) and redirect by role.

**Dashboard (`GET /api/dashboard`, `/api/alerts/low-stock`, `/api/transactions/recent`):** cards: Products, Stock value, Sales this month, Low stock. Restock table with status tag (Critical if stock ≤ 50 % of minimum, Low otherwise). Recent transactions with type tags. **Undo last** (admin) calls `POST /api/undo`. Label "ADS · Priority queue" / "ADS · Stack" next to the sections. Panel shows the SELECTs that loaded the page.

**Products:** search box (calls `GET /api/products?search=`), category filter, sort select and clickable headers (`sort`, `order`), results count. Row actions: Edit, Delete (admin). Add/Edit modal fields: SKU, name, category, supplier, unit price, stock, minimum stock, expiry (optional), description (optional), photo (optional, ≤ 1 MB, image only). Errors from MySQL (duplicate SKU, negative price) appear in the modal and in the panel. Label "ADS · Hash map / Merge sort / Binary search" shown on the toolbar tooltip.

**Sales:** customer select, date (today), item rows (product select shows SKU and in-stock, quantity, price auto-filled, line total), Add item, running total, **Save Sale**. Result toast plus panel steps. Quantity above stock disables Save and shows warning; server still enforces (trigger error shown if forced). **Order queue** card shows pending/processed orders in FIFO order ("ADS · Queue"). History table below.

**Purchases:** supplier select, item rows (product, quantity, unit cost), Save Purchase, history.

**Reports:** left list of reports, right table with the query result and row count; export CSV button (client side). Each run shows its SELECT in the panel.

**Customers/Suppliers:** table + modal (name, phone with 10-digit pattern, email, city; supplier adds contact person). Delete blocked by FK shows "In use by N records".

**Users (admin):** table (username, role, created), add user modal (password ≥ 8 characters), delete (not self).

**Database Model:** ER diagram image (zoom on click), relational schema image, table cards listing columns, types, PK/FK from `GET /api/model`.

## 5. Files
```
public/
  login.html dashboard.html products.html categories.html parties.html
  purchases.html sales.html reports.html users.html model.html
  css/  style.css  (tokens + components)
  js/   api.js       fetch wrapper, error handling, adds sql_log to panel
        session.js   who am I, role guard, logout
        layout.js    builds sidebar + top bar + panel on every page
        sqlpanel.js  toggle, render statements, outputs, errors, before/after
        format.js    ₹ and date formatting, escape HTML
        ui.js        toast, modal, confirm, table helpers (sort/paginate)
        pages/       products.js sales.js purchases.js ... one file per page
  img/  er_diagram.svg schema.svg favicon.svg (diagrams drawn by scripts/make_diagrams.py)
  mock/ sample JSON responses (used when `?mock=1`)
```
Rules: each file < ~250 lines, each function commented in one line, no inline event handlers, all text escaped before insertion (no `innerHTML` with raw data).

**As built (Phase 6):** the file list above also gained `js/mock.js` (the mock router used by `api.js`), `js/crud.js` (shared list + add/edit/delete for categories, customers, suppliers) and `js/itemform.js` (shared multi-item form for sales and purchases). The mock JSON in `public/mock/` is generated from real API answers by `scripts/make_mock.py`. Sales has an admin-only demo checkbox that sends a sale even when stock is too low, so the trigger error 1644 and the ROLLBACK can be shown on purpose.

## 6. Design tokens (from the mockups)
Same brand as the landing page: ink navy `#101b33` sidebar and primary buttons, shipping-tape yellow `#ffc72c` for the active item, focus ring and switch, background `#eef1f5`, cards `#ffffff` with border `#dde3ea`; success `#138a4a`, warning `#b26a00`, danger `#c62a22`; Live SQL panel `#0b1328`. Headings and numbers in a condensed face (Avenir Next Condensed, fallback Arial Narrow), body in Avenir Next / Segoe UI, SQL in the system monospace. Radius 10 px inputs, 16 px cards. Light theme only.

## 7. API contract the frontend relies on
**Every response:** `{ "ok": true|false, "data": …, "error": {"code","mysql_errno","message"}?, "sql_log": [ … ] }`.
**`sql_log` entry:**
```json
{ "seq": 3, "sql": "INSERT INTO Sale_Items (...) VALUES (...)", "kind": "DML",
  "rows_affected": 1, "result": {"columns":[…],"rows":[[…]],"total":N} ,
  "triggers": [{"name":"trg_sale_items_bi","note":"stock 38 ≥ 3 → allowed","level":"ok"},
               {"name":"trg_sale_items_ai","note":"stock 38 → 35, sale total 1950.00, ledger row added","level":"action"}],
  "before": {"table":"Products","key":6,"row":{"stock_quantity":38}},
  "after":  {"table":"Products","key":6,"row":{"stock_quantity":35}},
  "error": null, "ms": 4 }
```
Employee-forbidden calls return HTTP 403 with `error.code = "FORBIDDEN"`; unauthenticated calls return 401 and the frontend redirects to login. Full endpoint list: `03-API-Spec.md` (add `/api/dashboard`, `/api/transactions/recent`, `/api/users`, `/api/model`, `/api/categories`).

## 8. Mock mode
`?mock=1` makes `api.js` read `public/mock/*.json` instead of calling the server, so every page can be built and demonstrated before the C++ backend exists. Mock files must include: products (16), categories, customers, suppliers, alerts, a successful sale with full `sql_log`, a failed sale with error and rollback, report results.

## 9. Build order
1. `style.css`, `layout.js`, `sqlpanel.js`, `ui.js`, `format.js`, `api.js` (shell and panel working with mock data).
2. `login.html`, `dashboard.html`.
3. `products.html` (full CRUD), `categories.html`.
4. `parties.html`, `sales.html`, `purchases.html`.
5. `reports.html`, `users.html`, `model.html`.
6. Switch from mock to real API page by page; fix mismatches.

## 10. Acceptance checklist
- [ ] Toggle off hides the panel; on shows it; state survives reload.
- [ ] Every create/update/delete/sale/purchase shows its SQL **with output** in the panel.
- [ ] Oversell shows red error 1644 and ROLLBACK; stock on the page is unchanged.
- [ ] Duplicate SKU, negative price and deleting a used category show the MySQL error text.
- [ ] Search, sort, pagination work; FIFO queue and restock list display in the right order.
- [ ] Employee cannot see admin pages or delete buttons; direct URL access redirects.
- [ ] Layout works at 1366 × 768 and 1920 × 1080; no horizontal scroll; keyboard focus visible.
- [ ] Page text is escaped (typing `<script>` in a name shows as text).
- [ ] Works with `?mock=1` and with the real backend.

## 11. Confirmed decisions
| Topic | Decision |
|---|---|
| Sale price | Fixed from `Products.unit_price`; no editing, no discount (the price is copied into `Sale_Items.unit_price` at sale time) |
| Walk-in customers | A pre-created **"Walk-in Customer"** row (customer_id fixed in seed data) is the default in the sale form |
| Employee role | Record sales and purchases; view products, reports and low-stock; **add and edit customers**; no deletes, no product/category/supplier/user changes |
| Bill | Printable bill page `bill.html?sale_id=N` opened after Save Sale (print stylesheet, shop header, lines, total); endpoint `GET /api/sales/{id}/bill` |
| Sessions | Server session cookie (HTTP-only), expires after **8 hours**; 401 → redirect to login |
| Product delete | Blocked by foreign keys when the product has sales/purchases; MySQL error 1451 shown in the toast and panel |
| Demo data | **60 products, 20 customers (including Walk-in), 25 sales**, with matching purchases, suppliers and categories |
| Builder | The frontend is built in this project from this plan, not from an external tool |

**Consequences**
- Add page 11: `bill.html` (all roles who can sell).
- Seed data is larger than the DBMS report's (16 products); the report's outputs will be re-generated from the new data, and creation scripts are re-run on Aiven.
- `customers` add/edit endpoints are open to the Employee role; delete stays admin-only.
