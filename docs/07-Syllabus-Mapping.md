# 07 · Syllabus and Viva Mapping
"Demo script" = `sql/syllabus_demo.sql`, run with `scripts/run_syllabus_demo.sh`; captured output in [12-Syllabus-Demo-Output.md](12-Syllabus-Demo-Output.md). "Live SQL" = the panel in the running app.

| Topic | Where to point to it | Type |
|---|---|---|
| DDL (CREATE / ALTER / DROP / TRUNCATE) | `sql/schema.sql` (the photo column is added with ALTER), demo script §1 (TRUNCATE on a temporary table) | Practical |
| DML (INSERT / UPDATE / DELETE) | product, category, customer, supplier, user screens with Live SQL; demo script §2 | Practical |
| Basic query, WHERE, ORDER BY, GROUP BY, HAVING | Reports page (GROUP BY, ORDER BY); demo script §3 (HAVING) | Practical |
| Joins (INNER / LEFT / RIGHT) | Reports page (INNER, LEFT); demo script §4 (all three) | Practical |
| UNION / INTERSECT / EXCEPT | demo script §5 (not used by the app screens) | Practical |
| Nested queries, aggregates, NULL | Reports "never sold" (NOT IN subquery), dashboard totals (COALESCE); demo script §6–§7 | Practical |
| Constraints | Add a duplicate SKU, a negative price, or delete a category that is in use: MySQL's error appears in the dialog and in Live SQL; demo script §8 | Practical |
| Triggers, active database | Every sale and purchase: Live SQL shows the trigger notes and before/after stock; forced oversell shows error 1644 and ROLLBACK; demo script §9 | Practical |
| Views | Sales history uses `v_sales_summary`, the low-stock report uses `v_low_stock`; demo script §10 | Practical |
| TCL | Every write in Live SQL: START TRANSACTION, COMMIT or ROLLBACK; demo script §11 (SAVEPOINT) | Practical |
| DCL | `sql/roles.sql`, `scripts/load_roles.sh`; demo script §14 (not exercised by the web app; **untested on Aiven**) | Practical |
| Indexes, EXPLAIN | `sql/schema.sql`; demo script §12 (index range scan, covering lookup, full table scan) | Practical |
| BLOB | Product photo upload and display | Practical |
| ER, relational model, FDs, normal forms, algebra/calculus | DBMS report, Database Model page (ER diagram, schema, columns and keys from `information_schema`) | Theory |
| NoSQL / MongoDB | DBMS report | Theory |
| Recovery, serializability, timestamp/validation/multiple granularity, ISAM, B+ tree | DBMS report | Theory |

## ADS (data structures and algorithms), and where the running app uses each
| Structure / algorithm | Where in the app |
|---|---|
| ArrayList (`vector`) | the product list behind `GET /api/products` |
| HashMap | `GET /api/products/{id}` (`?via=hash`) |
| HashSet | `GET /api/products/sku-check`, used live by the Add Product form |
| TreeMap + binary search | `GET /api/products/{id}?via=binary` (id-ordered view, then binary search) |
| Linear search | product search by name and category filter |
| Merge sort | product sort (price, stock, name), also from the table headers |
| Queue (FIFO) | every sale goes through the order queue; Order queue card on the Sales page |
| Priority queue | restock alerts, lowest stock ratio first (dashboard) |
| Stack | Undo last (dashboard) |
