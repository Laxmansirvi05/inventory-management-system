// undo.h : stack of stock transactions so the last one can be undone (LIFO).
#pragma once
#include <stack>
#include <string>
#include <vector>

struct StockTxn {
    int productId = 0;
    int delta = 0;          // +n = stock in, -n = stock out
    std::string kind;       // "PURCHASE" or "SALE"
    int docId = 0;          // sale_id or purchase_id this line belongs to (0 = not part of a document)
};

class UndoStack {
public:
    void push(const StockTxn& t);       // O(1)
    bool empty() const;                 // O(1)
    const StockTxn& top() const;        // O(1), call only when not empty
    void pop();                         // O(1)
    size_t size() const;                // O(1)
    // The newest sale or purchase: all of its lines (newest first). One entry when docId is 0. O(n) (copies the stack)
    std::vector<StockTxn> topGroup() const;
    void popGroup(size_t n);            // remove the n newest entries
private:
    std::stack<StockTxn> stack_;
};
