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
