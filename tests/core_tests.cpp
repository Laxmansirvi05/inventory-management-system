// core_tests.cpp : console tests for Phase 2 (data structures, algorithms) and Phase 3 (stock logic).
// No database needed. Run: build/core_tests   (exit code 0 = all passed)
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include "fairlock.h"
#include "inventory.h"
#include "orders.h"
#include "sorting.h"

static int failures = 0;
static int checks = 0;
#define CHECK(cond, label) do { ++checks; if (cond) std::cout << "  PASS  " << label << "\n"; \
    else { ++failures; std::cout << "  FAIL  " << label << "   (" #cond ")\n"; } } while (0)

static Product mk(int id, const char* name, int cat, double price, int qty, int minStock) {
    Product p;
    p.id = id; p.sku = "SKU" + std::to_string(id); p.name = name;
    p.categoryId = cat; p.supplierId = 1; p.price = price; p.qty = qty; p.minStock = minStock;
    return p;
}

// Ten products, deliberately NOT in id order, to prove sorting and ordered views work.
static Inventory makeSample() {
    Inventory inv;
    std::string err;
    inv.addProduct(mk(8, "Mouse Wireless", 2, 599, 40, 10), err);
    inv.addProduct(mk(3, "gel pen", 1, 20, 100, 40), err);
    inv.addProduct(mk(1, "Ball Pen Blue", 1, 10, 200, 50), err);
    inv.addProduct(mk(10, "USB Cable", 2, 199, 15, 20), err);
    inv.addProduct(mk(5, "Notebook", 1, 60, 80, 40), err);
    inv.addProduct(mk(2, "Basmati Rice", 3, 110, 60, 40), err);
    inv.addProduct(mk(7, "Power Bank", 2, 1199, 8, 8), err);
    inv.addProduct(mk(4, "Stapler", 1, 85, 30, 10), err);
    inv.addProduct(mk(9, "Cola", 4, 40, 10, 60), err);
    inv.addProduct(mk(6, "Keyboard", 2, 699, 25, 8), err);
    return inv;
}

static void testRecords() {
    std::cout << "[records, hash set, hash map, tree map]\n";
    Inventory inv = makeSample();
    std::string err;
    CHECK(inv.size() == 10, "ten products added");
    Product dup = mk(99, "Clone", 1, 5, 1, 1);
    dup.sku = "SKU3";
    CHECK(!inv.addProduct(dup, err) && err == "Duplicate SKU", "duplicate SKU rejected by hash set");
    CHECK(!inv.addProduct(mk(3, "Same id", 1, 5, 1, 1), err), "duplicate id rejected");
    CHECK(!inv.addProduct(mk(98, "Free", 1, -5, 1, 1), err), "negative price rejected");
    CHECK(inv.findById(8) && inv.findById(8)->name == "Mouse Wireless", "hash map finds id 8");
    CHECK(inv.findById(77) == nullptr, "hash map returns null for missing id");
    std::vector<Product> ord = inv.inIdOrder();
    bool inOrder = true;
    for (size_t i = 1; i < ord.size(); ++i) if (ord[i - 1].id >= ord[i].id) inOrder = false;
    CHECK(inOrder && ord.front().id == 1 && ord.back().id == 10, "tree map lists products in id order");
    Product upd = *inv.findById(3);
    upd.price = 25;
    CHECK(inv.updateProduct(upd, err) && inv.findById(3)->price == 25, "update product");
    CHECK(inv.removeProduct(3) && inv.findById(3) == nullptr && !inv.hasSku("SKU3"), "remove product clears indexes");
    CHECK(inv.findById(8) && inv.findById(8)->id == 8, "pointers still valid after remove");
}

static void testSortSearch() {
    std::cout << "[merge sort, linear search, binary search]\n";
    Inventory inv = makeSample();
    std::vector<Product> v = inv.all();

    mergeSort(v, makeComparator("price", true));
    bool asc = true;
    for (size_t i = 1; i < v.size(); ++i) if (v[i - 1].price > v[i].price) asc = false;
    CHECK(asc && v.front().price == 10 && v.back().price == 1199, "sort by price ascending");

    mergeSort(v, makeComparator("price", false));
    CHECK(v.front().price == 1199 && v.back().price == 10, "sort by price descending");

    mergeSort(v, makeComparator("qty", true));
    CHECK(v.front().qty == 8 && v.back().qty == 200, "sort by quantity");

    mergeSort(v, makeComparator("name", true));
    CHECK(v.front().name == "Ball Pen Blue" && v[3].name == "gel pen", "sort by name ignores case (lowercase 'gel pen' sorts after 'Cola')");

    std::vector<Product> six = {mk(1, "a", 1, 5, 1, 1), mk(2, "b", 1, 5, 1, 1), mk(3, "c", 1, 3, 1, 1),
                                mk(4, "d", 1, 5, 1, 1), mk(5, "e", 1, 1, 1, 1), mk(6, "f", 1, 3, 1, 1)};
    mergeSort(six, makeComparator("price", true));
    CHECK(six[0].id == 5 && six[1].id == 3 && six[2].id == 6 && six[3].id == 1 && six[4].id == 2 && six[5].id == 4,
          "merge sort is stable (6-element example)");
    std::vector<Product> empty;
    mergeSort(empty, makeComparator("price", true));
    CHECK(empty.empty(), "sorting an empty list is safe");
    CHECK(!makeComparator("colour", true), "unknown sort field gives empty comparator");

    std::vector<Product> byId = inv.inIdOrder();
    int idx = binarySearchById(byId, 8);
    CHECK(idx >= 0 && byId[idx].id == 8, "binary search finds id 8 in sorted list");
    CHECK(binarySearchById(byId, 11) == -1 && binarySearchById(byId, 0) == -1, "binary search misses absent ids");

    CHECK(searchByName(inv.all(), "PEN").size() == 2, "linear search by name is case-insensitive (2 pens)");
    CHECK(searchByName(inv.all(), "zzz").empty(), "linear search with no match is empty");
    CHECK(searchByCategory(inv.all(), 2).size() == 4, "linear search by category (4 electronics)");
    CHECK(searchByName(inv.all(), "'; DROP TABLE Products;--").empty(), "injection text is plain text");
}

static void testStock() {
    std::cout << "[stock logic, undo stack, alerts]\n";
    Inventory inv;
    std::string err;
    inv.addProduct(mk(1, "Widget", 1, 10, 100, 20), err);
    TxnResult a = inv.purchase(1, 50);
    CHECK(a.ok && a.before == 100 && a.after == 150, "purchase 100 + 50 = 150");
    TxnResult b = inv.sell(1, 30);
    CHECK(b.ok && b.before == 150 && b.after == 120, "sale 150 - 30 = 120 (ADS outline example)");
    CHECK(inv.lowStockAlerts().empty(), "120 is above minimum 20: no alert");
    TxnResult c = inv.sell(1, 105);
    CHECK(c.ok && c.after == 15, "sell 105 -> 15");
    std::vector<LowStockItem> al = inv.lowStockAlerts();
    CHECK(al.size() == 1 && al[0].productId == 1, "15 is below minimum 20: alert raised");
    TxnResult d = inv.sell(1, 16);
    CHECK(!d.ok && d.after == 15 && inv.findById(1)->qty == 15, "oversell refused, stock unchanged");
    CHECK(!inv.sell(1, 0).ok && !inv.purchase(1, -3).ok, "zero or negative quantity refused");
    CHECK(!inv.sell(42, 1).ok, "unknown product refused");

    CHECK(inv.undoDepth() == 3, "three transactions on the undo stack");
    TxnResult u = inv.undoLast();
    CHECK(u.ok && inv.findById(1)->qty == 120, "undo restores quantity (15 -> 120)");
    u = inv.undoLast();
    CHECK(u.ok && inv.findById(1)->qty == 150, "undo is last-in first-out (120 -> 150)");
    CHECK(inv.undoDepth() == 1, "one transaction (the purchase of 50) left on the stack");
    Product shrunk = *inv.findById(1);
    shrunk.qty = 10;                                    // stock edited by hand: 10 < 50
    inv.updateProduct(shrunk, err);
    TxnResult refused = inv.undoLast();
    CHECK(!refused.ok && inv.findById(1)->qty == 10 && inv.undoDepth() == 1,
          "undo that would make stock negative is refused and stays on the stack");
    Inventory empty;
    CHECK(!empty.undoLast().ok, "undo with empty stack is refused");

    Inventory guard;
    guard.addProduct(mk(1, "Widget", 1, 10, 0, 5), err);
    guard.purchase(1, 10);
    guard.sell(1, 4);
    Inventory g2 = guard;                               // copy: pointers must be rebuilt, not shared
    g2.findById(1)->qty = 100;
    CHECK(guard.findById(1)->qty == 6, "copies of Inventory do not share state");
}

static void testAlertOrder() {
    std::cout << "[priority queue order]\n";
    Inventory inv = makeSample();
    // ratios: Cola 10/60=.17, USB Cable 15/20=.75, Power Bank 8/8=1.0 (gel pen 100/40 is fine)
    std::vector<LowStockItem> al = inv.lowStockAlerts();
    CHECK(al.size() == 3, "three low-stock products in sample data");
    CHECK(al.size() == 3 && al[0].name == "Cola" && al[1].name == "USB Cable" && al[2].name == "Power Bank",
          "alerts list lowest ratio first");
    inv.sell(7, 8);                                     // Power Bank to 0 -> ratio 0, most urgent
    al = inv.lowStockAlerts();
    CHECK(!al.empty() && al[0].name == "Power Bank", "after selling out, Power Bank jumps to the top");
    inv.purchase(9, 100);                               // Cola restocked -> leaves the list
    al = inv.lowStockAlerts();
    CHECK(al.size() == 2, "restocked product leaves the alert list");
}

static void testOrders() {
    std::cout << "[FIFO order queue]\n";
    Inventory inv = makeSample();
    OrderQueue q;
    q.enqueue(Order{101, 1, {{1, 5}}});
    q.enqueue(Order{102, 2, {{5, 10}, {4, 2}}});
    q.enqueue(Order{103, 3, {{6, 1}}});
    CHECK(q.size() == 3 && q.pendingIds() == std::vector<int>({101, 102, 103}), "three orders waiting in arrival order");
    OrderResult r1 = q.processNext(inv);
    OrderResult r2 = q.processNext(inv);
    OrderResult r3 = q.processNext(inv);
    CHECK(r1.orderId == 101 && r2.orderId == 102 && r3.orderId == 103, "processed in arrival order (FIFO)");
    CHECK(r1.ok && r2.ok && r3.ok && q.empty(), "all three succeeded, queue empty");
    CHECK(inv.findById(1)->qty == 195 && inv.findById(5)->qty == 70 && inv.findById(4)->qty == 28, "stock reduced by orders");
    CHECK(!q.processNext(inv).ok, "processing an empty queue fails gracefully");

    q.enqueue(Order{201, 1, {{1, 1}, {7, 999}}});       // second line cannot be served
    q.enqueue(Order{202, 1, {{1, 1}}});
    int before = inv.findById(1)->qty;
    OrderResult bad = q.processNext(inv);
    CHECK(!bad.ok && bad.orderId == 201 && inv.findById(1)->qty == before, "order with one bad line sells nothing (all-or-nothing)");
    OrderResult good = q.processNext(inv);
    CHECK(good.ok && good.orderId == 202, "next order still processed after a failed one");
    q.enqueue(Order{203, 1, {{9, 6}, {9, 6}}});         // same product twice: 12 > stock 10
    CHECK(!q.processNext(inv).ok, "same product on two lines is added up before checking");
}

static void testGroupUndo() {
    std::cout << "[undo of a whole sale or purchase]\n";
    Inventory inv;
    std::string err;
    inv.addProduct(mk(1, "A", 1, 10, 100, 5), err);
    inv.addProduct(mk(2, "B", 1, 10, 100, 5), err);
    inv.sell(1, 10, 7);                                 // sale 7 has two lines
    inv.sell(2, 20, 7);
    inv.purchase(1, 5, 8);                              // purchase 8 has one line
    std::vector<StockTxn> g = inv.undoGroup();
    CHECK(g.size() == 1 && g[0].kind == "PURCHASE" && g[0].docId == 8, "newest group is the purchase (one line)");
    CHECK(inv.undoGroupApply().ok && inv.findById(1)->qty == 90, "undoing the purchase takes its 5 back out (95 -> 90)");
    g = inv.undoGroup();
    CHECK(g.size() == 2 && g[0].docId == 7 && g[1].docId == 7, "next group is the sale with BOTH lines");
    CHECK(inv.undoGroupApply().ok && inv.findById(1)->qty == 100 && inv.findById(2)->qty == 100 && inv.undoDepth() == 0, "undoing the sale restores both products and empties the stack");
    CHECK(!inv.undoGroupApply().ok && inv.undoGroup().empty(), "nothing left to undo");
    inv.purchase(1, 50, 9);
    inv.sell(1, 120, 10);                               // stock 30: the purchase cannot be undone after the sale... undo the sale first
    CHECK(inv.undoGroupApply().ok && inv.findById(1)->qty == 150, "stack order: the sale is undone first");
    Product low = *inv.findById(1);
    low.qty = 10;                                       // stock changed outside the stack
    inv.updateProduct(low, err);
    TxnResult r = inv.undoGroupApply();                 // purchase of 50 would need stock >= 50
    CHECK(!r.ok && inv.findById(1)->qty == 10 && inv.undoDepth() == 1, "undo that would make stock negative is refused and leaves everything as it was");
    inv.clearUndo();
    CHECK(inv.undoDepth() == 0, "clearUndo empties the history");
}

static void testFairLock() {
    std::cout << "[fair lock: first come, first served]\n";
    FairMutex m;
    std::vector<int> served;
    m.lock();                                           // hold the lock while eight threads queue up, 20 ms apart
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&, i] { std::lock_guard<FairMutex> g(m); served.push_back(i); });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    m.unlock();
    for (std::thread& t : threads) t.join();
    bool inOrder = served.size() == 8;
    for (int i = 0; i < static_cast<int>(served.size()); ++i) if (served[static_cast<size_t>(i)] != i) inOrder = false;
    CHECK(inOrder, "eight waiting threads were served in the order they arrived");
    int counter = 0;                                    // and the lock still protects shared data
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back([&] { for (int k = 0; k < 1000; ++k) { std::lock_guard<FairMutex> g(m); ++counter; } });
    for (std::thread& t : workers) t.join();
    CHECK(counter == 4000, "4 threads x 1000 increments = 4000 (mutual exclusion holds)");
}

int main() {
    testRecords();
    testSortSearch();
    testStock();
    testAlertOrder();
    testOrders();
    testGroupUndo();
    testFairLock();
    std::cout << "\n" << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
