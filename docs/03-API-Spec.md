# 03 · API Specification
Base URL `/api` · JSON · session cookie after login · **every write response includes `sql_log`** (list of SQL statements run, trigger notes, before/after rows) for the live SQL panel.

| Method | Path | Role | Purpose | Concept shown |
|---|---|---|---|---|
| POST | /auth/login | any | Login → role | hashing, sessions |
| POST | /auth/logout | any | Logout | |
| GET | /products?search=&category=&sort=&order= | any | List/search/sort | vector, linear search, merge sort |
| GET | /products/{id} | any | One product | hash map / binary search |
| POST | /products | admin | Add product | INSERT, constraints, hash set (SKU) |
| PUT | /products/{id} | admin | Update product | UPDATE |
| DELETE | /products/{id} | admin | Delete product | DELETE, FK RESTRICT |
| POST | /products/{id}/photo | admin | Upload image | BLOB |
| GET | /products/{id}/photo | any | Show image | BLOB retrieval |
| GET | /categories, /suppliers, /customers | any | Lists | SELECT |
| POST/PUT/DELETE | /categories, /suppliers, /customers | admin | Manage | DML |
| POST | /purchases | any | Stock in (multi-item) | transaction, trigger |
| POST | /sales | any | Stock out (multi-item) | queue, transaction, BEFORE/AFTER triggers, ROLLBACK |
| POST | /undo | admin | Undo last stock transaction | stack |
| GET | /alerts/low-stock | any | Restock list, urgent first | priority queue, view |
| GET | /reports/{name} | any | Named report | joins, aggregates, nested queries |
| GET | /auth/me | any | Current user | sessions |
| GET | /sales, /sales/queue | any | Sales history (view) / FIFO order queue card | view, queue |
| GET | /sales/{id}/bill | any | Bill data for the print page | joins |
| GET | /purchases | any | Purchase history | joins, GROUP BY |
| GET | /dashboard, /transactions/recent | any | Dashboard cards, last 10 ledger rows | aggregates |
| GET | /reports | any | List of report names | |
| GET/POST/PUT/DELETE | /users | admin | User management (password hash never returned or shown) | hashing |
| GET | /model | admin | Tables, columns, keys from `information_schema` | catalog |

The earlier `/lab/*` endpoints were dropped when the frontend scope changed (see `09-Frontend-Plan.md` section 1).

**As built (Phase 5):** every response is `{"ok", "data", "error":{"code","mysql_errno","message"}, "sql_log"}` (the shape in `09`, section 7).
Error codes: `UNAUTHENTICATED` 401, `FORBIDDEN` 403, `NOT_FOUND` 404, `DUPLICATE` 409 (1062), `IN_USE` 409 (1451), `INSUFFICIENT_STOCK` 409 (1644), `CONSTRAINT` 400 (3819), `INVALID_REFERENCE` 400 (1452), `BAD_REQUEST` 400.
`GET /products` and `/alerts/low-stock` are served from the in-memory structures and therefore carry an empty `sql_log`.
Query parameters of `GET /products`: `search` (id or name), `category` (id), `sort` (`price`, `qty`, `name`), `order` (`asc`, `desc`).
Photos: `POST /products/{id}/photo` takes the raw image bytes as the body (PNG, JPEG, GIF or WebP, at most 1 MB); `GET` returns the image.
Employees may record sales and purchases and add/edit customers; everything else that writes is admin-only.

**Example – POST /api/sales**
```json
{ "customer_id": 3, "items": [ {"product_id": 6, "quantity": 3} ] }
```
```json
{ "ok": true, "sale_id": 14, "total": 1950.00,
  "sql_log": [
   {"sql":"START TRANSACTION"},
   {"sql":"INSERT INTO Sales (customer_id) VALUES (3)"},
   {"sql":"INSERT INTO Sale_Items (...) VALUES (...)","trigger":"trg_sale_items_bi passed; trg_sale_items_ai: stock 38→35, ledger row added"},
   {"sql":"COMMIT"} ] }
```
Error example: `{"ok":false,"error":"Insufficient stock for product 13","mysql_errno":1644,"sql_log":[...,{"sql":"ROLLBACK"}]}`
