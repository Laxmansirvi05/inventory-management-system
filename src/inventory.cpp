// inventory.cpp : product records, stock changes (Phase 2 and 3) and low-stock alerts.
#include "inventory.h"

// Rebuild the pointer-based indexes. The vector may move its elements when it grows,
// so every add/remove calls this. O(n log n)
void Inventory::rebuildIndexes() {
    byId_.clear();
    ordered_.clear();
    skus_.clear();
    for (Product& p : items_) {
        byId_[p.id] = &p;
        ordered_[p.id] = &p;
        skus_.insert(p.sku);
    }
    alerts_ = buildAlerts(items_);
}

bool Inventory::addProduct(const Product& p, std::string& error) {
    if (byId_.count(p.id)) { error = "Product id already exists"; return false; }
    if (skus_.count(p.sku)) { error = "Duplicate SKU"; return false; }       // hash set check
    if (p.price <= 0)      { error = "Price must be greater than 0"; return false; }
    if (p.qty < 0)         { error = "Stock cannot be negative"; return false; }
    items_.push_back(p);                                                      // amortised O(1)
    rebuildIndexes();
    return true;
}

bool Inventory::updateProduct(const Product& p, std::string& error) {
    Product* cur = findById(p.id);
    if (!cur) { error = "Product not found"; return false; }
    if (p.sku != cur->sku && skus_.count(p.sku)) { error = "Duplicate SKU"; return false; }
    if (p.price <= 0) { error = "Price must be greater than 0"; return false; }
    if (p.qty < 0)    { error = "Stock cannot be negative"; return false; }
    *cur = p;
    rebuildIndexes();
    return true;
}

bool Inventory::removeProduct(int id) {
    for (size_t i = 0; i < items_.size(); ++i) {     // linear scan, then erase: O(n)
        if (items_[i].id == id) {
            items_.erase(items_.begin() + static_cast<long>(i));
            rebuildIndexes();
            return true;
        }
    }
    return false;
}

Product* Inventory::findById(int id) {
    auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}

const Product* Inventory::findById(int id) const {
    auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}

bool Inventory::hasSku(const std::string& sku) const { return skus_.count(sku) > 0; }

std::vector<Product> Inventory::inIdOrder() const {
    std::vector<Product> out;
    out.reserve(ordered_.size());
    for (const auto& kv : ordered_) out.push_back(*kv.second);   // map iterates in key order
    return out;
}

// Shared stock change. delta > 0 adds stock, delta < 0 removes it. Never lets stock go below 0.
TxnResult Inventory::change(int productId, int delta, const std::string& kind, bool recordUndo, int docId) {
    TxnResult r;
    r.productId = productId;
    Product* p = findById(productId);
    if (!p) { r.message = "Product not found"; return r; }
    r.before = p->qty;
    if (delta == 0) { r.message = "Quantity must be greater than 0"; r.after = p->qty; return r; }
    if (p->qty + delta < 0) {
        r.message = "Insufficient stock for product " + std::to_string(productId);
        r.after = p->qty;
        return r;
    }
    p->qty += delta;
    r.after = p->qty;
    r.ok = true;
    r.message = "OK";
    if (recordUndo) undo_.push(StockTxn{productId, delta, kind, docId});   // O(1)
    alerts_ = buildAlerts(items_);                                  // re-evaluate low-stock queue
    return r;
}

TxnResult Inventory::purchase(int productId, int qty, int docId) {
    if (qty <= 0) { TxnResult r; r.productId = productId; r.message = "Quantity must be greater than 0"; return r; }
    return change(productId, qty, "PURCHASE", true, docId);
}

TxnResult Inventory::sell(int productId, int qty, int docId) {
    if (qty <= 0) { TxnResult r; r.productId = productId; r.message = "Quantity must be greater than 0"; return r; }
    return change(productId, -qty, "SALE", true, docId);
}

// Reverse the newest transaction. If reversing would make stock negative (stock was sold since),
// it is refused and the transaction stays on the stack.
TxnResult Inventory::undoLast() {
    if (undo_.empty()) { TxnResult r; r.message = "Nothing to undo"; return r; }
    StockTxn t = undo_.top();
    TxnResult r = change(t.productId, -t.delta, "UNDO", false);
    if (r.ok) undo_.pop();
    return r;
}

std::vector<LowStockItem> Inventory::lowStockAlerts() const { return alertsInOrder(alerts_); }

// Reverse the newest sale or purchase as a whole: every line or none. Stock may never go negative.
TxnResult Inventory::undoGroupApply() {
    TxnResult r;
    std::vector<StockTxn> group = undo_.topGroup();
    if (group.empty()) { r.message = "Nothing to undo"; return r; }
    for (const StockTxn& t : group) {                                   // check first, change afterwards
        const Product* p = findById(t.productId);
        if (!p || p->qty - t.delta < 0) {
            r.productId = t.productId;
            r.message = "Insufficient stock to undo for product " + std::to_string(t.productId);
            return r;
        }
    }
    for (const StockTxn& t : group) r = change(t.productId, -t.delta, "UNDO", false);
    undo_.popGroup(group.size());
    return r;
}
