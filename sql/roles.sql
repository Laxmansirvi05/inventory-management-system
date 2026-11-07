-- roles.sql : DCL demo accounts. Change the database name if yours is not `ims`.
-- Passwords here are placeholders for local/demo use only; do not reuse them elsewhere.

DROP USER IF EXISTS 'ims_admin'@'%';
DROP USER IF EXISTS 'ims_employee'@'%';

CREATE USER 'ims_admin'@'%'    IDENTIFIED BY 'Admin#Demo2026';
CREATE USER 'ims_employee'@'%' IDENTIFIED BY 'Emp#Demo2026';

-- Admin: everything on the IMS database
GRANT ALL PRIVILEGES ON ims.* TO 'ims_admin'@'%';

-- Employee: read everything, record sales and purchases, add/edit customers, no deletes
GRANT SELECT ON ims.* TO 'ims_employee'@'%';
GRANT INSERT ON ims.Sales          TO 'ims_employee'@'%';
GRANT INSERT ON ims.Sale_Items     TO 'ims_employee'@'%';
GRANT INSERT ON ims.Purchases      TO 'ims_employee'@'%';
GRANT INSERT ON ims.Purchase_Items TO 'ims_employee'@'%';
GRANT INSERT, UPDATE ON ims.Customers TO 'ims_employee'@'%';
-- Triggers update Products, Sales/Purchases totals and the ledger on the employee's behalf
GRANT UPDATE ON ims.Products TO 'ims_employee'@'%';
GRANT UPDATE ON ims.Sales TO 'ims_employee'@'%';
GRANT UPDATE ON ims.Purchases TO 'ims_employee'@'%';
GRANT INSERT ON ims.Stock_Transactions TO 'ims_employee'@'%';

-- REVOKE demo: DELETE was handed out, then taken back, so employees can never delete rows
GRANT DELETE ON ims.* TO 'ims_employee'@'%';
REVOKE DELETE ON ims.* FROM 'ims_employee'@'%';
