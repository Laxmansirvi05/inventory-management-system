// routes_reports.cpp : dashboard, recent transactions, named reports, user management, database model.
#include <cmath>
#include <cctype>
#include "server.h"

namespace {

// ---------- dashboard ----------

Reply dashboard(Ctx& c) {
    double value = 0;
    for (const Product& p : c.app.inv.all()) value += p.qty * p.price;
    QueryResult m = c.app.db.execute(
        "SELECT COUNT(*) AS orders, COALESCE(SUM(total_amount), 0) AS revenue FROM Sales WHERE sale_date >= ?",
        {P(firstOfMonth())}, &c.log);
    return okReply({{"products", c.app.inv.size()},
                    {"stock_value", std::round(value * 100) / 100},
                    {"low_stock", c.app.inv.lowStockAlerts().size()},
                    {"sales_this_month", rowsToJson(m)[0]},
                    {"undo_depth", c.app.inv.undoDepth()}});
}

Reply recentTransactions(Ctx& c) {
    return okReply(rowsToJson(c.app.db.execute(
        "SELECT t.txn_id, t.txn_date, p.product_name, t.txn_type, t.quantity_change, t.remarks "
        "FROM Stock_Transactions t JOIN Products p ON p.product_id = t.product_id "
        "ORDER BY t.txn_id DESC LIMIT 10", {}, &c.log)));
}

// ---------- reports (fixed list: names choose the SQL, nothing from the client is run as SQL) ----------

struct Report { const char* name; const char* title; const char* sql; };

const std::vector<Report>& reports() {
    static const std::vector<Report> list = {
        {"stock-value", "Stock value by category",
         "SELECT c.category_name, COUNT(*) AS products, SUM(p.stock_quantity) AS units, "
         "SUM(p.stock_quantity * p.unit_price) AS stock_value "
         "FROM Products p JOIN Categories c ON c.category_id = p.category_id "
         "GROUP BY c.category_name ORDER BY stock_value DESC"},
        {"top-sellers", "Top selling products",
         "SELECT p.product_name, SUM(si.quantity) AS units_sold, SUM(si.quantity * si.unit_price) AS revenue "
         "FROM Sale_Items si JOIN Products p ON p.product_id = si.product_id "
         "GROUP BY p.product_id, p.product_name ORDER BY units_sold DESC LIMIT 10"},
        {"sales-per-customer", "Sales per customer",
         "SELECT c.customer_name, COUNT(s.sale_id) AS orders, COALESCE(SUM(s.total_amount), 0) AS total_spent "
         "FROM Customers c LEFT JOIN Sales s ON s.customer_id = c.customer_id "
         "GROUP BY c.customer_id, c.customer_name ORDER BY total_spent DESC"},
        {"purchases-per-supplier", "Purchases per supplier",
         "SELECT s.supplier_name, COUNT(p.purchase_id) AS purchases, COALESCE(SUM(p.total_amount), 0) AS total_bought "
         "FROM Suppliers s LEFT JOIN Purchases p ON p.supplier_id = s.supplier_id "
         "GROUP BY s.supplier_id, s.supplier_name ORDER BY total_bought DESC"},
        {"never-sold", "Products never sold",
         "SELECT p.sku, p.product_name, p.stock_quantity FROM Products p "
         "WHERE p.product_id NOT IN (SELECT product_id FROM Sale_Items) ORDER BY p.product_name"},
        {"low-stock", "Low stock (view v_low_stock)",
         "SELECT product_id, sku, product_name, stock_quantity, min_stock_level, stock_ratio, supplier_name FROM v_low_stock"},
    };
    return list;
}

Reply listReports(Ctx&) {
    json arr = json::array();
    for (const Report& r : reports()) arr.push_back({{"name", r.name}, {"title", r.title}});
    return okReply(arr);
}

Reply runReport(Ctx& c) {
    const std::string name = c.params.empty() ? "" : c.params[0];
    for (const Report& r : reports()) {
        if (name != r.name) continue;
        QueryResult res = c.app.db.execute(r.sql, {}, &c.log);
        return okReply({{"name", r.name}, {"title", r.title}, {"columns", res.columns}, {"rows", rowsToJson(res)}});
    }
    return errReply(404, "NOT_FOUND", "Unknown report");
}

// ---------- users (admin only) ----------

bool validRole(const std::string& r) { return r == "ADMIN" || r == "EMPLOYEE"; }

bool validUsername(const std::string& u) {
    if (u.size() < 3 || u.size() > 40) return false;
    for (char ch : u) if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_' && ch != '.' && ch != '-') return false;
    return true;
}

Reply listUsers(Ctx& c) {
    return okReply(rowsToJson(c.app.db.execute(
        "SELECT user_id, username, role, created_at FROM Users ORDER BY username", {}, &c.log)));
}

Reply createUser(Ctx& c) {
    std::string username, password, role;
    if (!jsonString(c.body, "username", username) || !validUsername(trimmed(username))) {
        return errReply(400, "BAD_REQUEST", "username must be 3-40 letters, digits, dot, dash or underscore");
    }
    if (!jsonString(c.body, "password", password) || password.size() < 8 || password.size() > 100) {
        return errReply(400, "BAD_REQUEST", "password must be 8 to 100 characters");
    }
    if (!jsonString(c.body, "role", role) || !validRole(role)) return errReply(400, "BAD_REQUEST", "role must be ADMIN or EMPLOYEE");
    WriteResult w = runStatement(c.app.db, "INSERT INTO Users (username, password_hash, role) VALUES (?, ?, ?)",
                                 {P(trimmed(username)), PSecret(hashPassword(password, makeSalt())), P(role)}, c.log);
    if (!w.ok) return writeFailure(w);
    return okReply({{"id", w.id}}, 201);
}

Reply updateUser(Ctx& c) {
    int id = c.paramInt(0);
    std::string role, password;
    bool hasRole = c.body.is_object() && c.body.contains("role");
    bool hasPass = c.body.is_object() && c.body.contains("password");
    if (!hasRole && !hasPass) return errReply(400, "BAD_REQUEST", "send role and/or password");
    std::string sets;
    Params params;
    if (hasRole) {
        if (!jsonString(c.body, "role", role) || !validRole(role)) return errReply(400, "BAD_REQUEST", "role must be ADMIN or EMPLOYEE");
        if (id == c.user->userId && role != c.user->role) return errReply(400, "BAD_REQUEST", "You cannot change your own role");
        sets += "role = ?";
        params.push_back(P(role));
    }
    if (hasPass) {
        if (!jsonString(c.body, "password", password) || password.size() < 8 || password.size() > 100) {
            return errReply(400, "BAD_REQUEST", "password must be 8 to 100 characters");
        }
        sets += std::string(sets.empty() ? "" : ", ") + "password_hash = ?";
        params.push_back(PSecret(hashPassword(password, makeSalt())));
    }
    params.push_back(P(id));
    WriteResult w = runStatement(c.app.db, "UPDATE Users SET " + sets + " WHERE user_id = ?", params, c.log);
    if (!w.ok) return writeFailure(w);
    if (w.affected == 0) return errReply(404, "NOT_FOUND", "User not found");
    c.app.sessions.destroyUser(id, id == c.user->userId ? c.token : "");   // role or password changed: sign them out
    return okReply({{"id", id}});
}

Reply deleteUser(Ctx& c) {
    int id = c.paramInt(0);
    if (id == c.user->userId) return errReply(400, "BAD_REQUEST", "You cannot delete your own account");
    WriteResult w = runStatement(c.app.db, "DELETE FROM Users WHERE user_id = ?", {P(id)}, c.log);
    if (!w.ok) return writeFailure(w);
    if (w.affected == 0) return errReply(404, "NOT_FOUND", "User not found");
    c.app.sessions.destroyUser(id);                                          // a deleted account must not keep working
    return okReply({{"deleted", id}});
}

// ---------- database model (from information_schema) ----------

Reply model(Ctx& c) {
    QueryResult cols = c.app.db.execute(
        "SELECT TABLE_NAME AS table_name, COLUMN_NAME AS column_name, COLUMN_TYPE AS column_type, "
        "IS_NULLABLE AS is_nullable, COLUMN_KEY AS column_key "
        "FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME IN "
        "(SELECT TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_TYPE = 'BASE TABLE') "
        "ORDER BY TABLE_NAME, ORDINAL_POSITION", {}, &c.log);
    QueryResult fks = c.app.db.execute(
        "SELECT TABLE_NAME AS table_name, COLUMN_NAME AS column_name, REFERENCED_TABLE_NAME AS ref_table, "
        "REFERENCED_COLUMN_NAME AS ref_column FROM information_schema.KEY_COLUMN_USAGE "
        "WHERE TABLE_SCHEMA = DATABASE() AND REFERENCED_TABLE_NAME IS NOT NULL", {}, &c.log);

    json tables = json::array();
    std::string current;
    for (const auto& row : cols.rows) {
        std::string table = *row[0];
        if (table != current) { tables.push_back({{"name", table}, {"columns", json::array()}}); current = table; }
        json refs = nullptr;
        for (const auto& fk : fks.rows) {
            if (*fk[0] == table && *fk[1] == *row[1]) refs = {{"table", *fk[2]}, {"column", *fk[3]}};
        }
        tables.back()["columns"].push_back({{"name", *row[1]}, {"type", *row[2]}, {"nullable", *row[3] == "YES"},
                                            {"key", row[4] ? *row[4] : ""}, {"references", refs}});
    }
    return okReply({{"tables", tables}});
}

}  // namespace

void registerReportRoutes(App& app) {
    app.add("GET", "/api/dashboard", Access::Any, dashboard);
    app.add("GET", "/api/transactions/recent", Access::Any, recentTransactions);
    app.add("GET", "/api/reports", Access::Any, listReports);
    app.add("GET", R"(/api/reports/([a-z-]+))", Access::Any, runReport);
    app.add("GET", "/api/users", Access::Admin, listUsers);
    app.add("POST", "/api/users", Access::Admin, createUser);
    app.add("PUT", R"(/api/users/(\d+))", Access::Admin, updateUser);
    app.add("DELETE", R"(/api/users/(\d+))", Access::Admin, deleteUser);
    app.add("GET", "/api/model", Access::Admin, model);
}
