// restock.cpp : build the low-stock priority queue.
#include "restock.h"

// Products at or below their minimum enter the queue; lowest qty/minStock comes out first. O(n log n)
AlertQueue buildAlerts(const std::vector<Product>& products) {
    AlertQueue q;
    for (const Product& p : products) {
        if (p.qty > p.minStock) continue;                 // stock is fine
        LowStockItem item;
        item.productId = p.id;
        item.name = p.name;
        item.qty = p.qty;
        item.minStock = p.minStock;
        // minStock 0 and qty 0 means "out of stock with no threshold": most urgent
        item.ratio = p.minStock > 0 ? static_cast<double>(p.qty) / p.minStock : 0.0;
        q.push(item);
    }
    return q;
}

// Pop everything from a copy of the queue into a vector, most urgent first. O(k log k)
std::vector<LowStockItem> alertsInOrder(AlertQueue queue) {
    std::vector<LowStockItem> out;
    out.reserve(queue.size());
    while (!queue.empty()) {
        out.push_back(queue.top());
        queue.pop();
    }
    return out;
}
