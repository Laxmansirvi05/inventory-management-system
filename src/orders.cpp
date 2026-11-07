// orders.cpp : FIFO order queue.
#include "orders.h"
#include <map>

void OrderQueue::enqueue(const Order& o) { queue_.push(o); }

std::vector<int> OrderQueue::pendingIds() const {
    std::vector<int> ids;
    std::queue<Order> copy = queue_;       // queues cannot be iterated, so walk a copy
    while (!copy.empty()) { ids.push_back(copy.front().orderId); copy.pop(); }
    return ids;
}

OrderResult OrderQueue::processNext(Inventory& inv) {
    OrderResult res;
    if (queue_.empty()) { res.message = "No pending orders"; return res; }
    Order o = queue_.front();              // oldest order first (FIFO)
    queue_.pop();
    res.orderId = o.orderId;

    // Pass 1: check every line (same product twice is added up) so we never sell half an order.
    std::map<int, int> wanted;
    for (const OrderLine& l : o.lines) {
        if (l.qty <= 0) { res.message = "Quantity must be greater than 0"; return res; }
        wanted[l.productId] += l.qty;
    }
    for (const auto& kv : wanted) {
        const Product* p = inv.findById(kv.first);
        if (!p) { res.message = "Product not found: " + std::to_string(kv.first); return res; }
        if (kv.second > p->qty) { res.message = "Insufficient stock for product " + std::to_string(kv.first); return res; }
    }
    // Pass 2: apply.
    for (const OrderLine& l : o.lines) inv.sell(l.productId, l.qty);
    res.ok = true;
    res.message = "OK";
    return res;
}

OrderResult OrderQueue::processNext(const std::function<OrderResult(const Order&)>& handler) {
    if (queue_.empty()) { OrderResult r; r.message = "No pending orders"; return r; }
    Order o = queue_.front();              // oldest order first (FIFO)
    queue_.pop();
    OrderResult r = handler(o);
    r.orderId = o.orderId;
    return r;
}
