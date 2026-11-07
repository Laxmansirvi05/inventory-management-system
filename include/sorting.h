// sorting.h : hand-written merge sort, linear search and binary search over products.
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "product.h"

using ProductLess = std::function<bool(const Product&, const Product&)>;

// Stable merge sort. O(n log n) time, O(n) extra space.
void mergeSort(std::vector<Product>& items, const ProductLess& less);

// Comparator for field "price", "qty" or "name" (name ignores case). Empty function if field is unknown.
ProductLess makeComparator(const std::string& field, bool ascending);

// Linear search: products whose name contains `text` (case-insensitive). O(n * m).
std::vector<Product> searchByName(const std::vector<Product>& items, const std::string& text);

// Linear search: products in one category. O(n).
std::vector<Product> searchByCategory(const std::vector<Product>& items, int categoryId);

// Binary search by id. Input MUST be sorted by id ascending. Returns index or -1. O(log n).
int binarySearchById(const std::vector<Product>& sortedById, int id);
