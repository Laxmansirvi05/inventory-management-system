-- schema.sql : DDL for the Inventory Management System (MySQL 8)
-- Run order: schema.sql, triggers.sql, views.sql, seed.sql, roles.sql
-- Re-runnable: drops and recreates the database tables (DROP, CREATE).

DROP TABLE IF EXISTS Stock_Transactions;
DROP TABLE IF EXISTS Sale_Items;
DROP TABLE IF EXISTS Sales;
DROP TABLE IF EXISTS Purchase_Items;
DROP TABLE IF EXISTS Purchases;
DROP TABLE IF EXISTS Products;
DROP TABLE IF EXISTS Customers;
DROP TABLE IF EXISTS Suppliers;
DROP TABLE IF EXISTS Categories;
DROP TABLE IF EXISTS Users;

CREATE TABLE Categories (
    category_id    INT AUTO_INCREMENT PRIMARY KEY,
    category_name  VARCHAR(50)  NOT NULL UNIQUE,
    description    VARCHAR(150)
) ENGINE=InnoDB;

CREATE TABLE Suppliers (
    supplier_id     INT AUTO_INCREMENT PRIMARY KEY,
    supplier_name   VARCHAR(80) NOT NULL,
    contact_person  VARCHAR(60),
    phone           CHAR(10)    NOT NULL UNIQUE,
    email           VARCHAR(80) UNIQUE,
    city            VARCHAR(40),
    CONSTRAINT chk_supplier_phone CHECK (phone REGEXP '^[0-9]{10}$')
) ENGINE=InnoDB;

CREATE TABLE Customers (
    customer_id    INT AUTO_INCREMENT PRIMARY KEY,
    customer_name  VARCHAR(80) NOT NULL,
    phone          CHAR(10)    NOT NULL UNIQUE,
    email          VARCHAR(80) UNIQUE,
    city           VARCHAR(40),
    registered_on  DATE        NOT NULL DEFAULT (CURRENT_DATE),
    CONSTRAINT chk_customer_phone CHECK (phone REGEXP '^[0-9]{10}$')
) ENGINE=InnoDB;

CREATE TABLE Products (
    product_id      INT AUTO_INCREMENT PRIMARY KEY,
    sku             VARCHAR(20)   NOT NULL UNIQUE,
    product_name    VARCHAR(80)   NOT NULL,
    category_id     INT           NOT NULL,
    supplier_id     INT           NOT NULL,
    unit_price      DECIMAL(10,2) NOT NULL,
    stock_quantity  INT           NOT NULL DEFAULT 0,
    min_stock_level INT           NOT NULL DEFAULT 10,
    expiry_date     DATE          NULL,
    description     VARCHAR(150),
    CONSTRAINT chk_price     CHECK (unit_price > 0),
    CONSTRAINT chk_stock     CHECK (stock_quantity >= 0),
    CONSTRAINT chk_min_stock CHECK (min_stock_level >= 0),
    CONSTRAINT fk_prod_cat FOREIGN KEY (category_id) REFERENCES Categories(category_id)
        ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT fk_prod_sup FOREIGN KEY (supplier_id) REFERENCES Suppliers(supplier_id)
        ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB;

-- NEW for the web app: product photo (kept as a separate ALTER to demonstrate ALTER TABLE)
ALTER TABLE Products ADD COLUMN photo MEDIUMBLOB NULL;

CREATE TABLE Purchases (
    purchase_id    INT AUTO_INCREMENT PRIMARY KEY,
    supplier_id    INT           NOT NULL,
    purchase_date  DATE          NOT NULL DEFAULT (CURRENT_DATE),
    total_amount   DECIMAL(12,2) NOT NULL DEFAULT 0,
    CONSTRAINT fk_pur_sup FOREIGN KEY (supplier_id) REFERENCES Suppliers(supplier_id)
        ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB;

CREATE TABLE Purchase_Items (
    purchase_id  INT           NOT NULL,
    product_id   INT           NOT NULL,
    quantity     INT           NOT NULL,
    unit_cost    DECIMAL(10,2) NOT NULL,
    PRIMARY KEY (purchase_id, product_id),
    CONSTRAINT chk_pi_qty  CHECK (quantity > 0),
    CONSTRAINT chk_pi_cost CHECK (unit_cost > 0),
    CONSTRAINT fk_pi_pur  FOREIGN KEY (purchase_id) REFERENCES Purchases(purchase_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_pi_prod FOREIGN KEY (product_id)  REFERENCES Products(product_id)
        ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB;

CREATE TABLE Sales (
    sale_id       INT AUTO_INCREMENT PRIMARY KEY,
    customer_id   INT           NOT NULL,
    sale_date     DATE          NOT NULL DEFAULT (CURRENT_DATE),
    total_amount  DECIMAL(12,2) NOT NULL DEFAULT 0,
    CONSTRAINT fk_sale_cust FOREIGN KEY (customer_id) REFERENCES Customers(customer_id)
        ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB;

CREATE TABLE Sale_Items (
    sale_id     INT           NOT NULL,
    product_id  INT           NOT NULL,
    quantity    INT           NOT NULL,
    unit_price  DECIMAL(10,2) NOT NULL,
    PRIMARY KEY (sale_id, product_id),
    CONSTRAINT chk_si_qty   CHECK (quantity > 0),
    CONSTRAINT chk_si_price CHECK (unit_price > 0),
    CONSTRAINT fk_si_sale FOREIGN KEY (sale_id)    REFERENCES Sales(sale_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_si_prod FOREIGN KEY (product_id) REFERENCES Products(product_id)
        ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB;

CREATE TABLE Stock_Transactions (
    txn_id           INT AUTO_INCREMENT PRIMARY KEY,
    product_id       INT          NOT NULL,
    txn_type         VARCHAR(10)  NOT NULL,
    quantity_change  INT          NOT NULL,
    txn_date         DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    reference_id     INT,
    remarks          VARCHAR(100),
    CONSTRAINT chk_txn_type CHECK (txn_type IN ('PURCHASE','SALE','UNDO','ADJUST')),
    CONSTRAINT fk_txn_prod FOREIGN KEY (product_id) REFERENCES Products(product_id)
        ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB;

-- NEW for the web app: login accounts (10th table). password_hash format: salt$sha256hex(salt+password)
CREATE TABLE Users (
    user_id        INT AUTO_INCREMENT PRIMARY KEY,
    username       VARCHAR(40)  NOT NULL UNIQUE,
    password_hash  VARCHAR(140) NOT NULL,
    role           VARCHAR(10)  NOT NULL,
    created_at     DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT chk_user_role CHECK (role IN ('ADMIN','EMPLOYEE'))
) ENGINE=InnoDB;

-- Indexes (primary keys, UNIQUE sku and foreign keys are indexed automatically)
CREATE INDEX idx_products_name ON Products(product_name);
CREATE INDEX idx_products_cat_price ON Products(category_id, unit_price);
