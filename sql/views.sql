-- views.sql : three views used by the alerts, reports and product catalogue.

CREATE OR REPLACE VIEW v_low_stock AS
SELECT p.product_id, p.sku, p.product_name, p.stock_quantity, p.min_stock_level,
       ROUND(p.stock_quantity / NULLIF(p.min_stock_level, 0), 2) AS stock_ratio,
       s.supplier_name
FROM Products p
JOIN Suppliers s ON s.supplier_id = p.supplier_id
WHERE p.stock_quantity <= p.min_stock_level
ORDER BY stock_ratio ASC;

CREATE OR REPLACE VIEW v_sales_summary AS
SELECT s.sale_id, s.sale_date, c.customer_name, s.total_amount,
       COUNT(si.product_id) AS item_lines,
       COALESCE(SUM(si.quantity), 0) AS units_sold
FROM Sales s
JOIN Customers c ON c.customer_id = s.customer_id
LEFT JOIN Sale_Items si ON si.sale_id = s.sale_id
GROUP BY s.sale_id, s.sale_date, c.customer_name, s.total_amount;

CREATE OR REPLACE VIEW v_product_catalogue AS
SELECT p.product_id, p.sku, p.product_name, c.category_name, s.supplier_name,
       p.unit_price, p.stock_quantity, p.min_stock_level, p.expiry_date,
       (p.photo IS NOT NULL) AS has_photo
FROM Products p
JOIN Categories c ON c.category_id = p.category_id
JOIN Suppliers  s ON s.supplier_id = p.supplier_id;
