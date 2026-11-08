-- syllabus_demo.sql : one runnable example for every practical topic of the DBMS syllabus, on the seeded database.
-- Everything that changes data runs inside a transaction that is rolled back, or on a TEMPORARY table, so the data
-- is the same after the run. Sections 8 and 9 contain statements that are MEANT to fail: the error is the lesson.
-- Run with:  scripts/run_syllabus_demo.sh [database]      (captured output: docs/12-Syllabus-Demo-Output.md)
-- Needs MySQL 8.0.31 or newer (INTERSECT and EXCEPT).

SELECT '1. DDL: CREATE, ALTER, TRUNCATE, DROP (on a temporary table)' AS topic;
CREATE TEMPORARY TABLE demo_items (item_id INT PRIMARY KEY, label VARCHAR(30) NOT NULL);
INSERT INTO demo_items VALUES (1, 'one'), (2, 'two');
ALTER TABLE demo_items ADD COLUMN note VARCHAR(20) DEFAULT 'n/a';
SELECT * FROM demo_items;
TRUNCATE TABLE demo_items;
SELECT COUNT(*) AS rows_after_truncate FROM demo_items;
DROP TEMPORARY TABLE demo_items;
SHOW COLUMNS FROM Products;   -- the last column, photo MEDIUMBLOB, was added to the finished table with ALTER TABLE

SELECT '2. DML + TCL: INSERT, UPDATE, DELETE inside a transaction that is rolled back' AS topic;
START TRANSACTION;
INSERT INTO Categories (category_name, description) VALUES ('Demo category', 'temporary');
UPDATE Categories SET description = 'changed' WHERE category_name = 'Demo category';
SELECT category_id, category_name, description FROM Categories WHERE category_name = 'Demo category';
DELETE FROM Categories WHERE category_name = 'Demo category';
SELECT COUNT(*) AS demo_rows_left FROM Categories WHERE category_name = 'Demo category';
ROLLBACK;

SELECT '3. Basic query: WHERE, ORDER BY, GROUP BY, HAVING' AS topic;
SELECT product_name, unit_price, stock_quantity FROM Products
 WHERE unit_price BETWEEN 100 AND 300 AND stock_quantity > 50 ORDER BY unit_price DESC LIMIT 5;
SELECT c.category_name, COUNT(*) AS products, SUM(p.stock_quantity * p.unit_price) AS stock_value
  FROM Products p JOIN Categories c ON c.category_id = p.category_id
 GROUP BY c.category_name
HAVING SUM(p.stock_quantity * p.unit_price) > 50000
 ORDER BY stock_value DESC;

SELECT '4. Joins: INNER, LEFT, RIGHT' AS topic;
SELECT p.product_name, c.category_name, s.supplier_name
  FROM Products p INNER JOIN Categories c ON c.category_id = p.category_id
                  INNER JOIN Suppliers s ON s.supplier_id = p.supplier_id
 ORDER BY p.product_id LIMIT 5;
-- LEFT JOIN keeps every customer, also those without a sale: here the customers who never bought
SELECT c.customer_name, COUNT(s.sale_id) AS sales
  FROM Customers c LEFT JOIN Sales s ON s.customer_id = c.customer_id
 GROUP BY c.customer_id, c.customer_name HAVING COUNT(s.sale_id) = 0 ORDER BY c.customer_name;
-- RIGHT JOIN keeps every product, also those never sold: the same idea from the other side
SELECT p.product_name, COUNT(si.sale_id) AS times_sold
  FROM Sale_Items si RIGHT JOIN Products p ON p.product_id = si.product_id
 GROUP BY p.product_id, p.product_name HAVING COUNT(si.sale_id) = 0 ORDER BY p.product_name LIMIT 5;

SELECT '5. Set operations: UNION, INTERSECT, EXCEPT' AS topic;
SELECT city FROM Customers UNION     SELECT city FROM Suppliers ORDER BY city;   -- all cities, no duplicates
SELECT city FROM Customers INTERSECT SELECT city FROM Suppliers;                 -- cities with customers AND suppliers
SELECT city FROM Customers EXCEPT    SELECT city FROM Suppliers;                 -- customer cities without a supplier

SELECT '6. Nested queries: correlated subquery, NOT IN, EXISTS' AS topic;
SELECT product_name, unit_price, category_id FROM Products p
 WHERE unit_price > (SELECT AVG(unit_price) FROM Products WHERE category_id = p.category_id)
 ORDER BY category_id, unit_price DESC LIMIT 6;                                  -- dearer than the average of its own category
SELECT product_name FROM Products WHERE product_id NOT IN (SELECT product_id FROM Sale_Items) ORDER BY product_name LIMIT 5;
SELECT customer_name FROM Customers c
 WHERE EXISTS (SELECT 1 FROM Sales s WHERE s.customer_id = c.customer_id AND s.total_amount > 20000);

SELECT '7. Aggregates and NULL handling' AS topic;
SELECT COUNT(*) AS all_products, COUNT(expiry_date) AS with_expiry, COUNT(*) - COUNT(expiry_date) AS without_expiry,
       MIN(unit_price) AS cheapest, MAX(unit_price) AS dearest, ROUND(AVG(unit_price), 2) AS average_price FROM Products;
SELECT product_name, COALESCE(CAST(expiry_date AS CHAR), 'no expiry') AS expiry FROM Products WHERE expiry_date IS NULL LIMIT 3;

SELECT '8. Constraints: each statement below is EXPECTED to fail' AS topic;
INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price) VALUES ('SKU0001', 'dup', 1, 1, 5);   -- UNIQUE      -> 1062
INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price) VALUES ('X-NEG', 'neg', 1, 1, -5);    -- CHECK       -> 3819
INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price) VALUES ('X-NULL', NULL, 1, 1, 5);     -- NOT NULL    -> 1048
INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price) VALUES ('X-FK', 'fk', 999, 1, 5);     -- FOREIGN KEY -> 1452
DELETE FROM Categories WHERE category_id = 1;                                                                         -- RESTRICT    -> 1451
INSERT INTO Customers (customer_name, phone) VALUES ('Bad phone', '123');                                             -- CHECK       -> 3819
START TRANSACTION;                                                                                                    -- DEFAULT values fill what is not given
INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price) VALUES ('X-DEFAULT', 'defaults', 1, 1, 5);
SELECT sku, stock_quantity AS default_stock, min_stock_level AS default_minimum FROM Products WHERE sku = 'X-DEFAULT';
ROLLBACK;

SELECT '9. Triggers: the sale trigger pair, then an oversell' AS topic;
SELECT trigger_name, action_timing, event_manipulation, event_object_table
  FROM information_schema.triggers WHERE trigger_schema = DATABASE() ORDER BY trigger_name;
START TRANSACTION;
SELECT stock_quantity AS stock_before FROM Products WHERE product_id = 6;
INSERT INTO Sales (customer_id) VALUES (1);
SET @sale = LAST_INSERT_ID();
INSERT INTO Sale_Items (sale_id, product_id, quantity, unit_price) VALUES (@sale, 6, 2, 35);   -- BEFORE trigger checks, AFTER trigger acts
SELECT stock_quantity AS stock_after FROM Products WHERE product_id = 6;                         -- reduced by the trigger
SELECT total_amount AS sale_total FROM Sales WHERE sale_id = @sale;                              -- filled by the trigger
SELECT txn_type, quantity_change, remarks FROM Stock_Transactions ORDER BY txn_id DESC LIMIT 1;  -- ledger row written by the trigger
INSERT INTO Sales (customer_id) VALUES (1);
INSERT INTO Sale_Items (sale_id, product_id, quantity, unit_price) VALUES (LAST_INSERT_ID(), 6, 999999, 35);   -- oversell: ERROR 1644
ROLLBACK;
SELECT stock_quantity AS stock_after_rollback FROM Products WHERE product_id = 6;                -- back to the start

SELECT '10. Views' AS topic;
SELECT * FROM v_low_stock;
SELECT * FROM v_sales_summary ORDER BY sale_id DESC LIMIT 3;

SELECT '11. TCL: SAVEPOINT and ROLLBACK TO' AS topic;
START TRANSACTION;
UPDATE Products SET unit_price = unit_price + 1 WHERE product_id = 1;
SAVEPOINT after_first_change;
UPDATE Products SET unit_price = unit_price + 100 WHERE product_id = 1;
SELECT unit_price AS price_after_two_updates FROM Products WHERE product_id = 1;
ROLLBACK TO SAVEPOINT after_first_change;
SELECT unit_price AS price_after_rollback_to_savepoint FROM Products WHERE product_id = 1;
ROLLBACK;
SELECT unit_price AS price_after_full_rollback FROM Products WHERE product_id = 1;

SELECT '12. Indexes and EXPLAIN' AS topic;
SHOW INDEX FROM Products;
EXPLAIN SELECT product_name FROM Products WHERE category_id = 1 AND unit_price < 100;   -- can use the composite index (category_id, unit_price)
EXPLAIN SELECT product_name FROM Products WHERE product_name = 'Gel Pen';               -- can use idx_products_name
EXPLAIN SELECT product_name FROM Products WHERE description = 'x';                      -- no index on description: full table scan

SELECT '13. BLOB: stored product photos' AS topic;
SELECT COUNT(*) AS products_with_photo, COALESCE(SUM(LENGTH(photo)), 0) AS photo_bytes FROM Products WHERE photo IS NOT NULL;

SELECT '14. DCL: accounts created by roles.sql (an error here means roles.sql was not loaded)' AS topic;
SHOW GRANTS FOR 'ims_employee'@'%';
SHOW GRANTS FOR 'ims_admin'@'%';
