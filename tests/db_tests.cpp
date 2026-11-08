// db_tests.cpp : Phase 4 console test of the database layer against a real MySQL database.
// It WRITES data, so it refuses to run unless DB_NAME ends with "_test".
//   scripts/load_db.sh ims_test && DB_NAME=ims_test build/db_tests
#include <chrono>
#include <iostream>
#include <thread>
#include "database.h"
#include "repository.h"

static int failures = 0, checks = 0;
#define CHECK(cond, label) do { ++checks; if (cond) std::cout << "  PASS  " << label << "\n"; \
    else { ++failures; std::cout << "  FAIL  " << label << "   (" #cond ")\n"; } } while (0)

// Stock of a product as the database sees it.
static int dbStock(Database& db, int id) {
    QueryResult r = db.execute("SELECT stock_quantity FROM Products WHERE product_id = ?", {P(id)}, nullptr);
    return std::stoi(*r.rows[0][0]);
}

static long long dbCount(Database& db, const std::string& table) {
    QueryResult r = db.execute("SELECT COUNT(*) FROM " + table, {}, nullptr);   // table names are fixed in this test
    return std::stoll(*r.rows[0][0]);
}

static bool logHasSql(const SqlLog& log, const std::string& prefix) {
    for (const SqlEntry& e : log.entries()) if (e.sql.rfind(prefix, 0) == 0) return true;
    return false;
}

int main() {
    DbConfig cfg = DbConfig::fromEnv();
    if (cfg.name.size() < 5 || cfg.name.compare(cfg.name.size() - 5, 5, "_test") != 0) {
        std::cerr << "Refusing to run: DB_NAME must end with _test (got '" << cfg.name << "').\n"
                  << "Create it with: scripts/load_db.sh ims_test\n";
        return 2;
    }
    Database db;
    std::string err;
    if (!db.connect(cfg, err)) { std::cerr << "Cannot connect: " << err << "\n"; return 2; }

    std::cout << "[load products into memory]\n";
    Inventory inv;
    inv.replaceAll(loadProducts(db, nullptr));
    CHECK(inv.size() == 60, "60 products loaded from MySQL");
    CHECK(inv.findById(6) && inv.findById(6)->name == "Notebook 100 Pages", "hash map lookup works on loaded data");
    CHECK(inv.lowStockAlerts().size() == 6, "six low-stock alerts built from loaded data");

    std::cout << "[sale: transaction + triggers]\n";
    int stockMem0 = inv.findById(6)->qty;
    SqlLog saleLog;
    WriteResult sale = recordSale(db, inv, 1, {{6, 2}}, saleLog);
    std::cout << saleLog.toText();
    CHECK(sale.ok && sale.total == 70.0, "sale succeeded, total 2 x 35 = 70.00 (set by trigger)");
    CHECK(dbStock(db, 6) == stockMem0 - 2 && inv.findById(6)->qty == stockMem0 - 2, "database and memory both reduced by 2");
    CHECK(saleLog.entries().front().sql == "START TRANSACTION" && saleLog.entries().back().sql == "COMMIT", "log runs START TRANSACTION ... COMMIT");
    CHECK(logHasSql(saleLog, "INSERT INTO Sale_Items"), "log shows the Sale_Items INSERT");
    bool sawTrigger = false;
    for (const SqlEntry& e : saleLog.entries()) if (e.triggers.size() == 2 && e.before.present && e.after.present) sawTrigger = true;
    CHECK(sawTrigger, "INSERT carries trigger notes and before/after stock");
    CHECK(dbCount(db, "Stock_Transactions") == 147, "ledger row added by trigger (146 -> 147)");

    std::cout << "[oversell: error 1644 and ROLLBACK]\n";
    int dbBefore = dbStock(db, 6), memBefore = inv.findById(6)->qty;
    long long salesBefore = dbCount(db, "Sales");
    SqlLog badLog;
    WriteResult bad = recordSale(db, inv, 1, {{6, 100000}}, badLog);
    std::cout << badLog.toText();
    CHECK(!bad.ok && bad.errNo == 1644, "oversell rejected with MySQL error 1644");
    CHECK(bad.error == "Insufficient stock for product 6", "error text names the product");
    CHECK(badLog.entries().back().sql == "ROLLBACK", "log ends with ROLLBACK");
    CHECK(dbStock(db, 6) == dbBefore && inv.findById(6)->qty == memBefore, "stock unchanged in database and memory");
    CHECK(dbCount(db, "Sales") == salesBefore, "no half-created Sales row left behind");

    std::cout << "[sale with two items, one too large: nothing sold]\n";
    int s7 = dbStock(db, 7);
    SqlLog mixLog;
    WriteResult mix = recordSale(db, inv, 2, {{7, 1}, {6, 100000}}, mixLog);
    CHECK(!mix.ok && dbStock(db, 7) == s7 && inv.findById(7)->qty == s7, "first line rolled back together with the failed one");

    std::cout << "[purchase]\n";
    int p9 = dbStock(db, 9);
    SqlLog purLog;
    WriteResult pur = recordPurchase(db, inv, 5, {{9, 40, 20.0}}, purLog);
    std::cout << purLog.toText();
    CHECK(pur.ok && pur.total == 800.0, "purchase total 40 x 20 = 800.00");
    CHECK(dbStock(db, 9) == p9 + 40 && inv.findById(9)->qty == p9 + 40, "stock raised in database and memory");
    CHECK(inv.undoDepth() == 2, "memory undo stack holds the sale and the purchase");

    std::cout << "[product add / update / delete and constraint errors]\n";
    Product np;
    np.sku = "NEW0001"; np.name = "Test Item'; DROP TABLE Products;--"; np.categoryId = 1; np.supplierId = 1;
    np.price = 12.5; np.qty = 5; np.minStock = 2; np.description = "demo";
    SqlLog addLog;
    WriteResult add = insertProduct(db, inv, np, addLog);
    CHECK(add.ok && add.id > 60 && inv.findById(static_cast<int>(add.id)) != nullptr, "product added; id assigned by MySQL and mirrored in memory");
    CHECK(dbCount(db, "Products") == 61, "injection text stored as plain text, table still exists");
    QueryResult nm = db.execute("SELECT product_name FROM Products WHERE product_id = ?", {P(add.id)}, nullptr);
    CHECK(*nm.rows[0][0] == np.name, "name round-trips exactly");

    size_t memSize = inv.size();
    SqlLog dupLog;
    WriteResult dup = insertProduct(db, inv, np, dupLog);
    CHECK(!dup.ok && dup.errNo == 1062 && inv.size() == memSize && dupLog.entries().back().sql == "ROLLBACK", "duplicate SKU: error 1062, memory unchanged");
    Product neg = np; neg.sku = "NEW0002"; neg.price = -5;
    SqlLog negLog;
    WriteResult negRes = insertProduct(db, inv, neg, negLog);
    CHECK(!negRes.ok && negRes.errNo == 3819 && inv.size() == memSize, "negative price: CHECK error 3819, memory unchanged");

    Product up = *inv.findById(static_cast<int>(add.id));
    up.price = 15; up.name = "Renamed Item";
    SqlLog upLog;
    WriteResult upd = updateProductRow(db, inv, up, upLog);
    CHECK(upd.ok && inv.findById(static_cast<int>(add.id))->price == 15 && inv.findById(static_cast<int>(add.id))->qty == 5, "update changes price, keeps stock");

    SqlLog delBad;
    WriteResult delUsed = deleteProductRow(db, inv, 6, delBad);
    CHECK(!delUsed.ok && delUsed.errNo == 1451 && inv.findById(6) != nullptr, "delete of a product with history: error 1451, memory unchanged");
    Product fresh = np; fresh.sku = "NEW0003"; fresh.qty = 0;
    WriteResult f = insertProduct(db, inv, fresh, addLog);
    SqlLog delOk;
    WriteResult del = deleteProductRow(db, inv, static_cast<int>(f.id), delOk);
    CHECK(f.ok && del.ok && inv.findById(static_cast<int>(f.id)) == nullptr, "delete of a product with no history works");

    std::cout << "[delete and edit triggers, no silent cascade]\n";
    {
        Inventory scratch;
        scratch.replaceAll(loadProducts(db, nullptr));
        int s11 = dbStock(db, 11);
        SqlLog l;
        WriteResult sale = recordSale(db, scratch, 1, {{11, 3}}, l);
        CHECK(sale.ok && dbStock(db, 11) == s11 - 3, "setup: a sale of 3 took stock down");
        db.begin(nullptr);                                  // remove the line by hand, then look, then roll back
        db.execute("DELETE FROM Sale_Items WHERE sale_id = ?", {P(sale.id)}, nullptr);
        CHECK(dbStock(db, 11) == s11, "deleting a sale line puts the stock back (delete trigger)");
        QueryResult tot = db.execute("SELECT total_amount FROM Sales WHERE sale_id = ?", {P(sale.id)}, nullptr);
        CHECK(std::stod(*tot.rows[0][0]) == 0.0, "... and lowers the sale total");
        QueryResult led = db.execute("SELECT txn_type, quantity_change FROM Stock_Transactions ORDER BY txn_id DESC LIMIT 1", {}, nullptr);
        CHECK(*led.rows[0][0] == "UNDO" && *led.rows[0][1] == "3", "... and writes an UNDO ledger row");
        db.rollback(nullptr);
        unsigned code = 0;
        try { db.execute("DELETE FROM Sales WHERE sale_id = ?", {P(sale.id)}, nullptr); } catch (const DbError& e) { code = e.errNo; }
        CHECK(code == 1451, "deleting a sale that still has lines is refused (foreign key RESTRICT, no cascade)");
        code = 0;
        try { db.execute("UPDATE Sale_Items SET quantity = 1 WHERE sale_id = ?", {P(sale.id)}, nullptr); } catch (const DbError& e) { code = e.errNo; }
        CHECK(code == 1644, "editing a sale line in place is refused by trigger");
        SqlLog pl;
        WriteResult pur = recordPurchase(db, scratch, 1, {{11, 2, 10.0}}, pl);
        code = 0;
        try { db.execute("UPDATE Purchase_Items SET quantity = 9 WHERE purchase_id = ?", {P(pur.id)}, nullptr); } catch (const DbError& e) { code = e.errNo; }
        CHECK(pur.ok && code == 1644, "editing a purchase line in place is refused by trigger");
    }

    std::cout << "[undo removes the whole sale or purchase in MySQL]\n";
    {
        Inventory inv2;
        inv2.replaceAll(loadProducts(db, nullptr));
        long long salesBefore = dbCount(db, "Sales");
        int a = dbStock(db, 12), b = dbStock(db, 13);
        SqlLog sl;
        WriteResult s2 = recordSale(db, inv2, 1, {{12, 2}, {13, 1}}, sl);
        SqlLog ul;
        WriteResult u = undoLastTxn(db, inv2, ul);
        std::cout << ul.toText();
        CHECK(u.ok && u.kind == "SALE" && u.lines == 2 && u.id == s2.id, "undo reports the sale and its two lines");
        CHECK(dbStock(db, 12) == a && dbStock(db, 13) == b && inv2.findById(12)->qty == a && inv2.findById(13)->qty == b, "stock is restored in the database and in memory");
        CHECK(dbCount(db, "Sales") == salesBefore, "the sale itself is gone, so reports agree with stock again");
        bool traced = false;
        for (const SqlEntry& e : ul.entries()) if (e.sql.rfind("DELETE FROM Sale_Items", 0) == 0 && e.triggers.size() == 1 && e.before.present && e.after.present) traced = true;
        CHECK(traced && ul.entries().back().sql == "COMMIT", "the log shows each delete with its trigger note and before/after stock");
        QueryResult drift = db.execute("SELECT COUNT(*) FROM Products p WHERE p.stock_quantity <> (SELECT COALESCE(SUM(quantity_change), 0) FROM Stock_Transactions t WHERE t.product_id = p.product_id)", {}, nullptr);
        CHECK(*drift.rows[0][0] == "0", "stock still equals the ledger sum for every product");

        SqlLog pl;
        WriteResult pur = recordPurchase(db, inv2, 1, {{14, 5, 10.0}}, pl);
        db.execute("UPDATE Products SET stock_quantity = 0 WHERE product_id = 14", {}, nullptr);      // the stock left behind our back
        SqlLog bad;
        WriteResult ub = undoLastTxn(db, inv2, bad);
        CHECK(pur.ok && !ub.ok && ub.errNo == 0 && ub.error.rfind("Cannot undo purchase", 0) == 0, "a purchase whose stock is already gone cannot be undone");
        CHECK(bad.entries().back().sql == "ROLLBACK" && inv2.undoDepth() >= 1, "... it is rolled back and stays on the undo stack");
    }

    std::cout << "[lost connection: the server closes an idle connection]\n";
    {
        Database d2;
        std::string e2;
        d2.connect(cfg, e2);
        auto idleOut = [&]() {                              // make the server drop this connection after 1 s of silence
            d2.execute("SET SESSION wait_timeout = 1", {}, nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        };
        idleOut();
        long long count = -1;
        try { count = std::stoll(*d2.execute("SELECT COUNT(*) FROM Products", {}, nullptr).rows[0][0]); } catch (const DbError&) {}
        CHECK(count > 0, "a statement after an idle disconnect reconnects by itself and succeeds");

        d2.begin(nullptr);
        idleOut();
        unsigned code = 0;
        try { d2.execute("SELECT 1", {}, nullptr); } catch (const DbError& e) { code = e.errNo; }
        CHECK(isConnectionLost(code), "a connection lost INSIDE a transaction is reported, never silently retried");
        try { d2.rollback(nullptr); } catch (const DbError&) {}
        bool again = false;
        try { again = d2.execute("SELECT 1", {}, nullptr).rows.size() == 1; } catch (const DbError&) {}
        CHECK(again, "the next request after that works again (connection repaired)");

        SqlLog repaired;
        idleOut();
        recordSale(d2, inv, 1, {{7, 1}}, repaired);          // a whole sale right after a dropped connection
        CHECK(!repaired.empty() && repaired.entries().back().sql == "COMMIT", "a sale started on a dropped connection still completes (begin is retried)");
    }

    std::cout << "[log as JSON]\n";
    nlohmann::json j = saleLog.toJson();
    CHECK(j.is_array() && j.size() == saleLog.size() && j[0]["seq"] == 1, "log serialises to JSON array");
    nlohmann::json jb = badLog.toJson();
    CHECK(jb[jb.size() - 2]["error"]["mysql_errno"] == 1644, "failed statement carries mysql_errno 1644 in JSON");

    std::cout << "\n" << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
