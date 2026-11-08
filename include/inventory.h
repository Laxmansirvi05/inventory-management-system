// inventory.h : in-memory product store built from standard C++ data structures.
#pragma once
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "product.h"
#include "restock.h"
#include "undo.h"

// Outcome of a stock change.
struct TxnResult {
    bool ok = false;
    std::string message;
    int productId = 0;
    int before = 0;     // quantity before the change
    int after = 0;      // quantity after the change
};

class Inventory {
public:
    Inventory() = default;
    // The indexes hold pointers into items_, so a copy must rebuild them (a moved vector keeps its addresses).
    Inventory(const Inventory& o) : items_(o.items_), undo_(o.undo_) { rebuildIndexes(); }
    Inventory& operator=(const Inventory& o) {
        if (this != &o) { items_ = o.items_; undo_ = o.undo_; rebuildIndexes(); }
        return *this;
    }
    Inventory(Inventory&&) = default;
    Inventory& operator=(Inventory&&) = default;

    // Replace every product (used at start-up and to re-sync from MySQL). Keeps the undo stack. O(n log n)
    void replaceAll(std::vector<Product> products) { items_ = std::move(products); rebuildIndexes(); }

    // ---- product records ----
    bool addProduct(const Product& p, std::string& error);      // rejects duplicate id or SKU (hash set)
    bool updateProduct(const Product& p, std::string& error);   // replaces the record with the same id
    bool removeProduct(int id);
    Product* findById(int id);                                  // hash map, O(1) average
    const Product* findById(int id) const;
    bool hasSku(const std::string& sku) const;                  // hash set, O(1) average
    const std::vector<Product>& all() const { return items_; }  // array list
    std::vector<Product> inIdOrder() const;                     // tree map walk, O(n)
    size_t size() const { return items_.size(); }

    // ---- stock logic ----
    TxnResult purchase(int productId, int qty, int docId = 0);  // stock in (docId = purchase_id, for undo)
    TxnResult sell(int productId, int qty, int docId = 0);      // stock out, rule: qty <= stock (docId = sale_id)
    TxnResult undoLast();                                       // reverse the newest transaction (stack)
    size_t undoDepth() const { return undo_.size(); }
    std::vector<StockTxn> undoGroup() const { return undo_.topGroup(); }   // all lines of the newest sale or purchase
    TxnResult undoGroupApply();                                 // reverse every line of that group in memory, all or nothing
    void clearUndo() { undo_ = UndoStack(); }
    bool peekUndo(StockTxn& out) const {                        // newest transaction without removing it
        if (undo_.empty()) return false;
        out = undo_.top();
        return true;
    }

    // ---- alerts ----
    std::vector<LowStockItem> lowStockAlerts() const;           // most urgent first (priority queue)

private:
    void rebuildIndexes();                                      // O(n log n): pointers move when the vector grows
    TxnResult change(int productId, int delta, const std::string& kind, bool recordUndo, int docId = 0);

    std::vector<Product> items_;                                // ArrayList
    std::unordered_map<int, Product*> byId_;                    // HashMap
    std::unordered_set<std::string> skus_;                      // HashSet
    std::map<int, Product*> ordered_;                           // TreeMap (id order)
    AlertQueue alerts_;                                         // PriorityQueue, rebuilt after every change
    UndoStack undo_;                                            // Stack
};
