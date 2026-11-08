# 11 · Audit Report
**Date:** 08 Nov 2026 · **Scope:** backend code, database, frontend, Live SQL, deployment, architecture, documents against requirements · **Method:** read all source, rebuilt from scratch with all compiler warnings on, ran every test suite, and tried to break the system (dropped connections, concurrent buyers, brute force, path traversal, stale sessions) on a local test database. The live site was checked as far as Render's sleeping free tier allowed (see D1).

## 1. Verdict
The system does what the PRD asks, and the core claims hold up under test: stock changes only through triggers inside transactions, memory is updated only after COMMIT, no oversell is possible even with two simultaneous buyers, and every data-changing action shows its SQL in the Live SQL panel. The audit found **one serious defect** (no database reconnect, which could take the live site down for good), **four security and robustness gaps**, and **two requirement gaps** (three data structures and many syllabus topics were not reachable in the running app). All of these are fixed and tested. Remaining open items are design limits and operational risks, listed in section 5.

## 2. Test results after the fixes
| Suite | Result | What it covers |
|---|---|---|
| `build/core_tests` | 50 / 50 | data structures, algorithms, stock rules (no database) |
| `build/db_tests` | 32 / 32 | repository against real MySQL: transactions, triggers, constraint errors, rollback, **lost-connection recovery** |
| `tests/api_smoke.py` | 119 / 119 | every API route, roles, errors, photo BLOB, users, **throttling, session revocation, headers** |
| `tests/livesql_coverage.py` | 43 endpoints, 0 problems | what each endpoint sends to the Live SQL panel |
| Clean rebuild | 0 warnings | `cmake` with `-Wall -Wextra`, from an empty build folder |
| Browser (real server) | all 11 pages | rendering, dialogs, Live SQL panel, CSV export, role views, no console errors, **no Content-Security-Policy violations** |
| Data integrity after a full run | 0 violations | stock = ledger sum, sale and purchase totals = lines, no negative stock, no malformed hashes |

Not covered by any automated test: the browser UI (checked by hand), load or performance, and the full suites against the live Aiven data (deliberately not run: they add and delete records).

## 3. Findings that were fixed
| ID | Severity | Finding (evidence) | Fix |
|---|---|---|---|
| A1 | **High** | **No database reconnect.** After MySQL dropped the app's single connection, every database request failed permanently, login included, while `/api/health` still said "up" so Render would not restart it. Reproduced by killing the connection. Aiven has a weekly maintenance window and closes idle connections. | Reconnect and retry once **outside** transactions; inside a transaction the error is reported (retrying one statement alone would run it outside the transaction). A lost COMMIT triggers a memory re-sync. A test found that MySQL 8.0.24+ reports an idle timeout as error **4031**, not 2006/2013; now handled. 4 new tests. |
| A2 | Medium | **No login throttling.** 25 wrong passwords in a row were all answered normally. | 5 failures per user name or 30 per client address block that key for 60 seconds (HTTP 429). |
| A3 | Medium | **Sessions survived account changes.** A deleted ADMIN user's open session still read `/api/users`. | Sessions end when the user is deleted, their role changes, or their password is reset. |
| A4 | Low–Med | **No security headers** on pages or images. | Content-Security-Policy, X-Frame-Options, X-Content-Type-Options, Referrer-Policy, Permissions-Policy on every response; HSTS on HTTPS deployments. |
| A5 | Low | **Error handler hid real errors:** an oversized upload answered 404 "No such endpoint"; a bodyless POST too. | httplib errors keep their status (413 TOO_LARGE, 411, 405 ...). |
| A6 | Low | Merged line quantities could overflow an `int`. | Quantity capped at 1,000,000 per line. |
| A7 | Low | `GET /sales/queue` was served from memory with no explanatory note in the panel. | Note added. |
| A8 | Medium (requirements) | **Hash set, tree map and binary search were unit-tested but unreachable** in the running app, against the PRD rule that every structure can be shown. | `GET /products/sku-check` (hash set, used live by the Add Product form) and `GET /products/{id}?via=binary` (tree-map order, then binary search). |
| A9 | Medium (requirements) | **Syllabus topics not demonstrable:** HAVING, RIGHT JOIN, UNION/INTERSECT/EXCEPT, SAVEPOINT, EXPLAIN, TRUNCATE; the planned `lab_queries.sql` was never written. | `sql/syllabus_demo.sql` (14 sections, rolled back, 7 intended errors) and its captured output, `12-Syllabus-Demo-Output.md`. |
| A10 | Low | Documents still described the removed Lab pages (PRD F11, TRD folder tree, syllabus mapping, README). | Updated to match the build. |

## 4. What was verified working
**Requirements (PRD) F1–F13:** F1 login/roles ✔ · F2 product CRUD and search ✔ · F3 sorting ✔ · F4 categories, suppliers, customers ✔ · F5 purchases ✔ · F6 sales with stock check ✔ · F7 FIFO queue ✔ (see O9) · F8 low-stock priority queue ✔ · F9 six reports ✔ · F10 undo ✔ (see O2) · F11 Live SQL, Database Model, syllabus script ✔ (re-scoped, see `09-Frontend-Plan.md`) · F12 photo BLOB ✔ · F13 expiry stored and shown, invoice as printable bill ✔.

**Test plan T1–T19:** T1–T15, T17, T19 pass (API suite and browser). T16 (Lab queries) no longer applies; replaced by the syllabus script. T18 (same flows on Aiven) is partial: the scripts load, the app boots and reads the 60 products over verified TLS, but the full suite was not run against live data.

**Architecture:** layering is clean (routes → repository → database wrapper; the data-structure core has no database dependency). MySQL is the source of truth; memory changes only after COMMIT. All values reach SQL as bound parameters: the four places that assemble SQL text use only fixed table and column names and fixed fragments (checked by reading every call). Path traversal attempts (`..`, `%2e%2e`, encoded slashes) all returned 404. The repository contains no keys or certificates (scanned; the scan was checked against a known string).

**Database:** constraints, triggers and views behave as documented. Concurrency test: two buyers, one item left; the second buyer's BEFORE trigger passed (it read the old stock) and **`CHECK (stock_quantity >= 0)` stopped the sale**: stock 10, one sale, totals consistent. The CHECK, not the trigger, is the real safeguard; do not remove it.

**Live SQL coverage (43 endpoints):** all 22 data-changing endpoints return START TRANSACTION, the DML, and COMMIT; stock changes carry trigger notes and before/after rows; the four failing writes tested (oversell 1644, duplicate SKU 1062, in-use delete 1451, bad reference 1452) end in ROLLBACK with the MySQL error attached. Password hashes are masked as `'***'`.

## 5. Open items (not fixed), by importance
| ID | Severity | Item | Recommendation |
|---|---|---|---|
| O1 | Medium | Passwords are stored as a salted **single SHA-256**, a fast hash. If the database leaked, offline guessing would be fast. | Move to PBKDF2 (OpenSSL `PKCS5_PBKDF2_HMAC`) or Argon2, upgrading each hash at the user's next login. Needs a new stored format and a change to `load_db.sh`. Not done now because it touches the seed and load scripts. |
| O2 | Medium | **Undo reverses stock and adds a ledger row only.** The sale or purchase stays in history: after selling 2 and undoing, stock was back at 110 but revenue still counted the sale (₹82,981). The undo stack is also in memory and is lost on restart. | Rename the button "Reverse stock", or make undo also remove the sale or purchase inside its transaction. |
| O3 | Medium (ops) | The server keeps a **memory copy of the products** loaded at start. Direct database changes (reloading with the scripts, editing in the Aiven console) are invisible until the service restarts. Run only **one instance**. | After any manual database change, restart the Render service. |
| O4 | Medium (ops) | Render **auto-deploys every push to `main`**. A push during the viva restarts the app (cold start, undo history lost). | Turn auto-deploy off in Render before the viva. |
| O5 | Medium (ops) | Free tiers sleep. Render's 750 free hours a month are **shared by your four free services**. The Aiven trial credits end around 6–7 December 2026 (plan `developer-1`, about $5 a month afterwards; Aiven's free MySQL tier is being discontinued). | Follow the wake-up checklist in `10-Deployment.md`; keep the local copy as backup. |
| O6 | Low | `ON DELETE CASCADE` on the item tables: deleting a `Sales` row in SQL silently drops its lines without restoring stock or ledger; no trigger handles UPDATE or DELETE of item rows. The app offers no such action. | Use RESTRICT, or add delete/update triggers. |
| O7 | Low | Mock mode (`?mock=1`) also works on the live site. It only shows sample data and cannot reach real data, but a crafted link could mislead a viewer. | Disable it unless the host is `localhost`. |
| O8 | Low | Live SQL omits the helper SELECTs that build the before/after boxes (two per line item) and the login query (its result row holds the hash). It is complete for every statement that changes data. | Say so in the demo, or add a "show helper reads" switch. |
| O9 | Info | Requests run one at a time under a global lock (one database connection), so the order queue never holds more than one pending order; processing order still equals arrival order. Throughput is limited to one request at a time. | Fine for this scale; a worker thread would make the queue visible. |
| O10 | Info | Base image `ubuntu:24.04` and apt packages are not pinned by digest or version. CSRF protection relies on the `SameSite=Lax` cookie (no token), which is adequate for a JSON API. | Pin if reproducible builds matter. |

## 6. Operational notes
- **D1: live site availability.** During this audit the live site did not answer for over five minutes after being idle for hours, while the other two free services on the account also did not answer within 20 seconds. Render's event log shows no failure or suspension, and Aiven was running. Free instances sleep after 15 minutes idle and Render says they wake in about a minute; this wake-up took far longer. Treat the first request after idle as slow and **warm the site up well before a demo**.
- **Aiven password:** it was exposed once during setup; Aiven's record shows it was reset at 15:47 UTC on 07 Nov 2026, before the Render deploy, so the exposed value no longer works.
- **Demo passwords** (in `sql/seed.sql` and the tests) are for local databases only; loading a remote database refuses to run without your own passwords.
