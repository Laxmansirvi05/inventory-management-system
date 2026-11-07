// orders.h : customer orders processed first-in first-out (queue).
#pragma once
#include <functional>
#include <queue>
#include <string>
#include <vector>
#include "inventory.h"

struct OrderLine {
    int productId = 0;
    int qty = 0;
};

struct Order {
    int orderId = 0;
    int customerId = 0;
    std::vector<OrderLine> lines;
};

struct OrderResult {
    bool ok = false;
    int orderId = 0;
    std::string message;
};

class OrderQueue {
public:
    void enqueue(const Order& o);                 // O(1)
    bool empty() const { return queue_.empty(); }
    size_t size() const { return queue_.size(); }
    std::vector<int> pendingIds() const;          // oldest first, O(n) (copy of the queue)
    // Take the oldest order and apply it to the inventory. All-or-nothing: if any line
    // cannot be served, nothing is sold. The order leaves the queue either way.
    OrderResult processNext(Inventory& inv);
    // Same FIFO rule, but the caller decides how to apply the order (the server passes a function that writes to MySQL).
    OrderResult processNext(const std::function<OrderResult(const Order&)>& handler);
private:
    std::queue<Order> queue_;
};
