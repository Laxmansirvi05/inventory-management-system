// undo.cpp : stack of stock transactions (LIFO).
#include "undo.h"

void UndoStack::push(const StockTxn& t) { stack_.push(t); }
bool UndoStack::empty() const { return stack_.empty(); }
const StockTxn& UndoStack::top() const { return stack_.top(); }
void UndoStack::pop() { stack_.pop(); }
size_t UndoStack::size() const { return stack_.size(); }
