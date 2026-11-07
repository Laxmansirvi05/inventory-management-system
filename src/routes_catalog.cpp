// routes_catalog.cpp : products (in-memory search/sort + MySQL writes), photos, categories, suppliers, customers.
#include <cctype>
#include "server.h"
#include "sorting.h"

namespace {

// ---------- products ----------

json productJson(const App& app, const Product& p) {
    auto cat = app.categoryNames.find(p.categoryId);
    auto sup = app.supplierNames.find(p.supplierId);
    return {{"id", p.id}, {"sku", p.sku}, {"name", p.name},
            {"category_id", p.categoryId}, {"category_name", cat == app.categoryNames.end() ? "" : cat->second},
            {"supplier_id", p.supplierId}, {"supplier_name", sup == app.supplierNames.end() ? "" : sup->second},
            {"unit_price", p.price}, {"stock_quantity", p.qty}, {"min_stock_level", p.minStock},
            {"expiry_date", p.expiry.empty() ? json(nullptr) : json(p.expiry)},
            {"description", p.description}, {"has_photo", p.hasPhoto}};
}

bool allDigits(const std::string& s) {
    if (s.empty() || s.size() > 9) return false;
    for (char ch : s) if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    return true;
}

// GET /api/products?search=&category=&sort=&order=   (served from memory, no SQL)
Reply listProducts(Ctx& c) {
    std::string search = trimmed(c.q("search")), category = c.q("category"), sortField = c.q("sort"), order = c.q("order", "asc");
    std::vector<Product> list = c.app.inv.all();                     // ArrayList copy

    if (!search.empty()) {
        std::vector<Product> hits;
        if (allDigits(search)) {                                     // id search: hash map, O(1)
            if (const Product* p = c.app.inv.findById(std::stoi(search))) hits.push_back(*p);
        }
        for (const Product& p : searchByName(list, search)) {        // name search: linear scan
            bool already = false;
            for (const Product& h : hits) if (h.id == p.id) already = true;
            if (!already) hits.push_back(p);
        }
        list = hits;
    }
    if (!category.empty()) {
        if (!allDigits(category)) return errReply(400, "BAD_REQUEST", "category must be a category id");
        list = searchByCategory(list, std::stoi(category));          // linear scan
    }
    if (!sortField.empty()) {
        if (order != "asc" && order != "desc") return errReply(400, "BAD_REQUEST", "order must be asc or desc");
        ProductLess cmp = makeComparator(sortField, order == "asc");
        if (!cmp) return errReply(400, "BAD_REQUEST", "sort must be price, qty or name");
        mergeSort(list, cmp);                                        // hand-written merge sort
    }
    json arr = json::array();
    for (const Product& p : list) arr.push_back(productJson(c.app, p));
    return okReply(arr);
}

Reply getProduct(Ctx& c) {
    const Product* p = c.app.inv.findById(c.paramInt(0));           // hash map
    if (!p) return errReply(404, "NOT_FOUND", "Product not found");
    return okReply(productJson(c.app, *p));
}

// Read the JSON body of POST/PUT /api/products. Returns an error message, or "" when fine.
std::string parseProduct(const json& b, Product& p) {
    if (!b.is_object()) return "JSON body required";
    std::string s;
    if (!jsonString(b, "sku", s) || trimmed(s).empty()) return "sku is required";
    p.sku = trimmed(s);
    if (!jsonString(b, "name", s) || trimmed(s).empty()) return "name is required";
    p.name = trimmed(s);
    if (!jsonInt(b, "category_id", p.categoryId)) return "category_id is required";
    if (!jsonInt(b, "supplier_id", p.supplierId)) return "supplier_id is required";
    if (!jsonNumber(b, "unit_price", p.price)) return "unit_price is required";
    p.qty = 0;
    p.minStock = 10;
    if (b.contains("stock_quantity") && !jsonInt(b, "stock_quantity", p.qty)) return "stock_quantity must be a whole number";
    if (b.contains("min_stock_level") && !jsonInt(b, "min_stock_level", p.minStock)) return "min_stock_level must be a whole number";
    p.expiry.clear();
    if (b.contains("expiry_date") && !b["expiry_date"].is_null()) {
        if (!jsonString(b, "expiry_date", p.expiry)) return "expiry_date must be YYYY-MM-DD";
        p.expiry = trimmed(p.expiry);
    }
    p.description.clear();
    if (b.contains("description") && !b["description"].is_null()) {
        if (!jsonString(b, "description", p.description)) return "description must be text";
        p.description = trimmed(p.description);
    }
    return "";
}

Reply createProduct(Ctx& c) {
    Product p;
    std::string bad = parseProduct(c.body, p);
    if (!bad.empty()) return errReply(400, "BAD_REQUEST", bad);
    WriteResult w = insertProduct(c.app.db, c.app.inv, p, c.log);
    if (!w.ok) return writeFailure(w);
    return okReply(productJson(c.app, *c.app.inv.findById(static_cast<int>(w.id))), 201);
}

Reply updateProduct(Ctx& c) {
    int id = c.paramInt(0);
    if (!c.app.inv.findById(id)) return errReply(404, "NOT_FOUND", "Product not found");
    Product p;
    std::string bad = parseProduct(c.body, p);
    if (!bad.empty()) return errReply(400, "BAD_REQUEST", bad);
    p.id = id;
    WriteResult w = updateProductRow(c.app.db, c.app.inv, p, c.log);
    if (!w.ok) return writeFailure(w);
    return okReply(productJson(c.app, *c.app.inv.findById(id)));
}

Reply deleteProduct(Ctx& c) {
    WriteResult w = deleteProductRow(c.app.db, c.app.inv, c.paramInt(0), c.log);
    if (!w.ok) return writeFailure(w);
    return okReply({{"deleted", w.id}});
}

// ---------- product photo (BLOB) ----------

// Recognise an image by its first bytes; returns the MIME type or "" if it is not a supported image.
std::string imageType(const std::string& b) {
    if (b.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0) return "image/png";
    if (b.size() > 3 && b.compare(0, 3, "\xFF\xD8\xFF") == 0) return "image/jpeg";
    if (b.compare(0, 4, "GIF8") == 0) return "image/gif";
    if (b.size() > 12 && b.compare(0, 4, "RIFF") == 0 && b.compare(8, 4, "WEBP") == 0) return "image/webp";
    return "";
}

Reply uploadPhoto(Ctx& c) {
    const size_t kMax = 1024 * 1024;
    if (c.rawBody.empty()) return errReply(400, "BAD_REQUEST", "Send the image file as the request body");
    if (c.rawBody.size() > kMax) return errReply(413, "TOO_LARGE", "Photo must be 1 MB or smaller");
    if (imageType(c.rawBody).empty()) return errReply(400, "BAD_REQUEST", "Only PNG, JPEG, GIF or WebP images are allowed");
    WriteResult w = setProductPhoto(c.app.db, c.app.inv, c.paramInt(0), c.rawBody, c.log);
    if (!w.ok) return writeFailure(w);
    return okReply({{"id", w.id}, {"bytes", c.rawBody.size()}});
}

Reply showPhoto(Ctx& c) {
    QueryResult r = c.app.db.execute("SELECT photo FROM Products WHERE product_id = ?", {P(c.paramInt(0))}, nullptr);
    if (r.rows.empty() || !r.rows[0][0] || r.rows[0][0]->empty()) return errReply(404, "NOT_FOUND", "No photo for this product");
    Reply rep;
    rep.rawBody = *r.rows[0][0];
    rep.rawType = imageType(rep.rawBody);
    if (rep.rawType.empty()) rep.rawType = "application/octet-stream";
    return rep;
}

// ---------- categories, suppliers, customers: one generic CRUD ----------

struct Field { const char* column; const char* key; bool required; };

struct Entity {
    const char* table;
    const char* idColumn;
    const char* path;               // e.g. /api/categories
    const char* label;              // used in messages
    std::vector<Field> fields;
    const char* listSql;
    Access writeAccess;             // who may create and update (delete is always admin)
    bool affectsNames;              // refresh the category/supplier name cache after a write
    int protectedId;                // row that must never be deleted (Walk-in Customer), 0 = none
};

const std::vector<Entity>& entities() {
    static const std::vector<Entity> list = {
        {"Categories", "category_id", "/api/categories", "Category",
         {{"category_name", "category_name", true}, {"description", "description", false}},
         "SELECT c.category_id, c.category_name, c.description, COUNT(p.product_id) AS product_count "
         "FROM Categories c LEFT JOIN Products p ON p.category_id = c.category_id "
         "GROUP BY c.category_id, c.category_name, c.description ORDER BY c.category_name",
         Access::Admin, true, 0},
        {"Suppliers", "supplier_id", "/api/suppliers", "Supplier",
         {{"supplier_name", "supplier_name", true}, {"contact_person", "contact_person", false},
          {"phone", "phone", true}, {"email", "email", false}, {"city", "city", false}},
         "SELECT supplier_id, supplier_name, contact_person, phone, email, city FROM Suppliers ORDER BY supplier_name",
         Access::Admin, true, 0},
        {"Customers", "customer_id", "/api/customers", "Customer",
         {{"customer_name", "customer_name", true}, {"phone", "phone", true}, {"email", "email", false}, {"city", "city", false}},
         "SELECT customer_id, customer_name, phone, email, city, registered_on FROM Customers ORDER BY customer_name",
         Access::Any, false, 1},
    };
    return list;
}

// Read the entity's fields from the JSON body into statement parameters (empty optional text becomes NULL).
std::string readFields(const Entity& e, const json& body, Params& out) {
    if (!body.is_object()) return "JSON body required";
    for (const Field& f : e.fields) {
        std::string value;
        if (body.contains(f.key) && !body[f.key].is_null()) {
            if (!jsonString(body, f.key, value)) return std::string(f.key) + " must be text";
            value = trimmed(value);
        }
        if (value.empty()) {
            if (f.required) return std::string(f.key) + " is required";
            out.push_back(PNull());
        } else {
            out.push_back(P(value));
        }
    }
    return "";
}

Reply listEntity(const Entity& e, Ctx& c) {
    return okReply(rowsToJson(c.app.db.execute(e.listSql, {}, &c.log)));
}

Reply createEntity(const Entity& e, Ctx& c) {
    Params params;
    std::string bad = readFields(e, c.body, params);
    if (!bad.empty()) return errReply(400, "BAD_REQUEST", bad);
    std::string cols, marks;
    for (const Field& f : e.fields) { cols += std::string(cols.empty() ? "" : ", ") + f.column; marks += marks.empty() ? "?" : ", ?"; }
    WriteResult w = runStatement(c.app.db, std::string("INSERT INTO ") + e.table + " (" + cols + ") VALUES (" + marks + ")", params, c.log);
    if (!w.ok) return writeFailure(w);
    if (e.affectsNames) c.app.refreshNames();
    return okReply({{"id", w.id}}, 201);
}

Reply updateEntity(const Entity& e, Ctx& c) {
    int id = c.paramInt(0);
    Params params;
    std::string bad = readFields(e, c.body, params);
    if (!bad.empty()) return errReply(400, "BAD_REQUEST", bad);
    std::string sets;
    for (const Field& f : e.fields) sets += std::string(sets.empty() ? "" : ", ") + f.column + " = ?";
    params.push_back(P(id));
    WriteResult w = runStatement(c.app.db, std::string("UPDATE ") + e.table + " SET " + sets + " WHERE " + e.idColumn + " = ?", params, c.log);
    if (!w.ok) return writeFailure(w);
    if (w.affected == 0) return errReply(404, "NOT_FOUND", std::string(e.label) + " not found");
    if (e.affectsNames) c.app.refreshNames();
    return okReply({{"id", id}});
}

Reply deleteEntity(const Entity& e, Ctx& c) {
    int id = c.paramInt(0);
    if (id == e.protectedId) return errReply(400, "PROTECTED", std::string("This ") + e.label + " is built in and cannot be deleted");
    WriteResult w = runStatement(c.app.db, std::string("DELETE FROM ") + e.table + " WHERE " + e.idColumn + " = ?", {P(id)}, c.log);
    if (!w.ok) return writeFailure(w);
    if (w.affected == 0) return errReply(404, "NOT_FOUND", std::string(e.label) + " not found");
    if (e.affectsNames) c.app.refreshNames();
    return okReply({{"deleted", id}});
}

}  // namespace

void registerCatalogRoutes(App& app) {
    app.add("GET", "/api/products", Access::Any, listProducts);
    app.add("GET", R"(/api/products/(\d+))", Access::Any, getProduct);
    app.add("POST", "/api/products", Access::Admin, createProduct);
    app.add("PUT", R"(/api/products/(\d+))", Access::Admin, updateProduct);
    app.add("DELETE", R"(/api/products/(\d+))", Access::Admin, deleteProduct);
    app.add("POST", R"(/api/products/(\d+)/photo)", Access::Admin, uploadPhoto);
    app.add("GET", R"(/api/products/(\d+)/photo)", Access::Any, showPhoto);

    for (const Entity& e : entities()) {
        const Entity* ep = &e;                                   // entities() lives for the whole program
        std::string idPath = std::string(e.path) + R"(/(\d+))";
        app.add("GET", e.path, Access::Any, [ep](Ctx& c) { return listEntity(*ep, c); });
        app.add("POST", e.path, e.writeAccess, [ep](Ctx& c) { return createEntity(*ep, c); });
        app.add("PUT", idPath, e.writeAccess, [ep](Ctx& c) { return updateEntity(*ep, c); });
        app.add("DELETE", idPath, Access::Admin, [ep](Ctx& c) { return deleteEntity(*ep, c); });
    }
}
