#!/usr/bin/env python3
"""api_smoke.py : end-to-end test of the REST API (test plan T1-T19, API part).

Start the server against a TEST database first:
    scripts/load_db.sh ims_test
    DB_NAME=ims_test PORT=8099 build/ims &
    python3 tests/api_smoke.py http://127.0.0.1:8099
It changes data (sales, purchases, a product, a user), so never point it at your real database.
Demo logins come from the comment at the top of sql/seed.sql.
"""
import json
import sys
import urllib.error
import urllib.request
from http.cookiejar import CookieJar

BASE = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8099"
PNG = bytes.fromhex("89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4890000000d49444154789c6360f8cfc0f01f0005000201a5f645400000000049454e44ae426082")
passed = failed = 0


def check(cond, label):
    global passed, failed
    if cond:
        passed += 1
        print("  PASS ", label)
    else:
        failed += 1
        print("  FAIL ", label)


class Client:
    """One browser: keeps its own session cookie."""
    def __init__(self):
        self.opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(CookieJar()))

    def call(self, method, path, body=None, raw=None, ctype=None):
        data, headers = None, {}
        if body is not None:
            data, headers["Content-Type"] = json.dumps(body).encode(), "application/json"
        if raw is not None:
            data, headers["Content-Type"] = raw, ctype or "application/octet-stream"
        req = urllib.request.Request(BASE + path, data=data, method=method, headers=headers)
        try:
            resp = self.opener.open(req)
        except urllib.error.HTTPError as e:
            resp = e
        payload = resp.read()
        kind = resp.headers.get("Content-Type", "")
        return resp.status, (json.loads(payload) if kind.startswith("application/json") else payload), kind

    def j(self, method, path, body=None):
        status, doc, _ = self.call(method, path, body)
        return status, doc


def find(rows, **kw):
    return next((r for r in rows if all(r.get(k) == v for k, v in kw.items())), None)


anon, admin, emp = Client(), Client(), Client()

print("[T1 login and sessions]")
check(anon.j("GET", "/api/products")[0] == 401, "no session -> 401")
st, d = anon.j("POST", "/api/auth/login", {"username": "admin", "password": "wrong"})
check(st == 401 and d["error"]["code"] == "INVALID_CREDENTIALS", "wrong password -> 401")
st, d = anon.j("POST", "/api/auth/login", {"username": "nobody", "password": "whatever1"})
check(st == 401, "unknown user -> 401")
st, d = admin.j("POST", "/api/auth/login", {"username": "admin", "password": "Admin@2026"})
check(st == 200 and d["data"]["role"] == "ADMIN", "admin login ok")
check(d["sql_log"] == [], "login response carries no SQL (password hash never exposed)")
st, d = emp.j("POST", "/api/auth/login", {"username": "employee", "password": "Staff@2026"})
check(st == 200 and d["data"]["role"] == "EMPLOYEE", "employee login ok")
check(admin.j("GET", "/api/auth/me")[1]["data"]["username"] == "admin", "/auth/me returns the user")
check(anon.j("POST", "/api/auth/login", {"username": "admin"})[0] == 400, "missing password -> 400")
codes = [anon.j("POST", "/api/auth/login", {"username": "throttle.test", "password": "bad%d" % i})[0] for i in range(6)]
check(codes[:5] == [401] * 5 and codes[5] == 429, "6th wrong password for one user name is blocked (429)")
st, d = anon.j("POST", "/api/auth/login", {"username": "throttle.test", "password": "bad7"})
check(st == 429 and d["error"]["code"] == "TOO_MANY_ATTEMPTS", "a blocked user name stays blocked and says why")
check(Client().j("POST", "/api/auth/login", {"username": "employee", "password": "Staff@2026"})[0] == 200, "other users can still sign in meanwhile")
st, d, _ = anon.call("POST", "/api/auth/login", raw=b"{not json", ctype="application/json")
check(st == 400 and d["error"]["code"] == "INVALID_JSON", "broken JSON -> 400")

print("[T6 T7 search and sort]")
st, d = admin.j("GET", "/api/products")
check(st == 200 and len(d["data"]) == 60 and d["sql_log"] == [], "60 products, served from memory (no SQL)")
check(len(admin.j("GET", "/api/products?search=pen")[1]["data"]) >= 3, "search by name 'pen'")
r = admin.j("GET", "/api/products?search=8")[1]["data"]
check(any(p["id"] == 8 for p in r), "search by id 8 (hash map)")
prices = [p["unit_price"] for p in admin.j("GET", "/api/products?sort=price&order=asc")[1]["data"]]
check(prices == sorted(prices), "sort by price ascending (merge sort)")
prices = [p["unit_price"] for p in admin.j("GET", "/api/products?sort=price&order=desc")[1]["data"]]
check(prices == sorted(prices, reverse=True), "sort by price descending")
names = [p["name"].lower() for p in admin.j("GET", "/api/products?sort=name")[1]["data"]]
check(names == sorted(names), "sort by name")
check(admin.j("GET", "/api/products?sort=colour")[0] == 400, "unknown sort field -> 400")
cat1 = admin.j("GET", "/api/products?category=1")[1]["data"]
check(len(cat1) == 10 and all(p["category_id"] == 1 for p in cat1), "filter by category")
check(admin.j("GET", "/api/products?search=%27%20OR%201%3D1%20--")[1]["data"] == [], "T19 injection text is just text")
check(admin.j("GET", "/api/products/6")[1]["data"]["name"] == "Notebook 100 Pages", "get one product")
h = admin.j("GET", "/api/products/8?via=hash")[1]["data"]
b = admin.j("GET", "/api/products/8?via=binary")[1]["data"]
check(h["name"] == b["name"] and h["found_by"] == "hash map" and b["found_by"].startswith("binary search"), "same product found by hash map and by binary search (tree map order)")
check(all(admin.j("GET", "/api/products/%d?via=binary" % i)[1]["data"]["id"] == i for i in (1, 30, 60)), "binary search finds first, middle and last id")
check(admin.j("GET", "/api/products/9999?via=binary")[0] == 404 and admin.j("GET", "/api/products/6?via=x")[0] == 400, "binary lookup: missing id 404, bad option 400")
st, d = admin.j("GET", "/api/products/sku-check?sku=SKU0001")
check(st == 200 and d["data"]["exists"] is True and d["data"]["checked_with"] == "hash set" and d["sql_log"] == [], "SKU check answers 'taken' from the hash set, without SQL")
check(admin.j("GET", "/api/products/sku-check?sku=NO-SUCH-SKU")[1]["data"]["exists"] is False and admin.j("GET", "/api/products/sku-check")[0] == 400, "SKU check: free SKU false, missing parameter 400")
check(admin.j("GET", "/api/products/9999")[0] == 404, "missing product -> 404")

print("[T2 T3 T4 T5 product writes and roles]")
new = {"sku": "API0001", "name": "API Test Item", "category_id": 1, "supplier_id": 1, "unit_price": 12.5, "stock_quantity": 5, "min_stock_level": 2}
st, d = admin.j("POST", "/api/products", new)
check(st == 201 and any(e["sql"].startswith("INSERT INTO Products") for e in d["sql_log"]), "T3 add product, log shows INSERT")
pid = d["data"]["id"]
st, d = admin.j("POST", "/api/products", new)
check(st == 409 and d["error"]["mysql_errno"] == 1062 and d["sql_log"][-1]["sql"] == "ROLLBACK", "T4 duplicate SKU -> 1062 + ROLLBACK")
st, d = admin.j("POST", "/api/products", dict(new, sku="API0002", unit_price=-5))
check(st == 400 and d["error"]["mysql_errno"] == 3819, "T5 price -5 -> CHECK 3819")
check(admin.j("POST", "/api/products", {"sku": "X"})[0] == 400, "missing fields -> 400")
st, d = admin.j("PUT", "/api/products/%d" % pid, dict(new, name="Renamed", unit_price=20, stock_quantity=999))
check(st == 200 and d["data"]["name"] == "Renamed" and d["data"]["stock_quantity"] == 5, "update keeps stock")
check(emp.j("POST", "/api/products", dict(new, sku="API0003"))[0] == 403, "employee cannot add product")
st, d = emp.j("DELETE", "/api/products/%d" % pid)
check(st == 403 and d["error"]["code"] == "FORBIDDEN", "T2 employee DELETE product -> 403")
check(emp.j("GET", "/api/users")[0] == 403 and emp.j("GET", "/api/model")[0] == 403, "employee blocked from users and model")
check(emp.j("POST", "/api/undo")[0] == 403, "employee cannot undo")
check(admin.j("DELETE", "/api/products/%d" % pid)[0] == 200, "admin deletes the new product (no history)")

print("[T9 T10 sales]")
before = admin.j("GET", "/api/products/6")[1]["data"]["stock_quantity"]
st, d = emp.j("POST", "/api/sales", {"customer_id": 1, "items": [{"product_id": 6, "quantity": 2}]})
check(st == 201 and d["data"]["total"] == 70.0, "T9 sale ok, total 70.00")
ins = find(d["sql_log"], kind="DML", rows_affected=1) and [e for e in d["sql_log"] if e["sql"].startswith("INSERT INTO Sale_Items")][0]
check(len(ins["triggers"]) == 2 and ins["before"]["row"]["stock_quantity"] == str(before), "log carries trigger notes + before/after")
check(d["sql_log"][0]["sql"] == "START TRANSACTION" and d["sql_log"][-1]["sql"] == "COMMIT", "log runs START TRANSACTION..COMMIT")
sale_id = d["data"]["sale_id"]
check(admin.j("GET", "/api/products/6")[1]["data"]["stock_quantity"] == before - 2, "memory shows stock - 2")
st, d = emp.j("POST", "/api/sales", {"customer_id": 1, "items": [{"product_id": 6, "quantity": 99999}]})
check(st == 409 and d["error"]["mysql_errno"] == 1644 and d["error"]["message"] == "Insufficient stock for product 6", "T10 oversell -> 1644")
check(d["sql_log"][-1]["sql"] == "ROLLBACK", "T10 log ends with ROLLBACK")
check(admin.j("GET", "/api/products/6")[1]["data"]["stock_quantity"] == before - 2, "T10 stock unchanged after oversell")
check(emp.j("POST", "/api/sales", {"items": []})[0] == 400, "empty items -> 400")
st, d = emp.j("POST", "/api/sales", {"items": [{"product_id": 9999, "quantity": 1}]})
check(st == 404, "unknown product in sale -> 404")
st, d = emp.j("POST", "/api/sales", {"customer_id": 9999, "items": [{"product_id": 6, "quantity": 1}]})
check(st == 400 and d["error"]["mysql_errno"] == 1452, "unknown customer -> FK 1452")

print("[T11 FIFO queue and bill]")
q = admin.j("GET", "/api/sales/queue")[1]["data"]
ids = [o["order_id"] for o in q["orders"]]
check(ids == sorted(ids) and len(ids) >= 3 and q["pending"] == 0, "orders listed in arrival order")
check([o["status"] for o in q["orders"]][:2] == ["done", "failed"], "statuses recorded (done, failed ...)")
st, d = admin.j("GET", "/api/sales/%d/bill" % sale_id)
check(st == 200 and len(d["data"]["lines"]) == 1 and d["data"]["lines"][0]["line_total"] == 70.0, "bill has the sale line")
check(admin.j("GET", "/api/sales/9999/bill")[0] == 404, "bill for missing sale -> 404")
check(len(admin.j("GET", "/api/sales")[1]["data"]) >= 1, "sales history from view")

print("[T8 purchases, T12 alerts, T13 undo]")
before9 = admin.j("GET", "/api/products/9")[1]["data"]["stock_quantity"]
st, d = emp.j("POST", "/api/purchases", {"supplier_id": 5, "items": [{"product_id": 9, "quantity": 40, "unit_cost": 20}]})
check(st == 201 and d["data"]["total"] == 800.0, "T8 purchase total 800.00")
check(admin.j("GET", "/api/products/9")[1]["data"]["stock_quantity"] == before9 + 40, "T8 stock raised")
check(len(admin.j("GET", "/api/purchases")[1]["data"]) >= 1, "purchase history")
al = admin.j("GET", "/api/alerts/low-stock")[1]["data"]
ratios = [a["ratio"] for a in al]
check(len(al) >= 1 and ratios == sorted(ratios), "T12 alerts listed most urgent first")
st, d = admin.j("POST", "/api/undo")
check(st == 200 and d["data"]["product_id"] == 9 and d["data"]["stock_quantity"] == before9, "T13 undo reverses the purchase")
rec = admin.j("GET", "/api/transactions/recent")[1]["data"]
check(rec[0]["txn_type"] == "UNDO" and rec[0]["quantity_change"] == -40, "T13 ledger shows an UNDO row")

print("[T14 foreign keys]")
st, d = admin.j("DELETE", "/api/categories/1")
check(st == 409 and d["error"]["mysql_errno"] == 1451, "T14 delete category in use -> 1451")
check(admin.j("DELETE", "/api/products/6")[1]["error"]["mysql_errno"] == 1451, "delete product with history -> 1451")
check(admin.j("DELETE", "/api/customers/1")[0] == 400, "Walk-in customer cannot be deleted")

print("[categories, suppliers, customers]")
st, d = admin.j("POST", "/api/categories", {"category_name": "Test Cat", "description": ""})
check(st == 201, "add category")
cid = d["data"]["id"]
check(admin.j("POST", "/api/categories", {"category_name": "Test Cat"})[1]["error"]["mysql_errno"] == 1062, "duplicate category -> 1062")
check(admin.j("PUT", "/api/categories/%d" % cid, {"category_name": "Test Cat 2"})[0] == 200, "rename category")
check(admin.j("PUT", "/api/categories/9999", {"category_name": "x"})[0] == 404, "update missing category -> 404")
check(find(admin.j("GET", "/api/categories")[1]["data"], category_name="Test Cat 2") is not None, "category listed with product_count")
check(admin.j("GET", "/api/products")[1]["data"][0]["category_name"] != "", "product JSON carries category name")
check(admin.j("DELETE", "/api/categories/%d" % cid)[0] == 200, "delete unused category")
st, d = emp.j("POST", "/api/customers", {"customer_name": "Test Buyer", "phone": "9876500001", "city": "Hyderabad"})
check(st == 201, "employee adds a customer")
cust = d["data"]["id"]
check(emp.j("PUT", "/api/customers/%d" % cust, {"customer_name": "Test Buyer 2", "phone": "9876500001"})[0] == 200, "employee edits a customer")
st, d = emp.j("POST", "/api/customers", {"customer_name": "Bad Phone", "phone": "12345"})
check(st == 400 and d["error"]["mysql_errno"] == 3819, "bad phone -> CHECK 3819")
check(emp.j("DELETE", "/api/customers/%d" % cust)[0] == 403, "employee cannot delete a customer")
check(admin.j("DELETE", "/api/customers/%d" % cust)[0] == 200, "admin deletes the customer")
check(emp.j("POST", "/api/suppliers", {"supplier_name": "x", "phone": "9876500002"})[0] == 403, "employee cannot add supplier")
st, d = admin.j("POST", "/api/suppliers", {"supplier_name": "Test Supplier", "phone": "9876500003", "email": ""})
check(st == 201, "admin adds supplier")
check(admin.j("DELETE", "/api/suppliers/%d" % d["data"]["id"])[0] == 200, "admin deletes supplier")

print("[T15 reports, dashboard, model]")
names = [r["name"] for r in admin.j("GET", "/api/reports")[1]["data"]]
check(len(names) == 6, "six reports listed")
for n in names:
    st, d = emp.j("GET", "/api/reports/" + n)
    check(st == 200 and len(d["data"]["columns"]) >= 2 and "SELECT" in d["sql_log"][0]["sql"], "report %s runs (employee ok)" % n)
check(admin.j("GET", "/api/reports/nope")[0] == 404, "unknown report -> 404")
st, d = admin.j("GET", "/api/dashboard")
check(st == 200 and d["data"]["products"] == 60 and d["data"]["stock_value"] > 0, "dashboard numbers")
tables = admin.j("GET", "/api/model")[1]["data"]["tables"]
check(len(tables) == 10, "model lists 10 tables")
products_cols = find(tables, name=next(t["name"] for t in tables if t["name"].lower() == "products"))["columns"]
check(find(products_cols, name="category_id")["references"]["column"] == "category_id", "model shows foreign keys")

print("[T17 photo BLOB]")
st, d, _ = admin.call("POST", "/api/products/1/photo", raw=PNG, ctype="image/png")
check(st == 200 and d["data"]["bytes"] == len(PNG), "upload photo")
st, body, kind = admin.call("GET", "/api/products/1/photo")
check(st == 200 and body == PNG and kind.startswith("image/png"), "same bytes come back")
check(admin.j("GET", "/api/products/1")[1]["data"]["has_photo"] is True, "has_photo true")
check(admin.call("POST", "/api/products/1/photo", raw=b"not an image at all", ctype="image/png")[0] == 400, "non-image refused")
check(admin.call("POST", "/api/products/1/photo", raw=PNG + b"0" * (1024 * 1024), ctype="image/png")[0] == 413, "photo over 1 MB refused")
check(admin.call("GET", "/api/products/2/photo")[0] == 404, "no photo -> 404")
check(emp.call("POST", "/api/products/1/photo", raw=PNG, ctype="image/png")[0] == 403, "employee cannot upload photo")

print("[users]")
st, d = admin.j("POST", "/api/users", {"username": "temp.user", "password": "TempPass#1", "role": "EMPLOYEE"})
check(st == 201 and "***" in d["sql_log"][1]["sql"] and "TempPass" not in json.dumps(d), "create user; password hash hidden in SQL log")
uid = d["data"]["id"]
check(admin.j("POST", "/api/users", {"username": "temp.user", "password": "TempPass#1", "role": "EMPLOYEE"})[0] == 409, "duplicate username -> 409")
check(admin.j("POST", "/api/users", {"username": "weak", "password": "short", "role": "ADMIN"})[0] == 400, "short password -> 400")
newc = Client()
check(newc.j("POST", "/api/auth/login", {"username": "temp.user", "password": "TempPass#1"})[0] == 200, "new user can log in")
check(newc.j("GET", "/api/products")[0] == 200, "the new user's session works before the reset")
check(admin.j("PUT", "/api/users/%d" % uid, {"password": "Changed#Pass9"})[0] == 200, "reset password")
check(newc.j("GET", "/api/products")[0] == 401, "the user's open session ends when their password is reset")
check(Client().j("POST", "/api/auth/login", {"username": "temp.user", "password": "TempPass#1"})[0] == 401, "old password no longer works")
me = admin.j("GET", "/api/auth/me")[1]["data"]["user_id"]
check(admin.j("DELETE", "/api/users/%d" % me)[0] == 400, "cannot delete own account")
check(admin.j("PUT", "/api/users/%d" % me, {"role": "EMPLOYEE"})[0] == 400, "cannot demote self")
c3 = Client()
check(c3.j("POST", "/api/auth/login", {"username": "temp.user", "password": "Changed#Pass9"})[0] == 200, "user signs in with the new password")
check(admin.j("PUT", "/api/users/%d" % uid, {"role": "ADMIN"})[0] == 200 and c3.j("GET", "/api/products")[0] == 401, "a role change ends the user's open sessions")
check(c3.j("POST", "/api/auth/login", {"username": "temp.user", "password": "Changed#Pass9"})[0] == 200, "signing in again gives the new role")
check(admin.j("DELETE", "/api/users/%d" % uid)[0] == 200, "delete user")
check(c3.j("GET", "/api/users")[0] == 401, "a deleted user's open session stops working at once")
check(len(admin.j("GET", "/api/users")[1]["data"]) == 2, "back to two users")

print("[hardening]")
hdr = urllib.request.urlopen(BASE + "/login.html").headers
check("default-src 'self'" in hdr.get("Content-Security-Policy", ""), "pages carry a Content-Security-Policy")
check(hdr.get("X-Frame-Options") == "DENY" and hdr.get("X-Content-Type-Options") == "nosniff" and hdr.get("Referrer-Policy"), "pages carry framing, sniffing and referrer headers")
check(admin.call("GET", "/api/products")[0] == 200 and "Content-Security-Policy" in urllib.request.urlopen(BASE + "/img/er_diagram.png").headers, "images carry them too")
st, d, _ = anon.call("POST", "/api/auth/login", raw=b"a" * 3000000, ctype="application/json")
check(st == 413 and d["error"]["code"] == "TOO_LARGE", "an oversized body is answered 413 TOO_LARGE, not a false 404")
st, d = admin.j("POST", "/api/sales", {"items": [{"product_id": 6, "quantity": 2000000}]})
check(st == 400 and "1,000,000" in d["error"]["message"], "a quantity above 1,000,000 is refused before it reaches MySQL")

print("[misc]")
st, d = admin.j("GET", "/api/nope")
check(st == 404 and d["error"]["code"] == "NOT_FOUND", "unknown API path -> JSON 404")
check(admin.call("GET", "/img/er_diagram.png")[0] == 200, "static file served from public/")
st, d = emp.j("POST", "/api/auth/logout")
check(st == 200 and emp.j("GET", "/api/auth/me")[0] == 401, "logout ends the session")

print("\n%d/%d checks passed" % (passed, passed + failed))
sys.exit(0 if failed == 0 else 1)
