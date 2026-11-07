# 04 · Database Design
Source: DBMS report (Sections 3–4), executed and verified on MySQL 8.0.46. Additions for the web app are marked **NEW**.

## Tables (10)
Categories, Suppliers, Customers, Products (+ **NEW** `photo MEDIUMBLOB`), Purchases, Purchase_Items, Sales, Sale_Items, Stock_Transactions, **NEW** Users.

## ER diagram
![ER diagram](img/er_diagram.png)

## Relational schema
![Schema](img/schema.png)
(The Users table is standalone and not shown in these figures; update them when it is added.)

## Normalization
All report tables are in BCNF (functional dependencies in the DBMS report, Table 3.7). `Sales.total_amount` and `Purchases.total_amount` are deliberate derived columns maintained by triggers.

## Constraints
PRIMARY KEY, FOREIGN KEY (RESTRICT/CASCADE), UNIQUE (sku, phone, email, username), NOT NULL, CHECK (price > 0, stock ≥ 0, txn_type, phone format), DEFAULT.

## Triggers
| Trigger | Event | Action |
|---|---|---|
| trg_purchase_items_ai | AFTER INSERT on Purchase_Items | stock += qty, purchase total += line, ledger row |
| trg_sale_items_bi | BEFORE INSERT on Sale_Items | SIGNAL error if qty > stock |
| trg_sale_items_ai | AFTER INSERT on Sale_Items | stock -= qty, sale total += line, ledger row |

## Views
`v_low_stock`, `v_sales_summary`, `v_product_catalogue`.

## Indexes
Primary key (clustered), UNIQUE sku, foreign-key indexes, `idx_products_name`, composite `(category_id, unit_price)`; EXPLAIN demo in the Lab.

## Roles (DCL)
`ims_admin` (all privileges on the database), `ims_employee` (SELECT on views and tables, INSERT on Sales/Sale_Items/Purchases/Purchase_Items; no DELETE). The web app login role maps to which database account the Lab demo uses.
**To verify on Aiven:** whether CREATE USER / GRANT work for the admin account.

## Scripts
`sql/schema.sql`, `sql/triggers.sql`, `sql/seed.sql`, `sql/lab_queries.sql` (reuse the verified blocks from the DBMS report appendix).
