-- triggers.sql : three triggers that keep stock, totals and the ledger consistent.
-- Load with the mysql client (DELIMITER is a client command).

DROP TRIGGER IF EXISTS trg_purchase_items_ai;
DROP TRIGGER IF EXISTS trg_sale_items_bi;
DROP TRIGGER IF EXISTS trg_sale_items_ai;

DELIMITER $$

-- Stock in: raise stock, add the line to the purchase total, write a ledger row.
CREATE TRIGGER trg_purchase_items_ai AFTER INSERT ON Purchase_Items
FOR EACH ROW
BEGIN
    UPDATE Products SET stock_quantity = stock_quantity + NEW.quantity
     WHERE product_id = NEW.product_id;
    UPDATE Purchases SET total_amount = total_amount + NEW.quantity * NEW.unit_cost
     WHERE purchase_id = NEW.purchase_id;
    INSERT INTO Stock_Transactions (product_id, txn_type, quantity_change, reference_id, remarks)
    VALUES (NEW.product_id, 'PURCHASE', NEW.quantity, NEW.purchase_id, 'Stock received');
END$$

-- Stock out, step 1: refuse the sale line when stock is not enough (error 1644).
CREATE TRIGGER trg_sale_items_bi BEFORE INSERT ON Sale_Items
FOR EACH ROW
BEGIN
    DECLARE available INT;
    DECLARE msg VARCHAR(100);
    SELECT stock_quantity INTO available FROM Products WHERE product_id = NEW.product_id;
    IF available IS NULL OR NEW.quantity > available THEN
        SET msg = CONCAT('Insufficient stock for product ', NEW.product_id);   -- MESSAGE_TEXT needs a variable
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = msg;
    END IF;
END$$

-- Stock out, step 2: reduce stock, add the line to the sale total, write a ledger row.
CREATE TRIGGER trg_sale_items_ai AFTER INSERT ON Sale_Items
FOR EACH ROW
BEGIN
    UPDATE Products SET stock_quantity = stock_quantity - NEW.quantity
     WHERE product_id = NEW.product_id;
    UPDATE Sales SET total_amount = total_amount + NEW.quantity * NEW.unit_price
     WHERE sale_id = NEW.sale_id;
    INSERT INTO Stock_Transactions (product_id, txn_type, quantity_change, reference_id, remarks)
    VALUES (NEW.product_id, 'SALE', -NEW.quantity, NEW.sale_id, 'Stock sold');
END$$

DELIMITER ;
