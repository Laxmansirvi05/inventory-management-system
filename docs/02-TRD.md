# 02 · Technical Requirements / Design Document (TRD)

## 1. Architecture
```mermaid
flowchart LR
  B[Browser<br/>HTML + CSS + JS] -- HTTP / JSON --> S[C++ server<br/>cpp-httplib]
  S --> I[Inventory core<br/>STL data structures<br/>+ algorithms]
  S --> D[Database layer<br/>MySQL C API · libmysqlclient]
  D -- SSL --> M[(MySQL 8<br/>local or Aiven)]
  I <-- load at start / sync on change --> D
```
- **Frontend:** static files served by the C++ server (`/public`).
- **Backend:** one C++ executable, REST endpoints returning JSON.
- **Database:** MySQL is the permanent source of truth. At start-up products are loaded into memory structures; every change is written to MySQL **inside a transaction**, then the in-memory copy is updated.

## 2. Technology
| Part | Choice |
|---|---|
| Language | C++17 |
| Web server | cpp-httplib (single header) |
| JSON | nlohmann/json (single header) |
| DB access | MySQL C API `libmysqlclient` (prepared statements, SSL, port 3306). Chosen over Connector/C++ because Homebrew's Connector/C++ only offers the X protocol (port 33060), which Aiven does not expose |
| Build | CMake |
| Container | Docker (for Render) |
| Frontend | HTML, CSS, JavaScript (fetch API) |
| Hosting | Render (app), Aiven MySQL (database), GitHub (code) |

## 3. Folder structure
```
ims/
  src/        main.cpp  server.cpp  inventory.cpp  sorting.cpp  database.cpp  auth.cpp  sqllog.cpp
  include/    product.h inventory.h sorting.h database.h auth.h sqllog.h
  public/     index.html  login.html  products.html  sales.html  reports.html  lab.html  css/  js/
  sql/        schema.sql  triggers.sql  seed.sql  lab_queries.sql
  docs/       (these documents)
  Dockerfile  CMakeLists.txt  README.md
```

## 4. Data structures (ADS)
| Structure | C++ type | Used for | Complexity |
|---|---|---|---|
| ArrayList | `std::vector<Product>` | Product list, display, sorting | access O(1), append O(1) amortised |
| HashMap | `std::unordered_map<int,Product*>` | Find product by ID | O(1) average |
| Queue | `std::queue<Order>` | Customer orders processed FIFO | O(1) |
| PriorityQueue | `std::priority_queue<LowStockItem>` | Low-stock products, lowest ratio first | O(log n) |
| Stack | `std::stack<StockTxn>` | Undo last stock transaction | O(1) |
| HashSet | `std::unordered_set<std::string>` | Unique SKU check | O(1) average |
| TreeMap | `std::map<int,Product*>` | Products in ID order | O(log n) |

## 5. Algorithms (ADS)
| Algorithm | Where | Written by |
|---|---|---|
| Linear search | Search by name/category | hand-written |
| Binary search | Search by ID in sorted vector | hand-written |
| Merge sort | Sort by price, quantity, name | hand-written |
| Hashing | ID and SKU lookup | `unordered_map/set` |
| FIFO | Order queue | `std::queue` |
| Priority scheduling | Restock list | `std::priority_queue` with custom comparator |

## 6. Data flow for a write (example: record a sale)
1. Browser `POST /api/sales`. 2. Server checks role. 3. Order enters the FIFO queue, is dequeued and processed.
4. Database layer: `START TRANSACTION` → `INSERT Sales` → `INSERT Sale_Items` (BEFORE trigger checks stock; AFTER trigger reduces stock, updates total, writes ledger) → `COMMIT` (or `ROLLBACK` on error).
5. In-memory vector/map updated, low-stock queue re-evaluated. 6. Response contains the result **and the SQL log** for the live panel.

## 7. SQL scope (syllabus only)
Allowed: DDL (CREATE, ALTER, DROP, TRUNCATE), DML (INSERT, UPDATE, DELETE, SELECT with WHERE/ORDER BY/GROUP BY/HAVING), joins (INNER, LEFT, RIGHT), UNION/INTERSECT/EXCEPT, nested queries, aggregates, NULL handling, constraints (PK, FK, UNIQUE, NOT NULL, CHECK, DEFAULT), triggers, views, TCL (COMMIT, ROLLBACK, SAVEPOINT), DCL (GRANT, REVOKE), CREATE INDEX / SHOW INDEX / EXPLAIN, BLOB.
**Not used:** window functions, CTEs, stored procedures/functions, JSON functions, full-text search, partitioning.

## 8. Schema changes versus the DBMS report
The DBMS report schema has 9 tables. The web app adds:
- `Users(user_id, username, password_hash, role)` for login (10th table).
- `Products.photo MEDIUMBLOB` for product images (added with `ALTER TABLE`, which also demonstrates ALTER).
**Assumption to confirm:** the DBMS report will be updated to include these.

## 9. Security
Parameterised queries only; password hashing with a salt (SHA-256 via OpenSSL, or stronger if available); role checks on every endpoint; Lab query runner is whitelist-only (queries chosen by ID, no free SQL); secrets in environment variables; SSL to Aiven.

## 10. Configuration (environment variables)
`DB_HOST, DB_PORT, DB_USER, DB_PASSWORD, DB_NAME` (database), `DB_SSL_CA` (CA file path) or `DB_SSL_CA_PEM` (CA certificate text), `PORT`, `PUBLIC_DIR`, `SHOP_NAME`, `COOKIE_SECURE`. Sessions are server-side random tokens, so no `SESSION_SECRET` is needed.

## 11. Risks
| Risk | Mitigation |
|---|---|
| Aiven free plan limits / sleep | Develop locally first; wake service before demo; env-var switch back to local |
| Aiven restricts admin rights (GRANT, triggers) | Test script load on day one |
| Render free tier sleeps | Wake before demo |
| `libmysqlclient` build on Windows | Use Docker or prebuilt client library; document in README |
