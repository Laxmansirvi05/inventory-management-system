// repository.cpp : products, sales and purchases against MySQL, with Live-SQL annotations.
#include "repository.h"
#include <cstdlib>
#include <map>

namespace {

Param optText(const std::string& s) { return s.empty() ? PNull() : P(s); }

std::string cell(const QueryResult& r, size_t row, size_t col) {
    if (row >= r.rows.size() || col >= r.rows[row].size() || !r.rows[row][col]) return "";
    return *r.rows[row][col];
}

// Current stock of one product, read without logging (used only to describe what a trigger did).
std::string stockOf(Database& db, int productId) {
    QueryResult r = db.execute("SELECT stock_quantity FROM Products WHERE product_id = ?", {P(productId)}, nullptr);
    return cell(r, 0, 0);
}

SqlSnapshot stockSnapshot(int productId, const std::string& qty) {
    SqlSnapshot s;
    s.present = true;
    s.table = "Products";
    s.key = std::to_string(productId);
    s.row["stock_quantity"] = qty;
    return s;
}

// Roll back after a failure; a failing ROLLBACK is ignored because the original error matters more.
void abortTransaction(Database& db, SqlLog& log) {
    try { db.rollback(&log); } catch (const DbError&) {}
}

WriteResult failure(const DbError& e) {
    WriteResult r;
    r.errNo = e.errNo;
    r.error = e.what();
    return r;
}

// Memory no longer matches the database (should never happen): reload everything from MySQL.
void resync(Database& db, Inventory& inv) { inv.replaceAll(loadProducts(db, nullptr)); }

// A connection lost around COMMIT leaves it unknown whether the change was stored: reload memory from MySQL.
void resyncIfConnectionLost(Database& db, Inventory& inv, unsigned errNo) {
    if (!isConnectionLost(errNo)) return;
    try { resync(db, inv); } catch (const DbError&) { /* still down: the next request tries again */ }
}

}  // namespace

std::vector<Product> loadProducts(Database& db, SqlLog* log) {
    QueryResult r = db.execute(
        "SELECT product_id, sku, product_name, category_id, supplier_id, unit_price, stock_quantity, "
        "min_stock_level, expiry_date, description, (photo IS NOT NULL) FROM Products ORDER BY product_id", {}, log);
    std::vector<Product> out;
    for (size_t i = 0; i < r.rows.size(); ++i) {
        Product p;
        p.id = std::atoi(cell(r, i, 0).c_str());
        p.sku = cell(r, i, 1);
        p.name = cell(r, i, 2);
        p.categoryId = std::atoi(cell(r, i, 3).c_str());
        p.supplierId = std::atoi(cell(r, i, 4).c_str());
        p.price = std::atof(cell(r, i, 5).c_str());
        p.qty = std::atoi(cell(r, i, 6).c_str());
        p.minStock = std::atoi(cell(r, i, 7).c_str());
        p.expiry = cell(r, i, 8);
        p.description = cell(r, i, 9);
        p.hasPhoto = cell(r, i, 10) == "1";
        out.push_back(p);
    }
    return out;
}

WriteResult recordSale(Database& db, Inventory& inv, int customerId, const std::vector<OrderLine>& lines, SqlLog& log) {
    std::map<int, int> merged;                           // same product on two lines is one row (primary key)
    for (const OrderLine& l : lines) merged[l.productId] += l.qty;

    WriteResult res;
    std::string lastStock, lastQty;                      // for the error note if the BEFORE trigger rejects
    int lastPid = 0;
    try {
        db.begin(&log);
        QueryResult sale = db.execute("INSERT INTO Sales (customer_id) VALUES (?)", {P(customerId)}, &log);
        long long saleId = static_cast<long long>(sale.insertId);

        for (const auto& line : merged) {
            int pid = line.first, qty = line.second;
            QueryResult pr = db.execute("SELECT unit_price, stock_quantity FROM Products WHERE product_id = ?", {P(pid)}, &log);
            if (pr.rows.empty()) throw DbError(0, "Product not found: " + std::to_string(pid));
            std::string price = cell(pr, 0, 0), stockBefore = cell(pr, 0, 1);
            lastPid = pid; lastStock = stockBefore; lastQty = std::to_string(qty);

            db.execute("INSERT INTO Sale_Items (sale_id, product_id, quantity, unit_price) VALUES (?, ?, ?, ?)",
                       {P(saleId), P(pid), P(qty), Param{false, true, price}}, &log);

            // Describe what the triggers did, for the Live SQL panel.
            std::string stockAfter = stockOf(db, pid);
            SqlEntry& ins = log.last();
            ins.triggers.push_back({"trg_sale_items_bi", "stock " + stockBefore + " >= " + std::to_string(qty) + " -> allowed", "ok"});
            ins.triggers.push_back({"trg_sale_items_ai",
                                    "stock " + stockBefore + " -> " + stockAfter + ", sale total updated, ledger row added", "action"});
            ins.before = stockSnapshot(pid, stockBefore);
            ins.after = stockSnapshot(pid, stockAfter);
        }
        QueryResult tot = db.execute("SELECT total_amount FROM Sales WHERE sale_id = ?", {P(saleId)}, &log);
        res.total = std::atof(cell(tot, 0, 0).c_str());
        res.id = saleId;
        db.commit(&log);
    } catch (const DbError& e) {
        if (e.errNo == 1644 && !log.empty()) {
            log.last().triggers.push_back({"trg_sale_items_bi",
                "stock " + lastStock + " < " + lastQty + " -> SIGNAL, row rejected", "error"});
            log.last().before = stockSnapshot(lastPid, lastStock);
        }
        abortTransaction(db, log);
        resyncIfConnectionLost(db, inv, e.errNo);
        return failure(e);
    }

    // COMMIT succeeded: now bring memory in line.
    bool inSync = true;
    for (const auto& line : merged) inSync = inv.sell(line.first, line.second).ok && inSync;
    if (!inSync) resync(db, inv);
    res.ok = true;
    return res;
}

WriteResult recordPurchase(Database& db, Inventory& inv, int supplierId, const std::vector<PurchaseLine>& lines, SqlLog& log) {
    std::map<int, PurchaseLine> merged;                  // one row per product; costs of repeats are averaged by the caller
    for (const PurchaseLine& l : lines) {
        if (merged.count(l.productId)) merged[l.productId].qty += l.qty;
        else merged[l.productId] = l;
    }
    WriteResult res;
    try {
        db.begin(&log);
        QueryResult pur = db.execute("INSERT INTO Purchases (supplier_id) VALUES (?)", {P(supplierId)}, &log);
        long long purchaseId = static_cast<long long>(pur.insertId);
        for (const auto& kv : merged) {
            const PurchaseLine& l = kv.second;
            std::string stockBefore = stockOf(db, l.productId);
            db.execute("INSERT INTO Purchase_Items (purchase_id, product_id, quantity, unit_cost) VALUES (?, ?, ?, ?)",
                       {P(purchaseId), P(l.productId), P(l.qty), P(l.unitCost)}, &log);
            std::string stockAfter = stockOf(db, l.productId);
            SqlEntry& ins = log.last();
            ins.triggers.push_back({"trg_purchase_items_ai",
                                    "stock " + stockBefore + " -> " + stockAfter + ", purchase total updated, ledger row added", "action"});
            ins.before = stockSnapshot(l.productId, stockBefore);
            ins.after = stockSnapshot(l.productId, stockAfter);
        }
        QueryResult tot = db.execute("SELECT total_amount FROM Purchases WHERE purchase_id = ?", {P(purchaseId)}, &log);
        res.total = std::atof(cell(tot, 0, 0).c_str());
        res.id = purchaseId;
        db.commit(&log);
    } catch (const DbError& e) {
        abortTransaction(db, log);
        resyncIfConnectionLost(db, inv, e.errNo);
        return failure(e);
    }
    bool inSync = true;
    for (const auto& kv : merged) inSync = inv.purchase(kv.second.productId, kv.second.qty).ok && inSync;
    if (!inSync) resync(db, inv);
    res.ok = true;
    return res;
}

WriteResult insertProduct(Database& db, Inventory& inv, const Product& p, SqlLog& log) {
    WriteResult res;
    try {
        db.begin(&log);
        QueryResult r = db.execute(
            "INSERT INTO Products (sku, product_name, category_id, supplier_id, unit_price, stock_quantity, "
            "min_stock_level, expiry_date, description) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            {P(p.sku), P(p.name), P(p.categoryId), P(p.supplierId), P(p.price), P(p.qty), P(p.minStock),
             optText(p.expiry), optText(p.description)}, &log);
        res.id = static_cast<long long>(r.insertId);
        if (p.qty > 0) {                                  // opening stock goes into the ledger too
            db.execute("INSERT INTO Stock_Transactions (product_id, txn_type, quantity_change, reference_id, remarks) "
                       "VALUES (?, 'ADJUST', ?, ?, 'Opening stock')", {P(res.id), P(p.qty), P(res.id)}, &log);
        }
        db.commit(&log);
    } catch (const DbError& e) {
        abortTransaction(db, log);
        resyncIfConnectionLost(db, inv, e.errNo);
        return failure(e);
    }
    Product mem = p;
    mem.id = static_cast<int>(res.id);
    std::string err;
    if (!inv.addProduct(mem, err)) resync(db, inv);
    res.ok = true;
    return res;
}

WriteResult updateProductRow(Database& db, Inventory& inv, const Product& p, SqlLog& log) {
    WriteResult res;
    try {
        db.begin(&log);
        QueryResult r = db.execute(
            "UPDATE Products SET sku = ?, product_name = ?, category_id = ?, supplier_id = ?, unit_price = ?, "
            "min_stock_level = ?, expiry_date = ?, description = ? WHERE product_id = ?",
            {P(p.sku), P(p.name), P(p.categoryId), P(p.supplierId), P(p.price), P(p.minStock),
             optText(p.expiry), optText(p.description), P(p.id)}, &log);
        if (r.affectedRows == 0 && inv.findById(p.id) == nullptr) throw DbError(0, "Product not found");
        res.id = p.id;
        db.commit(&log);
    } catch (const DbError& e) {
        abortTransaction(db, log);
        resyncIfConnectionLost(db, inv, e.errNo);
        return failure(e);
    }
    Product mem = p;
    if (const Product* cur = inv.findById(p.id)) { mem.qty = cur->qty; mem.hasPhoto = cur->hasPhoto; }   // not edited here
    std::string err;
    if (!inv.updateProduct(mem, err)) resync(db, inv);
    res.ok = true;
    return res;
}

WriteResult deleteProductRow(Database& db, Inventory& inv, int productId, SqlLog& log) {
    WriteResult res;
    try {
        db.begin(&log);
        // A product's own opening-stock ledger rows must not block its deletion; sales, purchases and
        // other ledger rows still do (error 1451), and the ROLLBACK then restores these rows too.
        db.execute("DELETE FROM Stock_Transactions WHERE product_id = ? AND txn_type = 'ADJUST'", {P(productId)}, &log);
        QueryResult r = db.execute("DELETE FROM Products WHERE product_id = ?", {P(productId)}, &log);
        if (r.affectedRows == 0) throw DbError(0, "Product not found");
        res.id = productId;
        db.commit(&log);
    } catch (const DbError& e) {
        abortTransaction(db, log);
        resyncIfConnectionLost(db, inv, e.errNo);
        return failure(e);
    }
    inv.removeProduct(productId);
    res.ok = true;
    return res;
}

WriteResult runStatement(Database& db, const std::string& sql, const Params& params, SqlLog& log) {
    WriteResult res;
    try {
        db.begin(&log);
        QueryResult r = db.execute(sql, params, &log);
        res.id = static_cast<long long>(r.insertId);
        res.affected = r.affectedRows;
        db.commit(&log);
    } catch (const DbError& e) {
        abortTransaction(db, log);
        return failure(e);
    }
    res.ok = true;
    return res;
}

WriteResult undoLastTxn(Database& db, Inventory& inv, SqlLog& log) {
    WriteResult res;
    StockTxn t;
    if (!inv.peekUndo(t)) { res.error = "Nothing to undo"; return res; }
    try {
        db.begin(&log);
        std::string before = stockOf(db, t.productId);
        db.execute("UPDATE Products SET stock_quantity = stock_quantity + ? WHERE product_id = ?",
                   {P(-t.delta), P(t.productId)}, &log);
        std::string after = stockOf(db, t.productId);
        SqlEntry& upd = log.last();
        upd.before = stockSnapshot(t.productId, before);
        upd.after = stockSnapshot(t.productId, after);
        db.execute("INSERT INTO Stock_Transactions (product_id, txn_type, quantity_change, reference_id, remarks) "
                   "VALUES (?, 'UNDO', ?, NULL, ?)",
                   {P(t.productId), P(-t.delta), P("Undo of last " + t.kind)}, &log);
        res.id = t.productId;
        db.commit(&log);
    } catch (const DbError& e) {              // e.g. CHECK stock >= 0 fails when stock was edited since
        abortTransaction(db, log);
        resyncIfConnectionLost(db, inv, e.errNo);
        return failure(e);
    }
    if (!inv.undoLast().ok) resync(db, inv);
    res.ok = true;
    return res;
}

WriteResult setProductPhoto(Database& db, Inventory& inv, int productId, const std::string& bytes, SqlLog& log) {
    WriteResult res;
    try {
        db.begin(&log);
        QueryResult r = db.execute("UPDATE Products SET photo = ? WHERE product_id = ?", {PBlob(bytes), P(productId)}, &log);
        if (r.affectedRows == 0) throw DbError(0, "Product not found");
        res.id = productId;
        db.commit(&log);
    } catch (const DbError& e) {
        abortTransaction(db, log);
        resyncIfConnectionLost(db, inv, e.errNo);
        return failure(e);
    }
    if (Product* p = inv.findById(productId)) p->hasPhoto = true;
    res.ok = true;
    return res;
}
