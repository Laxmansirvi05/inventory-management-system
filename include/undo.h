// undo.h : stack of stock transactions so the last one can be undone (LIFO).
#pragma once
#include <stack>
#include <string>

struct StockTxn {
    int productId = 0;
    int delta = 0;          // +n = stock in, -n = stock out
    std::string kind;       // "PURCHASE" or "SALE"
};

class UndoStack {
public:
    void push(const StockTxn& t);       // O(1)
    bool empty() const;                 // O(1)
    const StockTxn& top() const;        // O(1), call only when not empty
    void pop();                         // O(1)
    size_t size() const;                // O(1)
private:
    std::stack<StockTxn> stack_;
};
