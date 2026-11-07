// product.h : the Product record used by every in-memory structure.
#pragma once
#include <string>

struct Product {
    int id = 0;
    std::string sku;
    std::string name;
    int categoryId = 0;
    int supplierId = 0;
    double price = 0.0;     // unit price in rupees (DB column is DECIMAL(10,2))
    int qty = 0;            // current stock (DB: stock_quantity)
    int minStock = 0;       // restock threshold (DB: min_stock_level)
    std::string expiry;     // "YYYY-MM-DD" or empty
    std::string description;// optional free text (DB: description)
    bool hasPhoto = false;  // a BLOB photo is stored (DB: photo IS NOT NULL)
};
