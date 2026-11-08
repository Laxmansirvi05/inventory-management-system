-- triggers.sql : seven triggers that keep stock, totals and the ledger consistent.
--   insert of a line : trg_purchase_items_ai, trg_sale_items_bi (stock check), trg_sale_items_ai
--   delete of a line : trg_purchase_items_ad, trg_sale_items_ad   (used by Undo)
--   edit of a line   : trg_purchase_items_bu, trg_sale_items_bu   (refused)
-- Load with the mysql client (DELIMITER is a client command).

DROP TRIGGER IF EXISTS trg_purchase_items_ai;
DROP TRIGGER IF EXISTS trg_sale_items_bi;
DROP TRIGGER IF EXISTS trg_sale_items_ai;
DROP TRIGGER IF EXISTS trg_sale_items_ad;
DROP TRIGGER IF EXISTS trg_sale_items_bu;
DROP TRIGGER IF EXISTS trg_purchase_items_ad;
DROP TRIGGER IF EXISTS trg_purchase_items_bu;

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

-- Removing a sale line (Undo): put the stock back, lower the sale total, write an UNDO ledger row.
CREATE TRIGGER trg_sale_items_ad AFTER DELETE ON Sale_Items
FOR EACH ROW
BEGIN
    UPDATE Products SET stock_quantity = stock_quantity + OLD.quantity
     WHERE product_id = OLD.product_id;
    UPDATE Sales SET total_amount = total_amount - OLD.quantity * OLD.unit_price
     WHERE sale_id = OLD.sale_id;
    INSERT INTO Stock_Transactions (product_id, txn_type, quantity_change, reference_id, remarks)
    VALUES (OLD.product_id, 'UNDO', OLD.quantity, OLD.sale_id, 'Sale line removed');
END$$

-- Removing a purchase line: take the stock out again (the CHECK stock >= 0 refuses it if it was already sold).
CREATE TRIGGER trg_purchase_items_ad AFTER DELETE ON Purchase_Items
FOR EACH ROW
BEGIN
    UPDATE Products SET stock_quantity = stock_quantity - OLD.quantity
     WHERE product_id = OLD.product_id;
    UPDATE Purchases SET total_amount = total_amount - OLD.quantity * OLD.unit_cost
     WHERE purchase_id = OLD.purchase_id;
    INSERT INTO Stock_Transactions (product_id, txn_type, quantity_change, reference_id, remarks)
    VALUES (OLD.product_id, 'UNDO', -OLD.quantity, OLD.purchase_id, 'Purchase line removed');
END$$

-- Lines are never edited in place: an edit would change the price or quantity without moving stock.
CREATE TRIGGER trg_sale_items_bu BEFORE UPDATE ON Sale_Items
FOR EACH ROW
BEGIN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Sale lines cannot be edited: remove the sale and record it again';
END$$

CREATE TRIGGER trg_purchase_items_bu BEFORE UPDATE ON Purchase_Items
FOR EACH ROW
BEGIN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Purchase lines cannot be edited: remove the purchase and record it again';
END$$

DELIMITER ;
