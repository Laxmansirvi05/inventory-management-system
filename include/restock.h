// restock.h : low-stock alerts, most urgent first (priority queue).
#pragma once
#include <queue>
#include <string>
#include <vector>
#include "product.h"

struct LowStockItem {
    int productId = 0;
    std::string name;
    int qty = 0;
    int minStock = 0;
    double ratio = 0.0;     // qty / minStock, smaller = more urgent

    // std::priority_queue keeps the "largest" on top, so "less" means *less urgent*.
    bool operator<(const LowStockItem& o) const {
        if (ratio != o.ratio) return ratio > o.ratio;
        return productId > o.productId;     // tie: lower id first
    }
};

using AlertQueue = std::priority_queue<LowStockItem>;

AlertQueue buildAlerts(const std::vector<Product>& products);             // O(n log n)
std::vector<LowStockItem> alertsInOrder(AlertQueue queue);                // O(k log k), copy is drained
