# 11 · Audit Report
**Dates:** audit 08 Nov 2026, follow-up fixes 08 Nov 2026 · **Scope:** backend code, database, frontend, Live SQL, deployment, architecture, documents against requirements · **Method:** read all source; rebuilt from scratch with all compiler warnings on; ran every test suite; tried to break the system (dropped connections, two simultaneous buyers, brute force, path traversal, stale sessions, a database edited behind the app's back) on local test databases; checked the live site.

## 1. Verdict
The system does what the PRD asks, and its core promises hold under test: stock changes only through triggers inside transactions, memory is updated only after COMMIT, no oversell is possible even with two simultaneous buyers, and every data-changing action shows its SQL in the Live SQL panel. The audit found one serious defect (no database reconnect), several security and robustness gaps, and two requirement gaps. **Every finding has been fixed** (sections 3 and 4). What remains (section 5) is outside the code: free-tier behaviour, a trial that ends, and two things only you can run.

## 2. Test results
| Suite | Result | What it covers |
|---|---|---|
| `core_tests` | 60 / 60 | data structures, algorithms, stock rules, whole-document undo, the fair lock |
| `auth_tests` | 29 / 29 | PBKDF2 and old hash formats (reference hashes computed independently in Python), sessions, login throttle |
| `db_tests` | 46 / 46 | repository on real MySQL: transactions, triggers (insert, delete, edit), constraint errors, rollback, undo of whole sales and purchases, lost-connection recovery |
| `tests/api_smoke.py` | 138 / 138 | every API route, roles, errors, photo BLOB, users, throttling, session revocation, headers, origin check, undo, reload, hash-set and binary-search endpoints |
| `tests/livesql_coverage.py` | 44 endpoints, 0 problems | what each endpoint sends to the Live SQL panel |
| Clean rebuild | 0 warnings | `cmake` with `-Wall -Wextra` |
| Docker image | builds and passes the API suite on Linux against MySQL 8.0 over TLS | see `10-Deployment.md` |
| Browser | all pages | rendering, dialogs, Live SQL panel (including dimmed helper reads), CSV export, Undo, reload banner, SKU check, role views, no console errors, no Content-Security-Policy violations |
| Data integrity after full runs | 0 violations | stock = ledger sum, totals = lines, no negative stock |

`scripts/test_on_aiven.sh` runs the database, API and Live SQL suites against a throwaway database on Aiven. It was tested end to end against a local MySQL (46/46, 138/138, 44 endpoints, scratch database dropped, password never echoed). **It has not been run against Aiven itself**: it needs your Aiven password.
Not automated: browser UI tests (checked by hand) and load or performance tests.

## 3. First round of findings (all fixed)
| ID | Severity | Finding | Fix |
|---|---|---|---|
| A1 | **High** | **No database reconnect.** After MySQL dropped the app's single connection every request failed permanently, login included, while `/api/health` still said "up". | Reconnect and retry once outside transactions; inside one the error is reported (a lone retry would run outside the transaction). Idle-timeout error 4031 handled. A lost COMMIT re-syncs memory. |
| A2 | Medium | No login throttling (25 wrong guesses all answered). | 5 failures per user name or 30 per address block that key for 60 s (HTTP 429). |
| A3 | Medium | Sessions survived deletion, role change, password reset. | Those events end the user's sessions. |
| A4 | Low–Med | No security headers. | Content-Security-Policy, X-Frame-Options, nosniff, Referrer-Policy, Permissions-Policy on every response; HSTS on HTTPS. |
| A5 | Low | Error handler turned a 413 and others into a false 404. | httplib errors keep their status. |
| A6 | Low | Merged quantities could overflow an `int`. | Quantity capped at 1,000,000 per line. |
| A7 | Low | Memory-served `/sales/queue` had no note in the panel. | Note added. |
| A8 | Medium | Hash set, tree map and binary search unreachable in the running app. | `GET /products/sku-check` (live SKU check in the form) and `GET /products/{id}?via=binary`. |
| A9 | Medium | Syllabus topics not demonstrable (HAVING, RIGHT JOIN, set operations, SAVEPOINT, EXPLAIN, TRUNCATE). | `sql/syllabus_demo.sql` and its captured output `12-Syllabus-Demo-Output.md`. |
| A10 | Low | Documents described the removed Lab pages. | Updated. |

## 4. Second round: the open items, now resolved
| ID | Was | Now |
|---|---|---|
| O1 | Passwords stored as one salted SHA-256 (fast to guess if the database leaked). | **PBKDF2-HMAC-SHA256, 100,000 rounds**, format `pbkdf2-sha256$rounds$salt$hex`. Old hashes still work and are replaced at the user's next login. `load_db.sh` and the seed write PBKDF2 hashes. Verified against hashes computed independently in Python. |
| O2 | Undo reversed stock only; the sale stayed in the history, so reports disagreed with stock. | **Undo removes the whole sale or purchase** (all its lines together): the line-delete triggers put stock back and write UNDO ledger rows, then the document is deleted. Refused if a purchase's stock was already sold. Undo history is deliberately limited to changes made since the server started, so old history can never be undone by accident; the screen says so. |
| O3 | Memory copy went stale after edits made directly in the database. | The dashboard **detects the difference** (database totals against memory) and shows a banner with a **Reload from database** button (`POST /api/system/reload`, admin). |
| O4 | Render redeployed on every push. | `render.yaml` sets **`autoDeploy: false`**; deploy by hand (Render → Manual Deploy). |
| O5 | Free-tier sleep, trial end. | `scripts/warmup.sh` wakes the site and waits until the app **and** the database answer (`/api/health?deep=1`). `scripts/backup_db.sh` saves tables, data, triggers and views for moving to another MySQL. See section 5 for what code cannot change. |
| O6 | `ON DELETE CASCADE` and no triggers for deleting or editing lines (silent drift). | Foreign keys are **RESTRICT**; new triggers `trg_sale_items_ad`, `trg_purchase_items_ad` keep stock, totals and ledger right when a line is deleted; `trg_*_items_bu` refuse in-place edits. Tested. |
| O7 | Mock mode usable on the live site. | `?mock=1` is honoured **only on localhost** (or `file://`). |
| O8 | Live SQL hid the helper reads behind the Before/After boxes. | They are **shown, dimmed and labelled "helper read"**. Only the login query is still hidden (its result row holds the password hash). |
| O9 | Order queue "FIFO" rested on a mutex that makes no ordering promise. | A **fair ticket lock** (`include/fairlock.h`) serves every request strictly in arrival order, tested with eight queued threads. The order queue still processes each sale in turn. |
| O10 | Base image not pinned; no cross-site request protection beyond the cookie. | Image **pinned by digest** (update procedure in the `Dockerfile`). Changes requested from another site are refused (**Origin check**, 403 `CROSS_SITE`), on top of the `SameSite=Lax` cookie. |

## 5. What cannot be fixed by code
- **Free tiers.** Render's free instances sleep after 15 minutes idle (the first request can take a minute or more) and its 750 free hours a month are shared by all your free services. Mitigation: run `scripts/warmup.sh` before a demo and keep the local copy ready.
- **The Aiven trial ends around 6–7 December 2026** (plan `developer-1`, about $5 a month afterwards; Aiven's free MySQL tier is being discontinued). Mitigation: `scripts/backup_db.sh` then restore into any MySQL, or run locally.
- **Aiven-specific runs you must do:** `scripts/test_on_aiven.sh` (full suite on a throwaway Aiven database) and `scripts/load_roles.sh` (accounts and privileges) were not run against Aiven by me.
- **Single instance.** One server, one database connection, one request at a time. Do not scale the Render service above one instance. This is by design and fine at this scale.
- **Login hashing cost** is about 0.1 s per sign-in on a laptop and may be slower on Render's free CPU. Sign-ins are throttled, so this cannot be used to slow the site down much.

## 6. Verified working
**Requirements F1–F13:** F1 login and roles ✔ · F2 product CRUD and search ✔ · F3 sorting ✔ · F4 categories, suppliers, customers ✔ · F5 purchases ✔ · F6 sales with stock check ✔ · F7 FIFO queue ✔ · F8 low-stock priority queue ✔ · F9 six reports ✔ · F10 undo of whole sales and purchases ✔ · F11 Live SQL, Database Model, syllabus script ✔ · F12 photo BLOB ✔ · F13 expiry stored and shown, printable bill ✔.
**Test plan T1–T19:** see `06-Test-Plan.md` for the test that covers each.
**Concurrency:** two simultaneous buyers, one item left: the second buyer's trigger check passed (it read the old stock) and **`CHECK (stock_quantity >= 0)` stopped the sale** (stock 10, one sale, totals consistent). The CHECK is the real safeguard; keep it.
**Security:** all values reach SQL as bound parameters (the few places that assemble SQL text use fixed table and column names); path-traversal attempts return 404; no keys, certificates or `.env` files are in the repository (scan checked against a known string).
**Live SQL:** all data-changing endpoints return START TRANSACTION, the DML and COMMIT; stock changes carry trigger notes and before/after rows; failing writes end in ROLLBACK with the MySQL error; password hashes are masked as `'***'`.
