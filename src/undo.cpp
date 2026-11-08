// undo.cpp : stack of stock transactions (LIFO).
#include "undo.h"

void UndoStack::push(const StockTxn& t) { stack_.push(t); }
bool UndoStack::empty() const { return stack_.empty(); }
const StockTxn& UndoStack::top() const { return stack_.top(); }
void UndoStack::pop() { stack_.pop(); }
size_t UndoStack::size() const { return stack_.size(); }

std::vector<StockTxn> UndoStack::topGroup() const {
    std::vector<StockTxn> group;
    if (stack_.empty()) return group;
    std::stack<StockTxn> copy = stack_;                      // std::stack cannot be walked, so walk a copy
    const StockTxn first = copy.top();
    group.push_back(first);
    copy.pop();
    while (first.docId > 0 && !copy.empty() && copy.top().docId == first.docId && copy.top().kind == first.kind) {
        group.push_back(copy.top());
        copy.pop();
    }
    return group;
}

void UndoStack::popGroup(size_t n) {
    while (n-- > 0 && !stack_.empty()) stack_.pop();
}
