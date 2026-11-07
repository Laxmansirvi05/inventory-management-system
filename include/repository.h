// repository.h : database operations for products, sales and purchases.
// Rules: every write runs in a transaction; the in-memory Inventory is updated only after COMMIT;
// on any error the transaction is rolled back and memory is left untouched.
#pragma once
#include <string>
#include <vector>
#include "database.h"
#include "inventory.h"
#include "orders.h"
#include "sqllog.h"

struct WriteResult {
    bool ok = false;
    long long id = 0;           // new sale_id / purchase_id / product_id
    double total = 0.0;         // sale or purchase total as computed by the triggers
    long long affected = 0;     // rows matched by the statement (runStatement)
    unsigned errNo = 0;         // MySQL error number when !ok (0 = not a MySQL error)
    std::string error;
};

struct PurchaseLine {
    int productId = 0;
    int qty = 0;
    double unitCost = 0.0;
};

// SELECT all products (start-up load and re-sync). `log` may be null.
std::vector<Product> loadProducts(Database& db, SqlLog* log);

// Stock out: INSERT Sales + Sale_Items; BEFORE/AFTER triggers check and change stock.
// Unit prices come from Products.unit_price (they are not editable at sale time).
WriteResult recordSale(Database& db, Inventory& inv, int customerId, const std::vector<OrderLine>& lines, SqlLog& log);

// Stock in: INSERT Purchases + Purchase_Items; the AFTER trigger raises stock.
WriteResult recordPurchase(Database& db, Inventory& inv, int supplierId, const std::vector<PurchaseLine>& lines, SqlLog& log);

// Product maintenance. Stock is never edited here after creation (only purchases, sales and undo change it).
WriteResult insertProduct(Database& db, Inventory& inv, const Product& p, SqlLog& log);
WriteResult updateProductRow(Database& db, Inventory& inv, const Product& p, SqlLog& log);
WriteResult deleteProductRow(Database& db, Inventory& inv, int productId, SqlLog& log);

// Any single INSERT/UPDATE/DELETE in its own transaction (categories, suppliers, customers, users).
WriteResult runStatement(Database& db, const std::string& sql, const Params& params, SqlLog& log);

// Reverse the newest stock transaction in MySQL (stock + an UNDO ledger row), then in memory.
WriteResult undoLastTxn(Database& db, Inventory& inv, SqlLog& log);

// Store a product photo (BLOB).
WriteResult setProductPhoto(Database& db, Inventory& inv, int productId, const std::string& bytes, SqlLog& log);
