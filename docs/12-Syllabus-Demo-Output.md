# 12 · Syllabus Demo Output
Captured output of `sql/syllabus_demo.sql` (run with `scripts/run_syllabus_demo.sh`) on **08 Nov 2026**, local MySQL **26.7.0**, against the seed data (60 products, 25 sales).
Every statement is shown, followed by its result. The seven `ERROR` lines are **intended**: they are the constraint and trigger demonstrations (sections 8 and 9). Changes are rolled back or made on a temporary table; row counts, total stock and total prices were identical before and after the run.
`EXPLAIN` shows MySQL's plan format, which differs between versions. Section 14 shows the grants of the accounts created by `sql/roles.sql` for a database called `ims`.
To run it again, including on Aiven: `scripts/run_syllabus_demo.sh <database>` (same `MYSQL_*` variables as `load_db.sh`).

```text
--------------
-- syllabus_demo.sql : one runnable example for every practical topic of the DBMS syllabus, on the seeded database.
--------------

Query OK, 0 rows affected

--------------
-- Everything that changes data runs inside a transaction that is rolled back, or on a TEMPORARY table, so the data
--------------

Query OK, 0 rows affected

--------------
-- is the same after the run. Sections 8 and 9 contain statements that are MEANT to fail: the error is the lesson.
--------------

Query OK, 0 rows affected

--------------
-- Run with:  scripts/run_syllabus_demo.sh [database]      (captured output: docs/12-Syllabus-Demo-Output.md)
--------------

Query OK, 0 rows affected

--------------
-- Needs MySQL 8.0.31 or newer (INTERSECT and EXCEPT).
--------------

Query OK, 0 rows affected

--------------
SELECT '1. DDL: CREATE, ALTER, TRUNCATE, DROP (on a temporary table)' AS topic
--------------

+--------------------------------------------------------------+
| topic                                                        |
+--------------------------------------------------------------+
| 1. DDL: CREATE, ALTER, TRUNCATE, DROP (on a temporary table) |
+--------------------------------------------------------------+
1 row in set

--------------
CREATE TEMPORARY TABLE demo_items (item_id INT PRIMARY KEY, label VARCHAR(30) NOT NULL)
--------------

Query OK, 0 rows affected

--------------
INSERT INTO demo_items VALUES (1, 'one'), (2, 'two')
--------------

Query OK, 2 rows affected
Records: 2  Duplicates: 0  Warnings: 0

--------------
ALTER TABLE demo_items ADD COLUMN note VARCHAR(20) DEFAULT 'n/a'
--------------

Query OK, 2 rows affected
Records: 2  Duplicates: 0  Warnings: 0

--------------
SELECT * FROM demo_items
--------------

+---------+-------+------+
| item_id | label | note |
+---------+-------+------+
|       1 | one   | n/a  |
|       2 | two   | n/a  |
+---------+-------+------+
2 rows in set

--------------
TRUNCATE TABLE demo_items
--------------

Query OK, 0 rows affected

--------------
SELECT COUNT(*) AS rows_after_truncate FROM demo_items
--------------

+---------------------+
| rows_after_truncate |
+---------------------+
|                   0 |
+---------------------+
1 row in set

--------------
DROP TEMPORARY TABLE demo_items
--------------

Query OK, 0 rows affected

--------------
SHOW COLUMNS FROM Products   -- the last column, photo MEDIUMBLOB, was added to the finished table with ALTER TABLE
--------------

+-----------------+---------------+------+-----+---------+----------------+
| Field           | Type          | Null | Key | Default | Extra          |
+-----------------+---------------+------+-----+---------+----------------+
| product_id      | int           | NO   | PRI | NULL    | auto_increment |
| sku             | varchar(20)   | NO   | UNI | NULL    |                |
| product_name    | varchar(80)   | NO   | MUL | NULL    |                |
| category_id     | int           | NO   | MUL | NULL    |                |
| supplier_id     | int           | NO   | MUL | NULL    |                |
| unit_price      | decimal(10,2) | NO   |     | NULL    |                |
| stock_quantity  | int           | NO   |     | 0       |                |
| min_stock_level | int           | NO   |     | 10      |                |
| expiry_date     | date          | YES  |     | NULL    |                |
| description     | varchar(150)  | YES  |     | NULL    |                |
| photo           | mediumblob    | YES  |     | NULL    |                |
+-----------------+---------------+------+-----+---------+----------------+
11 rows in set

--------------
SELECT '2. DML + TCL: INSERT, UPDATE, DELETE inside a transaction that is rolled back' AS topic
--------------

+-------------------------------------------------------------------------------+
| topic                                                                         |
+-------------------------------------------------------------------------------+
| 2. DML + TCL: INSERT, UPDATE, DELETE inside a transaction that is rolled back |
+-------------------------------------------------------------------------------+
1 row in set

--------------
START TRANSACTION
--------------

Query OK, 0 rows affected

--------------
INSERT INTO Categories (category_name, description) VALUES ('Demo category', 'temporary')
--------------

Query OK, 1 row affected

--------------
UPDATE Categories SET description = 'changed' WHERE category_name = 'Demo category'
--------------

Query OK, 1 row affected
Rows matched: 1  Changed: 1  Warnings: 0

--------------
SELECT category_id, category_name, description FROM Categories WHERE category_name = 'Demo category'
--------------

+-------------+---------------+-------------+
| category_id | category_name | description |
+-------------+---------------+-------------+
|           7 | Demo category | changed     |
+-------------+---------------+-------------+
1 row in set

--------------
DELETE FROM Categories WHERE category_name = 'Demo category'
--------------

Query OK, 1 row affected

--------------
SELECT COUNT(*) AS demo_rows_left FROM Categories WHERE category_name = 'Demo category'
--------------

+----------------+
| demo_rows_left |
+----------------+
|              0 |
+----------------+
1 row in set

--------------
ROLLBACK
--------------

Query OK, 0 rows affected

--------------
SELECT '3. Basic query: WHERE, ORDER BY, GROUP BY, HAVING' AS topic
--------------

+---------------------------------------------------+
| topic                                             |
+---------------------------------------------------+
| 3. Basic query: WHERE, ORDER BY, GROUP BY, HAVING |
+---------------------------------------------------+
1 row in set

--------------
SELECT product_name, unit_price, stock_quantity FROM Products
 WHERE unit_price BETWEEN 100 AND 300 AND stock_quantity > 50 ORDER BY unit_price DESC LIMIT 5
--------------

+--------------------+------------+----------------+
| product_name       | unit_price | stock_quantity |
+--------------------+------------+----------------+
| Shampoo 340ml      |     225.00 |            158 |
| Peanut Butter 340g |     210.00 |             57 |
| Deodorant 150ml    |     210.00 |             71 |
| USB Cable Type-C   |     199.00 |             54 |
| Floor Cleaner 1L   |     189.00 |             70 |
+--------------------+------------+----------------+
5 rows in set

--------------
SELECT c.category_name, COUNT(*) AS products, SUM(p.stock_quantity * p.unit_price) AS stock_value
  FROM Products p JOIN Categories c ON c.category_id = p.category_id
 GROUP BY c.category_name
HAVING SUM(p.stock_quantity * p.unit_price) > 50000
 ORDER BY stock_value DESC
--------------

+---------------+----------+-------------+
| category_name | products | stock_value |
+---------------+----------+-------------+
| Electronics   |       10 |   259493.00 |
| Personal Care |       10 |   140675.00 |
| Groceries     |       10 |   110343.00 |
| Household     |       10 |    96713.00 |
| Beverages     |       10 |    81310.00 |
| Stationery    |       10 |    65450.00 |
+---------------+----------+-------------+
6 rows in set

--------------
SELECT '4. Joins: INNER, LEFT, RIGHT' AS topic
--------------

+------------------------------+
| topic                        |
+------------------------------+
| 4. Joins: INNER, LEFT, RIGHT |
+------------------------------+
1 row in set

--------------
SELECT p.product_name, c.category_name, s.supplier_name
  FROM Products p INNER JOIN Categories c ON c.category_id = p.category_id
                  INNER JOIN Suppliers s ON s.supplier_id = p.supplier_id
 ORDER BY p.product_id LIMIT 5
--------------

+----------------------+---------------+--------------------+
| product_name         | category_name | supplier_name      |
+----------------------+---------------+--------------------+
| Ball Pen Blue        | Stationery    | Sri Balaji Traders |
| Ball Pen Black       | Stationery    | Sri Balaji Traders |
| Gel Pen              | Stationery    | Sri Balaji Traders |
| Pencil HB Pack of 10 | Stationery    | Sri Balaji Traders |
| Notebook 200 Pages   | Stationery    | Metro Wholesale    |
+----------------------+---------------+--------------------+
5 rows in set

--------------
-- LEFT JOIN keeps every customer, also those without a sale: here the customers who never bought
--------------

Query OK, 0 rows affected

--------------
SELECT c.customer_name, COUNT(s.sale_id) AS sales
  FROM Customers c LEFT JOIN Sales s ON s.customer_id = c.customer_id
 GROUP BY c.customer_id, c.customer_name HAVING COUNT(s.sale_id) = 0 ORDER BY c.customer_name
--------------

+---------------+-------+
| customer_name | sales |
+---------------+-------+
| Ananya Sharma |     0 |
| Harsha Reddy  |     0 |
| Lakshmi Reddy |     0 |
| Nikhil Patel  |     0 |
| Pooja Sharma  |     0 |
| Rohit Das     |     0 |
| Sneha Singh   |     0 |
| Swathi Rao    |     0 |
+---------------+-------+
8 rows in set

--------------
-- RIGHT JOIN keeps every product, also those never sold: the same idea from the other side
--------------

Query OK, 0 rows affected

--------------
SELECT p.product_name, COUNT(si.sale_id) AS times_sold
  FROM Sale_Items si RIGHT JOIN Products p ON p.product_id = si.product_id
 GROUP BY p.product_id, p.product_name HAVING COUNT(si.sale_id) = 0 ORDER BY p.product_name LIMIT 5
--------------

+---------------------+------------+
| product_name        | times_sold |
+---------------------+------------+
| Basmati Rice 1kg    |          0 |
| Biscuits Pack       |          0 |
| Body Lotion 250ml   |          0 |
| Coconut Water 200ml |          0 |
| Cola 600ml          |          0 |
+---------------------+------------+
5 rows in set

--------------
SELECT '5. Set operations: UNION, INTERSECT, EXCEPT' AS topic
--------------

+---------------------------------------------+
| topic                                       |
+---------------------------------------------+
| 5. Set operations: UNION, INTERSECT, EXCEPT |
+---------------------------------------------+
1 row in set

--------------
SELECT city FROM Customers UNION     SELECT city FROM Suppliers ORDER BY city   -- all cities, no duplicates
--------------

+--------------+
| city         |
+--------------+
| Guntur       |
| Hyderabad    |
| Secunderabad |
| Vijayawada   |
| Warangal     |
+--------------+
5 rows in set

--------------
SELECT city FROM Customers INTERSECT SELECT city FROM Suppliers                 -- cities with customers AND suppliers
--------------

+--------------+
| city         |
+--------------+
| Hyderabad    |
| Secunderabad |
| Warangal     |
+--------------+
3 rows in set

--------------
SELECT city FROM Customers EXCEPT    SELECT city FROM Suppliers                 -- customer cities without a supplier
--------------

+------------+
| city       |
+------------+
| Guntur     |
| Vijayawada |
+------------+
2 rows in set

--------------
SELECT '6. Nested queries: correlated subquery, NOT IN, EXISTS' AS topic
--------------

+--------------------------------------------------------+
| topic                                                  |
+--------------------------------------------------------+
| 6. Nested queries: correlated subquery, NOT IN, EXISTS |
+--------------------------------------------------------+
1 row in set

--------------
SELECT product_name, unit_price, category_id FROM Products p
 WHERE unit_price > (SELECT AVG(unit_price) FROM Products WHERE category_id = p.category_id)
 ORDER BY category_id, unit_price DESC LIMIT 6                                  -- dearer than the average of its own category
--------------

+---------------------+------------+-------------+
| product_name        | unit_price | category_id |
+---------------------+------------+-------------+
| A4 Paper Ream       |     320.00 |           1 |
| Geometry Box        |     140.00 |           1 |
| Highlighter Set     |     120.00 |           1 |
| Power Bank 10000mAh |    1199.00 |           2 |
| Keyboard USB        |     699.00 |           2 |
| Mouse Wireless      |     599.00 |           2 |
+---------------------+------------+-------------+
6 rows in set

--------------
SELECT product_name FROM Products WHERE product_id NOT IN (SELECT product_id FROM Sale_Items) ORDER BY product_name LIMIT 5
--------------

+---------------------+
| product_name        |
+---------------------+
| Basmati Rice 1kg    |
| Biscuits Pack       |
| Body Lotion 250ml   |
| Coconut Water 200ml |
| Cola 600ml          |
+---------------------+
5 rows in set

--------------
SELECT customer_name FROM Customers c
 WHERE EXISTS (SELECT 1 FROM Sales s WHERE s.customer_id = c.customer_id AND s.total_amount > 20000)
--------------

+---------------+
| customer_name |
+---------------+
| Divya Das     |
+---------------+
1 row in set

--------------
SELECT '7. Aggregates and NULL handling' AS topic
--------------

+---------------------------------+
| topic                           |
+---------------------------------+
| 7. Aggregates and NULL handling |
+---------------------------------+
1 row in set

--------------
SELECT COUNT(*) AS all_products, COUNT(expiry_date) AS with_expiry, COUNT(*) - COUNT(expiry_date) AS without_expiry,
       MIN(unit_price) AS cheapest, MAX(unit_price) AS dearest, ROUND(AVG(unit_price), 2) AS average_price FROM Products
--------------

+--------------+-------------+----------------+----------+---------+---------------+
| all_products | with_expiry | without_expiry | cheapest | dearest | average_price |
+--------------+-------------+----------------+----------+---------+---------------+
|           60 |          20 |             40 |    10.00 | 1199.00 |        149.85 |
+--------------+-------------+----------------+----------+---------+---------------+
1 row in set

--------------
SELECT product_name, COALESCE(CAST(expiry_date AS CHAR), 'no expiry') AS expiry FROM Products WHERE expiry_date IS NULL LIMIT 3
--------------

+----------------+-----------+
| product_name   | expiry    |
+----------------+-----------+
| Ball Pen Blue  | no expiry |
| Ball Pen Black | no expiry |
| Gel Pen        | no expiry |
+----------------+-----------+
3 rows in set

--------------
SELECT '8. Constraints: each statement below is EXPECTED to fail' AS topic
--------------

+----------------------------------------------------------+
| topic                                                    |
+----------------------------------------------------------+
| 8. Constraints: each statement below is EXPECTED to fail |
+----------------------------------------------------------+
1 row in set

--------------
INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price) VALUES ('SKU0001', 'dup', 1, 1, 5)   -- UNIQUE      -> 1062
--------------

ERROR 1062 (23000) at line 68: Duplicate entry 'SKU0001' for key 'products.sku'
--------------
INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price) VALUES ('X-NEG', 'neg', 1, 1, -5)    -- CHECK       -> 3819
--------------

ERROR 3819 (HY000) at line 69: Check constraint 'chk_price' is violated.
--------------
INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price) VALUES ('X-NULL', NULL, 1, 1, 5)     -- NOT NULL    -> 1048
--------------

ERROR 1048 (23000) at line 70: Column 'product_name' cannot be null
--------------
INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price) VALUES ('X-FK', 'fk', 999, 1, 5)     -- FOREIGN KEY -> 1452
--------------

ERROR 1452 (23000) at line 71: Cannot add or update a child row: a foreign key constraint fails (`ims_test`.`products`, CONSTRAINT `fk_prod_cat` FOREIGN KEY (`category_id`) REFERENCES `categories` (`category_id`) ON DELETE RESTRICT ON UPDATE CASCADE)
--------------
DELETE FROM Categories WHERE category_id = 1                                                                         -- RESTRICT    -> 1451
--------------

ERROR 1451 (23000) at line 72: Cannot delete or update a parent row: a foreign key constraint fails (`ims_test`.`products`, CONSTRAINT `fk_prod_cat` FOREIGN KEY (`category_id`) REFERENCES `categories` (`category_id`) ON DELETE RESTRICT ON UPDATE CASCADE)
--------------
INSERT INTO Customers (customer_name, phone) VALUES ('Bad phone', '123')                                             -- CHECK       -> 3819
--------------

ERROR 3819 (HY000) at line 73: Check constraint 'chk_customer_phone' is violated.
--------------
START TRANSACTION                                                                                                    -- DEFAULT values fill what is not given
--------------

Query OK, 0 rows affected

--------------
INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price) VALUES ('X-DEFAULT', 'defaults', 1, 1, 5)
--------------

Query OK, 1 row affected

--------------
SELECT sku, stock_quantity AS default_stock, min_stock_level AS default_minimum FROM Products WHERE sku = 'X-DEFAULT'
--------------

+-----------+---------------+-----------------+
| sku       | default_stock | default_minimum |
+-----------+---------------+-----------------+
| X-DEFAULT |             0 |              10 |
+-----------+---------------+-----------------+
1 row in set

--------------
ROLLBACK
--------------

Query OK, 0 rows affected

--------------
SELECT '9. Triggers: the sale trigger pair, then an oversell' AS topic
--------------

+------------------------------------------------------+
| topic                                                |
+------------------------------------------------------+
| 9. Triggers: the sale trigger pair, then an oversell |
+------------------------------------------------------+
1 row in set

--------------
SELECT trigger_name, action_timing, event_manipulation, event_object_table
  FROM information_schema.triggers WHERE trigger_schema = DATABASE() ORDER BY trigger_name
--------------

+-----------------------+---------------+--------------------+--------------------+
| TRIGGER_NAME          | ACTION_TIMING | EVENT_MANIPULATION | EVENT_OBJECT_TABLE |
+-----------------------+---------------+--------------------+--------------------+
| trg_purchase_items_ai | AFTER         | INSERT             | Purchase_Items     |
| trg_sale_items_ai     | AFTER         | INSERT             | Sale_Items         |
| trg_sale_items_bi     | BEFORE        | INSERT             | Sale_Items         |
+-----------------------+---------------+--------------------+--------------------+
3 rows in set

--------------
START TRANSACTION
--------------

Query OK, 0 rows affected

--------------
SELECT stock_quantity AS stock_before FROM Products WHERE product_id = 6
--------------

+--------------+
| stock_before |
+--------------+
|          112 |
+--------------+
1 row in set

--------------
INSERT INTO Sales (customer_id) VALUES (1)
--------------

Query OK, 1 row affected

--------------
SET @sale = LAST_INSERT_ID()
--------------

Query OK, 0 rows affected

--------------
INSERT INTO Sale_Items (sale_id, product_id, quantity, unit_price) VALUES (@sale, 6, 2, 35)   -- BEFORE trigger checks, AFTER trigger acts
--------------

Query OK, 1 row affected

--------------
SELECT stock_quantity AS stock_after FROM Products WHERE product_id = 6                         -- reduced by the trigger
--------------

+-------------+
| stock_after |
+-------------+
|         110 |
+-------------+
1 row in set

--------------
SELECT total_amount AS sale_total FROM Sales WHERE sale_id = @sale                              -- filled by the trigger
--------------

+------------+
| sale_total |
+------------+
|      70.00 |
+------------+
1 row in set

--------------
SELECT txn_type, quantity_change, remarks FROM Stock_Transactions ORDER BY txn_id DESC LIMIT 1  -- ledger row written by the trigger
--------------

+----------+-----------------+------------+
| txn_type | quantity_change | remarks    |
+----------+-----------------+------------+
| SALE     |              -2 | Stock sold |
+----------+-----------------+------------+
1 row in set

--------------
INSERT INTO Sales (customer_id) VALUES (1)
--------------

Query OK, 1 row affected

--------------
INSERT INTO Sale_Items (sale_id, product_id, quantity, unit_price) VALUES (LAST_INSERT_ID(), 6, 999999, 35)   -- oversell: ERROR 1644
--------------

ERROR 1644 (45000) at line 91: Insufficient stock for product 6
--------------
ROLLBACK
--------------

Query OK, 0 rows affected

--------------
SELECT stock_quantity AS stock_after_rollback FROM Products WHERE product_id = 6                -- back to the start
--------------

+----------------------+
| stock_after_rollback |
+----------------------+
|                  112 |
+----------------------+
1 row in set

--------------
SELECT '10. Views' AS topic
--------------

+-----------+
| topic     |
+-----------+
| 10. Views |
+-----------+
1 row in set

--------------
SELECT * FROM v_low_stock
--------------

+------------+---------+------------------+----------------+-----------------+-------------+---------------------+
| product_id | sku     | product_name     | stock_quantity | min_stock_level | stock_ratio | supplier_name       |
+------------+---------+------------------+----------------+-----------------+-------------+---------------------+
|         31 | SKU0031 | Mineral Water 1L |             24 |              80 |        0.30 | Value Mart Agencies |
|         22 | SKU0022 | Wheat Flour 1kg  |             20 |              40 |        0.50 | Krishna Supplies    |
|          3 | SKU0003 | Gel Pen          |             32 |              40 |        0.80 | Sri Balaji Traders  |
|         45 | SKU0045 | Hand Wash 250ml  |             20 |              25 |        0.80 | Metro Wholesale     |
|          8 | SKU0008 | Stapler Medium   |             10 |              10 |        1.00 | Sri Balaji Traders  |
|         12 | SKU0012 | Earphones Wired  |             15 |              15 |        1.00 | Deccan Distributors |
+------------+---------+------------------+----------------+-----------------+-------------+---------------------+
6 rows in set

--------------
SELECT * FROM v_sales_summary ORDER BY sale_id DESC LIMIT 3
--------------

+---------+------------+------------------+--------------+------------+------------+
| sale_id | sale_date  | customer_name    | total_amount | item_lines | units_sold |
+---------+------------+------------------+--------------+------------+------------+
|      25 | 2026-06-13 | Aarav Iyer       |     18824.00 |          3 |        176 |
|      24 | 2026-06-08 | Walk-in Customer |     10910.00 |          4 |        459 |
|      23 | 2026-06-03 | Aarav Iyer       |      6342.00 |          2 |        130 |
+---------+------------+------------------+--------------+------------+------------+
3 rows in set

--------------
SELECT '11. TCL: SAVEPOINT and ROLLBACK TO' AS topic
--------------

+------------------------------------+
| topic                              |
+------------------------------------+
| 11. TCL: SAVEPOINT and ROLLBACK TO |
+------------------------------------+
1 row in set

--------------
START TRANSACTION
--------------

Query OK, 0 rows affected

--------------
UPDATE Products SET unit_price = unit_price + 1 WHERE product_id = 1
--------------

Query OK, 1 row affected
Rows matched: 1  Changed: 1  Warnings: 0

--------------
SAVEPOINT after_first_change
--------------

Query OK, 0 rows affected

--------------
UPDATE Products SET unit_price = unit_price + 100 WHERE product_id = 1
--------------

Query OK, 1 row affected
Rows matched: 1  Changed: 1  Warnings: 0

--------------
SELECT unit_price AS price_after_two_updates FROM Products WHERE product_id = 1
--------------

+-------------------------+
| price_after_two_updates |
+-------------------------+
|                  111.00 |
+-------------------------+
1 row in set

--------------
ROLLBACK TO SAVEPOINT after_first_change
--------------

Query OK, 0 rows affected

--------------
SELECT unit_price AS price_after_rollback_to_savepoint FROM Products WHERE product_id = 1
--------------

+-----------------------------------+
| price_after_rollback_to_savepoint |
+-----------------------------------+
|                             11.00 |
+-----------------------------------+
1 row in set

--------------
ROLLBACK
--------------

Query OK, 0 rows affected

--------------
SELECT unit_price AS price_after_full_rollback FROM Products WHERE product_id = 1
--------------

+---------------------------+
| price_after_full_rollback |
+---------------------------+
|                     10.00 |
+---------------------------+
1 row in set

--------------
SELECT '12. Indexes and EXPLAIN' AS topic
--------------

+-------------------------+
| topic                   |
+-------------------------+
| 12. Indexes and EXPLAIN |
+-------------------------+
1 row in set

--------------
SHOW INDEX FROM Products
--------------

+----------+------------+------------------------+--------------+--------------+-----------+-------------+----------+--------+------+------------+---------+---------------+---------+------------+
| Table    | Non_unique | Key_name               | Seq_in_index | Column_name  | Collation | Cardinality | Sub_part | Packed | Null | Index_type | Comment | Index_comment | Visible | Expression |
+----------+------------+------------------------+--------------+--------------+-----------+-------------+----------+--------+------+------------+---------+---------------+---------+------------+
| Products |          0 | PRIMARY                |            1 | product_id   | A         |          60 |     NULL |   NULL |      | BTREE      |         |               | YES     | NULL       |
| Products |          0 | sku                    |            1 | sku          | A         |          60 |     NULL |   NULL |      | BTREE      |         |               | YES     | NULL       |
| Products |          1 | fk_prod_sup            |            1 | supplier_id  | A         |           5 |     NULL |   NULL |      | BTREE      |         |               | YES     | NULL       |
| Products |          1 | idx_products_name      |            1 | product_name | A         |          60 |     NULL |   NULL |      | BTREE      |         |               | YES     | NULL       |
| Products |          1 | idx_products_cat_price |            1 | category_id  | A         |           6 |     NULL |   NULL |      | BTREE      |         |               | YES     | NULL       |
| Products |          1 | idx_products_cat_price |            2 | unit_price   | A         |          59 |     NULL |   NULL |      | BTREE      |         |               | YES     | NULL       |
+----------+------------+------------------------+--------------+--------------+-----------+-------------+----------+--------+------+------------+---------+---------------+---------+------------+
6 rows in set

--------------
EXPLAIN SELECT product_name FROM Products WHERE category_id = 1 AND unit_price < 100   -- can use the composite index (category_id, unit_price)
--------------

+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| EXPLAIN                                                                                                                                                                                                                 |
+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| -> Index range scan on Products using idx_products_cat_price over (category_id = 1 AND unit_price < 100.00), with index condition: ((products.category_id = 1) and (products.unit_price < 100.00))  (cost=3.41 rows=7)
 |
+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
1 row in set

--------------
EXPLAIN SELECT product_name FROM Products WHERE product_name = 'Gel Pen'               -- can use idx_products_name
--------------

+--------------------------------------------------------------------------------------------------------------+
| EXPLAIN                                                                                                      |
+--------------------------------------------------------------------------------------------------------------+
| -> Covering index lookup on Products using idx_products_name (product_name = 'Gel Pen')  (cost=0.35 rows=1)
 |
+--------------------------------------------------------------------------------------------------------------+
1 row in set

--------------
EXPLAIN SELECT product_name FROM Products WHERE description = 'x'                      -- no index on description: full table scan
--------------

+---------------------------------------------------------------------------------------------------------------------+
| EXPLAIN                                                                                                             |
+---------------------------------------------------------------------------------------------------------------------+
| -> Filter: (products.`description` = 'x')  (cost=6.35 rows=6.1)
    -> Table scan on Products  (cost=6.35 rows=61)
 |
+---------------------------------------------------------------------------------------------------------------------+
1 row in set

--------------
SELECT '13. BLOB: stored product photos' AS topic
--------------

+---------------------------------+
| topic                           |
+---------------------------------+
| 13. BLOB: stored product photos |
+---------------------------------+
1 row in set

--------------
SELECT COUNT(*) AS products_with_photo, COALESCE(SUM(LENGTH(photo)), 0) AS photo_bytes FROM Products WHERE photo IS NOT NULL
--------------

+---------------------+-------------+
| products_with_photo | photo_bytes |
+---------------------+-------------+
|                   0 |           0 |
+---------------------+-------------+
1 row in set

--------------
SELECT '14. DCL: accounts created by roles.sql (an error here means roles.sql was not loaded)' AS topic
--------------

+---------------------------------------------------------------------------------------+
| topic                                                                                 |
+---------------------------------------------------------------------------------------+
| 14. DCL: accounts created by roles.sql (an error here means roles.sql was not loaded) |
+---------------------------------------------------------------------------------------+
1 row in set

--------------
SHOW GRANTS FOR 'ims_employee'@'%'
--------------

+------------------------------------------------------------------+
| Grants for ims_employee@%                                        |
+------------------------------------------------------------------+
| GRANT USAGE ON *.* TO `ims_employee`@`%`                         |
| GRANT SELECT ON `ims`.* TO `ims_employee`@`%`                    |
| GRANT INSERT, UPDATE ON `ims`.`customers` TO `ims_employee`@`%`  |
| GRANT UPDATE ON `ims`.`products` TO `ims_employee`@`%`           |
| GRANT INSERT ON `ims`.`purchase_items` TO `ims_employee`@`%`     |
| GRANT INSERT, UPDATE ON `ims`.`purchases` TO `ims_employee`@`%`  |
| GRANT INSERT ON `ims`.`sale_items` TO `ims_employee`@`%`         |
| GRANT INSERT, UPDATE ON `ims`.`sales` TO `ims_employee`@`%`      |
| GRANT INSERT ON `ims`.`stock_transactions` TO `ims_employee`@`%` |
+------------------------------------------------------------------+
9 rows in set

--------------
SHOW GRANTS FOR 'ims_admin'@'%'
--------------

+----------------------------------------------------+
| Grants for ims_admin@%                             |
+----------------------------------------------------+
| GRANT USAGE ON *.* TO `ims_admin`@`%`              |
| GRANT ALL PRIVILEGES ON `ims`.* TO `ims_admin`@`%` |
+----------------------------------------------------+
2 rows in set

Bye
```
