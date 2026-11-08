# 13 · Viva and Demo Guide

## Numbers to remember
10 tables · 7 triggers · 3 views · 6 categories, 5 suppliers, 20 customers, 60 products, 25 seeded sales · 11 web pages · 40+ API routes · tests: core 60, auth 29, database 46, API 138, Live SQL 44 endpoints · MySQL on Aiven (Singapore), app on Render (Singapore), C++17 server (cpp-httplib + MySQL C API).

## Five-minute demo script
Before you start: `scripts/warmup.sh https://inventory-8fp8.onrender.com` (wakes the free instance and the database). Have `http://localhost:8080` ready as a fallback.
1. **Log in as admin.** Say: roles are checked on the server for every request; passwords are stored as PBKDF2 hashes; failed sign-ins are throttled.
2. **Dashboard.** Stat cards; *Restock alerts* come from a **priority queue** (lowest stock ratio first); *Recent transactions* come from the ledger. Switch **Live SQL** on in the top bar and leave it on.
3. **Products.** Search "pen" (linear search), click the Price header (merge sort on the server), open **Add product**, type an existing SKU and leave the field: "already in use" appears at once (**hash set**, no SQL, the panel says so).
4. **Sale.** Sales page → pick a product → **Save sale**. Point at the panel: START TRANSACTION → INSERT Sales → INSERT Sale_Items with the two **trigger notes** (BEFORE: stock check, AFTER: stock, total, ledger) and the **Before/After** boxes → COMMIT. Dimmed entries are helper reads that build those boxes.
5. **Oversell.** Tick the admin demo box, enter a huge quantity, save: **error 1644**, **ROLLBACK**, stock unchanged. Say: atomicity.
6. **Undo.** Dashboard → **Undo last**: the whole sale disappears, stock comes back, the ledger gets an UNDO row (the delete trigger did it); show the DELETE statements in the panel. This is the **stack**.
7. **Constraint.** Add a product with a negative price or delete a category in use: MySQL's error appears (CHECK 3819, foreign key 1451).
8. **Reports and Database Model.** Reports run fixed SQL (joins, GROUP BY, a NOT IN subquery, a view); Database Model shows the ER diagram and the tables read from `information_schema`.
9. **Close with the syllabus script:** `scripts/run_syllabus_demo.sh ims` (HAVING, RIGHT JOIN, INTERSECT/EXCEPT, SAVEPOINT, EXPLAIN, GRANT) or open `docs/12-Syllabus-Demo-Output.md`.

## What happens when I click Sell
1. The browser sends `POST /api/sales` with the cookie. The server checks the **Origin**, the **session** and the **role**, then takes its place in the **fair ticket lock** (requests are served in arrival order).
2. The order joins the **FIFO queue** and is taken out again (`OrderQueue`).
3. `recordSale` runs one MySQL **transaction**: `START TRANSACTION`, `INSERT INTO Sales`, then for each product: read price and stock, `INSERT INTO Sale_Items`.
4. On that insert the **BEFORE trigger** refuses it (error 1644) if the quantity exceeds stock; otherwise the **AFTER trigger** lowers the stock, adds the line to the sale total, and writes a ledger row.
5. `COMMIT`. Only now is the **in-memory** copy updated (and the line pushed on the **undo stack**), so memory never holds data MySQL rejected.
6. The reply carries the result and the whole `sql_log`, which the **Live SQL panel** draws.
If anything fails: `ROLLBACK`, memory untouched, the error and the ROLLBACK are in the panel.

## Data structures and algorithms
| Structure | Where in the app | Complexity | Why this one |
|---|---|---|---|
| `vector` (array list) | product list, sorting, display | access O(1), append O(1) amortised | contiguous, simple to sort and page |
| `unordered_map` (hash map) | product by id | O(1) average | fastest lookup by key |
| `unordered_set` (hash set) | SKU already taken? | O(1) average | only membership matters |
| `map` (tree map) | products in id order; feeds binary search | O(log n) | keeps keys sorted |
| `queue` | every sale, first in first out | O(1) | arrival order |
| `priority_queue` | restock alerts, lowest stock ratio first | O(log n) | always the most urgent on top |
| `stack` | Undo (newest sale or purchase first) | O(1) | last in, first out |
| Merge sort (hand-written) | sort by price, stock, name | O(n log n), O(n) space, **stable** | predictable, keeps equal items in order |
| Binary search (hand-written) | `GET /products/{id}?via=binary` | O(log n), **needs sorted input** | halves the range each step |
| Linear search (hand-written) | search by name, filter by category | O(n) | no order needed, substring match |
Merge sort in one breath: split the list in halves until single items, then merge pairs back by always taking the smaller front item; taking from the right only when it is strictly smaller keeps the sort stable.

## DBMS topics (full table in `07-Syllabus-Mapping.md`)
**ACID in this project:** Atomicity = a failed sale rolls back everything (demo step 5). Consistency = constraints and triggers keep stock, totals and ledger in step (the consistency queries return 0 violations). Isolation = two buyers at once: the CHECK stops the second (tested). Durability = COMMIT, on Aiven with backups.
**Normal form:** the tables are in BCNF; `total_amount` columns are deliberately derived and kept by triggers.

## Likely questions, short answers
- **Why keep products in memory when MySQL has them?** For the data-structure work and speed. MySQL stays the source of truth: writes go to MySQL first and memory changes only after COMMIT; a dashboard banner and a reload button cover edits made outside the app.
- **What if two users sell the last item together?** The BEFORE trigger reads the old stock for both, but `CHECK (stock_quantity >= 0)` rejects the second update; tested with two real sessions.
- **Why triggers and not code?** The database enforces the rule for every client, including a person typing SQL. The app cannot forget it.
- **Why prepared statements?** Values travel separately from the SQL text, so input can never become SQL (injection text is stored as plain text; tested).
- **How does Undo work?** The stack holds each line with its sale or purchase id. Undo deletes that document's lines; the delete triggers put the stock back and write UNDO rows; then the document is deleted. Reports and stock agree. It only covers changes since the server started, so old history is safe.
- **Why PBKDF2 and a salt?** The salt makes equal passwords differ; 100,000 rounds make each guess slow. Old hashes are upgraded at the next login.
- **What if the database connection drops?** The server reconnects and retries once outside a transaction; inside a transaction it reports the error (a lone retry would run outside the transaction).
- **How is FIFO guaranteed?** A ticket lock serves requests in arrival order, and each sale passes through the queue structure.
- **Why the MySQL C API?** Connector/C++ speaks the X protocol on another port, which Aiven does not offer.
- **What does the Live SQL panel show?** The server records every statement of the request and returns it; the panel draws the SQL, results, trigger notes, before/after rows and errors. Only the login query is hidden, because its result holds a password hash.
- **What are the limits?** One server instance with one database connection and one request at a time; free tiers sleep; the Aiven trial ends around 6–7 December 2026 (`scripts/backup_db.sh` saves everything).

## If something goes wrong on the day
- **Site slow or silent:** free instances sleep; run `scripts/warmup.sh <url>` and wait for READY (up to a few minutes).
- **Dashboard banner "database was changed outside this app":** click **Reload from database**.
- **Aiven unreachable:** run locally: `scripts/load_db.sh ims` (or `scripts/migrate_db.sh ims` for an older copy), `build/ims`, open `http://localhost:8080` (demo logins are in `sql/seed.sql`, for local databases only).
- **Do not push to GitHub during the demo.** Auto-deploy is off, but a manual deploy restarts the site and clears the undo history.
