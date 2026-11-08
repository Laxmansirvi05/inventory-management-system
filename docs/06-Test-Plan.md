# 06 · Test Plan
| ID | Area | Steps | Expected |
|---|---|---|---|
| T1 | Login | Valid admin / employee / wrong password | Role pages open / error |
| T2 | Role check | Employee calls DELETE product | 403 |
| T3 | Add product | Valid data | Row added, SQL log shows INSERT |
| T4 | Duplicate SKU | Reuse SKU | MySQL 1062 shown; hash set also blocks |
| T5 | Invalid price | price = -5 | CHECK error 3819 |
| T6 | Search | By name / category / ID | Correct rows (linear / binary / hash) |
| T7 | Sort | Price, quantity, name, asc/desc | Correct order (merge sort) |
| T8 | Purchase | 2 items | Stock up, ledger rows, total correct |
| T9 | Sale OK | 1–2 items | Stock down, total correct, COMMIT shown |
| T10 | Sale oversell | qty > stock | Error, ROLLBACK, stock unchanged |
| T11 | FIFO | 3 orders in sequence | Processed in arrival order |
| T12 | Low stock | Reduce stock below minimum | Item appears first in urgent list |
| T13 | Undo | Undo last transaction | Stock restored, ledger entry added |
| T14 | Delete referenced | Delete category with products | FK 1451 shown |
| T15 | Reports | Each report | Matches manual SQL in Workbench |
| T16 | Lab queries | Run each whitelisted query | Output matches DBMS report |
| T17 | BLOB | Upload then view photo | Same image returned |
| T18 | Aiven | Same flows against Aiven | Same results |
| T19 | Security | Try SQL injection text in search | Treated as plain text |

## Status (08 Nov 2026): which test covers each case
| ID | Covered by | Result |
|---|---|---|
| T1 Login | `api_smoke` [T1], `auth_tests` | pass |
| T2 Role check | `api_smoke` [T2…] (employee gets 403 on delete, users, model, undo, reload) | pass |
| T3 Add product, T4 duplicate SKU, T5 invalid price | `api_smoke`, `db_tests`, browser | pass (T4: MySQL 1062 shown; the hash set also answers `sku-check`) |
| T6 Search, T7 Sort | `api_smoke`, `core_tests` | pass |
| T8 Purchase, T9 Sale OK, T10 Oversell | `api_smoke`, `db_tests`, browser (Live SQL shows ROLLBACK and error 1644) | pass; also tested with two simultaneous buyers |
| T11 FIFO | `core_tests` (queue, fair lock), `api_smoke` (queue card order) | pass |
| T12 Low stock | `api_smoke`, `core_tests` | pass |
| T13 Undo | `api_smoke`, `db_tests`, browser (whole sale or purchase removed, stock back, ledger UNDO row) | pass |
| T14 Delete referenced | `api_smoke`, `db_tests`, `syllabus_demo.sql` §8 | pass |
| T15 Reports | `api_smoke`, `livesql_coverage` (each report runs the SQL shown in the panel) | pass |
| T16 Lab queries | no longer applicable; replaced by `sql/syllabus_demo.sql` (output in `12-Syllabus-Demo-Output.md`) | replaced |
| T17 BLOB | `api_smoke` (upload, same bytes back, size and type limits) | pass |
| T18 Aiven | load verified, live app reads the 60 products over verified TLS; full suites via `scripts/test_on_aiven.sh` | tool tested locally; **run it once on Aiven** |
| T19 Security | `api_smoke` (injection text, throttle, sessions, headers, origin, path traversal checked separately) | pass |
