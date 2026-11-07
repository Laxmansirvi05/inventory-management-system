// routes_stock.cpp : purchases, sales (FIFO order queue), bill, undo, low-stock alerts.
#include "server.h"

namespace {

const size_t kMaxLines = 100;
const size_t kHistoryKept = 20;

// Read "items": [{product_id, quantity, ...}]. Returns an error message or "".
std::string readItems(const json& body, std::vector<json>& out) {
    if (!body.is_object() || !body.contains("items") || !body["items"].is_array() || body["items"].empty()) {
        return "items must be a non-empty list";
    }
    if (body["items"].size() > kMaxLines) return "too many items in one request";
    for (const json& it : body["items"]) {
        if (!it.is_object()) return "each item must be an object";
        out.push_back(it);
    }
    return "";
}

// ---------- purchases ----------

Reply createPurchase(Ctx& c) {
    int supplierId = 0;
    if (!jsonInt(c.body, "supplier_id", supplierId)) return errReply(400, "BAD_REQUEST", "supplier_id is required");
    std::vector<json> items;
    std::string bad = readItems(c.body, items);
    if (!bad.empty()) return errReply(400, "BAD_REQUEST", bad);

    std::vector<PurchaseLine> lines;
    for (const json& it : items) {
        PurchaseLine l;
        if (!jsonInt(it, "product_id", l.productId) || !jsonInt(it, "quantity", l.qty) || !jsonNumber(it, "unit_cost", l.unitCost)) {
            return errReply(400, "BAD_REQUEST", "each item needs product_id, quantity and unit_cost");
        }
        lines.push_back(l);
    }
    WriteResult w = recordPurchase(c.app.db, c.app.inv, supplierId, lines, c.log);
    if (!w.ok) return writeFailure(w);
    return okReply({{"purchase_id", w.id}, {"total", w.total}}, 201);
}

Reply listPurchases(Ctx& c) {
    return okReply(rowsToJson(c.app.db.execute(
        "SELECT p.purchase_id, p.purchase_date, s.supplier_name, p.total_amount, COUNT(pi.product_id) AS item_lines "
        "FROM Purchases p JOIN Suppliers s ON s.supplier_id = p.supplier_id "
        "LEFT JOIN Purchase_Items pi ON pi.purchase_id = p.purchase_id "
        "GROUP BY p.purchase_id, p.purchase_date, s.supplier_name, p.total_amount "
        "ORDER BY p.purchase_id DESC LIMIT 50", {}, &c.log)));
}

// ---------- sales ----------

OrderRecord* findRecord(App& app, int orderId) {
    for (OrderRecord& r : app.orderHistory) if (r.orderId == orderId) return &r;
    return nullptr;
}

// POST /api/sales : the order joins the FIFO queue, then the oldest order is taken and written to MySQL.
Reply createSale(Ctx& c) {
    App& app = c.app;
    Order order;
    order.customerId = 1;                                       // customer 1 = Walk-in Customer
    if (c.body.is_object() && c.body.contains("customer_id") && !jsonInt(c.body, "customer_id", order.customerId)) {
        return errReply(400, "BAD_REQUEST", "customer_id must be a number");
    }
    std::vector<json> items;
    std::string bad = readItems(c.body, items);
    if (!bad.empty()) return errReply(400, "BAD_REQUEST", bad);
    for (const json& it : items) {
        OrderLine l;
        if (!jsonInt(it, "product_id", l.productId) || !jsonInt(it, "quantity", l.qty)) {
            return errReply(400, "BAD_REQUEST", "each item needs product_id and quantity");
        }
        order.lines.push_back(l);
    }

    order.orderId = app.nextOrderId++;                           // numbered only once the request is valid
    app.orders.enqueue(order);                                   // queue: first in ...
    OrderRecord rec;
    rec.orderId = order.orderId; rec.customerId = order.customerId; rec.itemCount = static_cast<int>(order.lines.size());
    rec.status = "pending";
    app.orderHistory.push_back(rec);
    while (app.orderHistory.size() > kHistoryKept) app.orderHistory.pop_front();

    WriteResult mine;
    while (!app.orders.empty()) {                                // ... first out
        app.orders.processNext([&](const Order& o) {
            WriteResult w = recordSale(app.db, app.inv, o.customerId, o.lines, c.log);
            if (o.orderId == order.orderId) mine = w;
            if (OrderRecord* r = findRecord(app, o.orderId)) {
                r->status = w.ok ? "done" : "failed";
                r->message = w.ok ? "OK" : w.error;
                r->saleId = w.ok ? w.id : 0;
                r->total = w.total;
            }
            OrderResult res;
            res.ok = w.ok;
            res.message = w.ok ? "OK" : w.error;
            return res;
        });
    }
    if (!mine.ok) return writeFailure(mine);
    return okReply({{"sale_id", mine.id}, {"total", mine.total}, {"order_id", order.orderId}}, 201);
}

Reply listSales(Ctx& c) {
    return okReply(rowsToJson(c.app.db.execute(
        "SELECT sale_id, sale_date, customer_name, total_amount, item_lines, units_sold "
        "FROM v_sales_summary ORDER BY sale_id DESC LIMIT 50", {}, &c.log)));
}

Reply orderQueue(Ctx& c) {
    json arr = json::array();
    for (const OrderRecord& r : c.app.orderHistory) {
        arr.push_back({{"order_id", r.orderId}, {"customer_id", r.customerId}, {"items", r.itemCount},
                       {"status", r.status}, {"message", r.message},
                       {"sale_id", r.saleId ? json(r.saleId) : json(nullptr)}, {"total", r.total}});
    }
    return okReply({{"orders", arr}, {"pending", c.app.orders.size()}});
}

Reply saleBill(Ctx& c) {
    int id = c.paramInt(0);
    QueryResult head = c.app.db.execute(
        "SELECT s.sale_id, s.sale_date, s.total_amount, c.customer_name, c.phone "
        "FROM Sales s JOIN Customers c ON c.customer_id = s.customer_id WHERE s.sale_id = ?", {P(id)}, &c.log);
    if (head.rows.empty()) return errReply(404, "NOT_FOUND", "Sale not found");
    QueryResult lines = c.app.db.execute(
        "SELECT p.sku, p.product_name, si.quantity, si.unit_price, si.quantity * si.unit_price AS line_total "
        "FROM Sale_Items si JOIN Products p ON p.product_id = si.product_id "
        "WHERE si.sale_id = ? ORDER BY p.product_name", {P(id)}, &c.log);
    return okReply({{"shop", c.app.shopName}, {"sale", rowsToJson(head)[0]}, {"lines", rowsToJson(lines)}});
}

// ---------- undo and alerts ----------

Reply undo(Ctx& c) {
    WriteResult w = undoLastTxn(c.app.db, c.app.inv, c.log);
    if (!w.ok) return writeFailure(w);
    const Product* p = c.app.inv.findById(static_cast<int>(w.id));
    return okReply({{"product_id", w.id}, {"stock_quantity", p ? p->qty : 0}, {"undo_depth", c.app.inv.undoDepth()}});
}

// GET /api/alerts/low-stock : priority queue, most urgent first (served from memory)
Reply lowStock(Ctx& c) {
    json arr = json::array();
    for (const LowStockItem& it : c.app.inv.lowStockAlerts()) {
        const Product* p = c.app.inv.findById(it.productId);
        auto sup = p ? c.app.supplierNames.find(p->supplierId) : c.app.supplierNames.end();
        arr.push_back({{"product_id", it.productId}, {"sku", p ? p->sku : ""}, {"name", it.name},
                       {"stock_quantity", it.qty}, {"min_stock_level", it.minStock}, {"ratio", it.ratio},
                       {"supplier_name", sup == c.app.supplierNames.end() ? "" : sup->second},
                       {"status", it.qty * 2 <= it.minStock ? "Critical" : "Low"}});
    }
    return okReply(arr);
}

}  // namespace

void registerStockRoutes(App& app) {
    app.add("POST", "/api/purchases", Access::Any, createPurchase);
    app.add("GET", "/api/purchases", Access::Any, listPurchases);
    app.add("POST", "/api/sales", Access::Any, createSale);
    app.add("GET", "/api/sales", Access::Any, listSales);
    app.add("GET", "/api/sales/queue", Access::Any, orderQueue);
    app.add("GET", R"(/api/sales/(\d+)/bill)", Access::Any, saleBill);
    app.add("POST", "/api/undo", Access::Admin, undo);
    app.add("GET", "/api/alerts/low-stock", Access::Any, lowStock);
}
