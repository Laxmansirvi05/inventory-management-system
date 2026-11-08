# 08 · Implementation Plan
The deadline is not fixed yet, so the plan is expressed in **working days (D1–D14)**; stretch or compress as needed. Each phase ends with a **checkpoint** that can be demonstrated and a **viva note** (what must be explainable).

**If time runs short, drop in this order:** expiry/invoice → undo (stack) → photo (BLOB) → simplify reports. Never drop: products CRUD, search, sort, purchases/sales with triggers, FIFO queue, low-stock priority queue, login, the live SQL panel and the Query Explorer.

## Status as built (08 Nov 2026)
| Phase | State | Notes |
|---|---|---|
| 0 Setup, 1 Database | done | 10 tables, 7 triggers, 3 views, seed (60 products); MySQL C API instead of Connector/C++ |
| 2–3 Core structures and stock logic | done | `core_tests` 60/60 |
| 4 Database layer, 5 REST server | done | `db_tests` 46/46, `api_smoke` 138/138 |
| 6 Frontend | done | 11 pages, Live SQL panel, mock mode (localhost only) |
| 7 DBMS demonstration | done differently | the Lab pages were replaced by the Live SQL panel plus `sql/syllabus_demo.sql` |
| 8 Deployment | done | Docker image, Render service in Singapore, Aiven MySQL; see `10-Deployment.md` |
| 9 Testing and hardening | done | see `11-Audit-Report.md` |
| 10 Documentation and viva | done | `13-Viva-Guide.md`; the DBMS and ADS reports are separate documents |

## Phase 0 · Setup (D1)
**Tasks:** create GitHub repo `inventory`; add folders from TRD §3; install MySQL 8, C++17 compiler, CMake (the MySQL C client library `libmysqlclient` comes with MySQL); download cpp-httplib and nlohmann/json into `third_party/`; follow one code style (4 spaces, `camelCase`, one class per file).
**Done when:** a "hello" C++ program builds with CMake and connects to local MySQL.
**Branches:** `main` (stable), one feature branch per phase, merge by pull request.

## Phase 1 · Database foundation (D1–D2)
**Tasks**
1. `sql/schema.sql`: 9 tables from the DBMS report + `Users` + `Products.photo` (via ALTER, kept as a separate statement).
2. `sql/triggers.sql`: three triggers (reuse verified report code).
3. `sql/views.sql`: `v_low_stock`, `v_sales_summary`, `v_product_catalogue`.
4. `sql/seed.sql`: categories, suppliers, customers, 16 products, purchases, sales, two users (admin, employee with hashed passwords).
5. `sql/roles.sql`: `ims_admin`, `ims_employee` accounts with GRANT/REVOKE.
6. Create the Aiven MySQL service; confirm version ≥ 8.0.31; load all scripts; note any statement Aiven rejects.
**Done when:** scripts run clean on local MySQL **and** Aiven; `SELECT` from each view works; oversell test raises error 1644.
**Viva note:** why each constraint/trigger exists; ER diagram walk-through.

## Phase 2 · Core data structures & algorithms (D2–D4)
Pure C++ with **no database**, tested from a small `main` with sample data (this satisfies the ADS "basic version" in the outline).
| File | Contents |
|---|---|
| `include/product.h` | `struct Product` (id, sku, name, categoryId, supplierId, price, qty, minStock, expiry) |
| `inventory.h/.cpp` | `class Inventory`: `vector<Product>`, `unordered_map<int,Product*>` (id), `unordered_set<string>` (SKU), `map<int,Product*>` (ordered by id); add/update/remove/find |
| `sorting.h/.cpp` | hand-written `mergeSort(vector<Product>&, comparator)`; `linearSearch(name/category)`; `binarySearchById(sortedVector, id)` |
| `orders.h/.cpp` | `queue<Order>`: enqueue, `processNext()` FIFO |
| `restock.h/.cpp` | `priority_queue<LowStockItem>` ordered by `qty/minStock` (lowest first); `buildAlerts()` |
| `undo.h/.cpp` | `stack<StockTxn>`: push on change, `undoLast()` |
**Tests (console):** add duplicate SKU rejected by hash set; binary search finds id 8 in sorted list; sort by price ascending/descending; three orders processed in arrival order; alert list shows lowest ratio first; undo restores quantity.
**Done when:** all console tests pass and each function has a one-line comment stating its complexity.
**Viva note:** why each structure was chosen (see TRD §4), merge sort steps on a 6-element example, binary search needs sorted input.

## Phase 3 · Stock logic (D4–D5)
**Tasks:** `Inventory::purchase(productId, qty)` and `sell(productId, qty)` with the rule *qty ≤ stock*; after each change re-evaluate the low-stock queue; push to undo stack; return a result object (`ok`, message, before/after quantity).
**Done when:** the example from the ADS outline (100 + 50 − 30 = 120, alert below minimum 20) reproduces exactly.

## Phase 4 · Database layer in C++ (D4–D6)
| File | Contents |
|---|---|
| `database.h/.cpp` | `class Database`: connect (SSL, env vars), `loadProducts()`, `execute(sql, params)` with prepared statements, `begin()/commit()/rollback()` |
| `sqllog.h/.cpp` | per-request SQL logger: records statement text, parameters, rows affected, time, MySQL error code; helper to capture **before/after rows** and trigger notes |
| `repository.cpp` | functions per table: products, categories, suppliers, customers, purchases, sales, users, ledger |
**Rules:** every write inside a transaction; commit then update memory; on error rollback and leave memory untouched; no string-concatenated SQL.
**Done when:** console program loads products from MySQL into `Inventory`, records a sale (trigger fires), shows SQL log, and an oversell returns error 1644 with ROLLBACK in the log.
**Viva note:** prepared statements vs injection, why commit before memory update.

## Phase 5 · REST server (D6–D8)
**Tasks:** `server.cpp` with cpp-httplib; routes from `03-API-Spec.md`; JSON responses; `auth.cpp` (password hash check, session token in cookie, role middleware); static file serving of `public/`; consistent error format `{ok:false,error,mysql_errno,sql_log}`.
**Order of routes:** auth → products (GET/POST/PUT/DELETE) → categories/suppliers/customers → purchases → sales → alerts → undo → reports → photo (BLOB).
**Done when:** every route works from `curl`/Postman against local MySQL; employee gets 403 on delete.

## Phase 6 · Frontend (D7–D10)
**Design:** one stylesheet, one colour palette, left sidebar, cards for stats, tables with sort and search boxes, toast messages; plain `fetch` calls; no framework.
| Page | Content |
|---|---|
| `login.html` | username/password, role redirect |
| `dashboard.html` | dashboard: counts, stock value, low-stock list, recent transactions |
| `products.html` | table, search (name/category/id), sort, add/edit/delete modal, photo upload |
| `parties.html` | suppliers and customers tabs |
| `purchases.html` / `sales.html` | multi-item form, order queue display, result with totals |
| `reports.html` | named reports with tables |
| `lab.html` | DBMS Lab (Phase 7) |
**Shared UI:** a collapsible **live SQL panel** on every write page that renders `sql_log`.
**Done when:** a full flow (login → add product → sell → low-stock alert → report) works in the browser.

## Phase 7 · DBMS demonstration and scripts (D8–D11)
The frontend shows only the queries caused by the current action, with their output, in the Live SQL panel (see `09-Frontend-Plan.md`). Everything else is delivered as scripts and report content.
**Tasks:** (1) Database Model page (ER diagram, schema, table details from `information_schema`); (2) Live SQL panel wired to every endpoint; (3) complete creation scripts with captured outputs: CREATE TABLE and constraints, triggers, views, every INSERT, indexes, users and grants, run on local MySQL and on Aiven, saved for the DBMS report and for the Aiven console; (4) trigger, constraint, transaction and rollback behaviour exercised through normal actions and recorded.
**Done when:** every write action shows SQL plus output in the panel, and the full script set runs clean on Aiven with outputs saved.

## Phase 8 · Deployment (D11–D12)
1. `Dockerfile`: build stage (compiler, CMake, connector) → runtime stage with the binary + `public/`.
2. Push to GitHub; create Render web service from the repo; set env vars (`DB_*`, `DB_SSL_CA_PEM`, `COOKIE_SECURE`; see `10-Deployment.md`).
3. Add Aiven CA certificate (as secret file or env var); confirm SSL connection.
4. Smoke-test: login, sale, lab query on the live URL.
5. Write the "wake-up" checklist (open Render and Aiven 5 minutes before a demo).
**Fallback:** local run with the same env-var switch.

## Phase 9 · Testing & hardening (D12–D13)
Run `06-Test-Plan.md` (T1–T19) on local and Aiven; fix defects; check SQL-injection text, role checks, empty inputs; remove debug output; confirm no secrets in Git (`.gitignore` for `.env`).

## Phase 10 · Documentation & viva prep (D13–D14)
Update README run steps; screenshots of each page and Workbench; update PRD/TRD to match final code; prepare a 5-minute demo script and a viva cheat sheet (each structure, algorithm, SQL concept, and "what happens when I click Sell"). Then update the DBMS report (web UI, 10 tables, new outputs) and write the ADS report.

## Timeline at a glance
| Day | D1 | D2 | D3 | D4 | D5 | D6 | D7 | D8 | D9 | D10 | D11 | D12 | D13 | D14 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Setup | ■ | | | | | | | | | | | | | |
| DB (1) | ■ | ■ | | | | | | | | | | | | |
| Core (2–3) | | ■ | ■ | ■ | ■ | | | | | | | | | |
| DB layer (4) | | | | ■ | ■ | ■ | | | | | | | | |
| Server (5) | | | | | | ■ | ■ | ■ | | | | | | |
| Frontend (6) | | | | | | | ■ | ■ | ■ | ■ | | | | |
| Lab (7) | | | | | | | | ■ | ■ | ■ | ■ | | | |
| Deploy (8) | | | | | | | | | | | ■ | ■ | | |
| Test (9) | | | | | | | | | | | | ■ | ■ | |
| Docs/viva (10) | | | | | | | | | | | | | ■ | ■ |

## Checkpoints (demo after each)
C1 (D2) database on local + Aiven · C2 (D5) console ADS demo · C3 (D6) DB-backed console sale · C4 (D8) API works · C5 (D10) full web flow · C6 (D11) DBMS Lab · C7 (D12) live on Render · C8 (D14) final.

## Definition of done (project)
1. All PRD must-haves work in the browser on the live URL.
2. Every action shows its SQL in the live panel; every Practical item in the syllabus mapping is demonstrable.
3. T1–T19 pass.
4. Every part of the code can be explained without notes.
5. README lets a stranger run the project.

## Risks and fallback
| Risk | Fallback |
|---|---|
| Connector/C++ unusable (X protocol only, not on Aiven) | **Decided on D1:** use the MySQL C API (`libmysqlclient`); verified with a prepared-statement test |
| Aiven lacks a needed privilege | Demo GRANT/REVOKE on local MySQL and show screenshots |
| Time shortage | Drop optional items first: expiry, invoice, undo, photo (BLOB), then simplify reports |
| Free tiers asleep during viva | Wake early; keep the local copy ready |

## Open questions
1. Final deadline (to scale D1–D14)? 2. Aiven version and privileges (known after Phase 1). 3. Is login required, or optional? 4. Any marking scheme from the ADS faculty?
