// sorting.cpp : merge sort, linear search and binary search written by hand (no std::sort / std::find).
#include "sorting.h"
#include <cctype>

namespace {

// Lower-case copy used for case-insensitive comparison. O(m)
std::string lower(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

// Merge the two sorted halves [lo,mid) and [mid,hi) into tmp, then copy back. O(hi-lo)
// Taking from the right only when it is strictly smaller keeps equal items in original order (stable).
void mergeHalves(std::vector<Product>& a, std::vector<Product>& tmp,
                 size_t lo, size_t mid, size_t hi, const ProductLess& less) {
    size_t i = lo, j = mid, k = lo;
    while (i < mid && j < hi) {
        if (less(a[j], a[i])) tmp[k++] = a[j++];
        else                  tmp[k++] = a[i++];
    }
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi)  tmp[k++] = a[j++];
    for (size_t x = lo; x < hi; ++x) a[x] = tmp[x];
}

// Split [lo,hi) in two, sort each half recursively, merge. O(n log n)
void sortRange(std::vector<Product>& a, std::vector<Product>& tmp,
               size_t lo, size_t hi, const ProductLess& less) {
    if (hi - lo < 2) return;
    size_t mid = lo + (hi - lo) / 2;
    sortRange(a, tmp, lo, mid, less);
    sortRange(a, tmp, mid, hi, less);
    mergeHalves(a, tmp, lo, mid, hi, less);
}

}  // namespace

void mergeSort(std::vector<Product>& items, const ProductLess& less) {
    if (items.size() < 2) return;
    std::vector<Product> tmp(items.size());
    sortRange(items, tmp, 0, items.size(), less);
}

ProductLess makeComparator(const std::string& field, bool ascending) {
    ProductLess base;
    if (field == "price")      base = [](const Product& a, const Product& b) { return a.price < b.price; };
    else if (field == "qty")   base = [](const Product& a, const Product& b) { return a.qty < b.qty; };
    else if (field == "name")  base = [](const Product& a, const Product& b) { return lower(a.name) < lower(b.name); };
    else return ProductLess();
    if (ascending) return base;
    return [base](const Product& a, const Product& b) { return base(b, a); };   // swap arguments = descending
}

std::vector<Product> searchByName(const std::vector<Product>& items, const std::string& text) {
    std::vector<Product> found;
    std::string needle = lower(text);
    for (const Product& p : items) {                       // look at every product once
        if (lower(p.name).find(needle) != std::string::npos || lower(p.sku).find(needle) != std::string::npos) found.push_back(p);
    }
    return found;
}

std::vector<Product> searchByCategory(const std::vector<Product>& items, int categoryId) {
    std::vector<Product> found;
    for (const Product& p : items) {
        if (p.categoryId == categoryId) found.push_back(p);
    }
    return found;
}

int binarySearchById(const std::vector<Product>& sortedById, int id) {
    int lo = 0, hi = static_cast<int>(sortedById.size()) - 1;
    while (lo <= hi) {                                     // halve the range each step
        int mid = lo + (hi - lo) / 2;
        if (sortedById[mid].id == id) return mid;
        if (sortedById[mid].id < id) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}
