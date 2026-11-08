#!/usr/bin/env python3
"""livesql_coverage.py : audit of the Live SQL panel feed. Calls every API endpoint once and prints, per endpoint,
how many SQL statements the response carries (this is exactly what the browser panel renders).

Needs a server on a TEST database (it creates and deletes rows):
    scripts/load_db.sh ims_test
    DB_NAME=ims_test PORT=8099 build/ims &
    python3 tests/livesql_coverage.py http://127.0.0.1:8099
Exit code 1 when a data-changing endpoint returns no SQL, or a failed write does not end in ROLLBACK.
"""
import json
import pathlib
import re
import sys
import urllib.error
import urllib.request
from http.cookiejar import CookieJar

BASE = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8099"
ROOT = pathlib.Path(__file__).resolve().parent.parent
PASSWORD = re.search(r"admin / (\S+) ;", (ROOT / "sql" / "seed.sql").read_text()).group(1)
PNG = bytes.fromhex("89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4890000000d49444154789c6360f8cfc0f01f0005000201a5f645400000000049454e44ae426082")
opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(CookieJar()))
rows, problems = [], []


def call(method, path, body=None, raw=None):
    data, headers = None, {}
    if body is not None:
        data, headers = json.dumps(body).encode(), {"Content-Type": "application/json"}
    if raw is not None:
        data, headers = raw, {"Content-Type": "image/png"}
    req = urllib.request.Request(BASE + path, data=data, method=method, headers=headers)
    try:
        resp = opener.open(req)
    except urllib.error.HTTPError as e:
        resp = e
    payload = resp.read()
    return resp.status, (json.loads(payload) if resp.headers.get("Content-Type", "").startswith("application/json") else payload)


def record(label, method, path, body=None, raw=None, writes=False, expect_sql=True, note=""):
    status, doc = call(method, path, body, raw)
    log = doc.get("sql_log", []) if isinstance(doc, dict) else []
    kinds = sorted({e["kind"] for e in log})
    outputs = sum(1 for e in log if e["result"] or e["rows_affected"] is not None)
    trig = sum(len(e["triggers"]) for e in log)
    snap = sum(1 for e in log if e["before"] or e["after"])
    errs = [e for e in log if e["error"]]
    rows.append((label, status, len(log), ",".join(kinds) or "-", outputs, trig, snap, bool(errs), note))
    if expect_sql and not log:
        problems.append("%s returned no SQL" % label)
    if errs and (not log or log[-1]["sql"] != "ROLLBACK") and writes:
        problems.append("%s failed but the log does not end in ROLLBACK" % label)
    return status, doc


call("POST", "/api/auth/login", {"username": "admin", "password": PASSWORD})
print("== reads")
record("GET /products (memory)", "GET", "/api/products", expect_sql=False, note="answered from C++ structures: panel shows a note")
record("GET /alerts/low-stock (memory)", "GET", "/api/alerts/low-stock", expect_sql=False, note="memory: panel shows a note")
record("GET /sales/queue (memory)", "GET", "/api/sales/queue", expect_sql=False, note="memory")
for p in ["categories", "suppliers", "customers", "dashboard", "transactions/recent", "sales", "purchases", "users", "model", "sales/1/bill"]:
    record("GET /" + p, "GET", "/api/" + p)
for r in ["stock-value", "top-sellers", "sales-per-customer", "purchases-per-supplier", "never-sold", "low-stock"]:
    record("GET /reports/" + r, "GET", "/api/reports/" + r)

print("== writes")
cid = record("POST /categories", "POST", "/api/categories", {"category_name": "Audit cat"}, writes=True)[1]["data"]["id"]
record("PUT /categories/{id}", "PUT", "/api/categories/%d" % cid, {"category_name": "Audit cat 2"}, writes=True)
record("DELETE /categories/{id}", "DELETE", "/api/categories/%d" % cid, writes=True)
sid = record("POST /suppliers", "POST", "/api/suppliers", {"supplier_name": "Audit sup", "phone": "9999900001"}, writes=True)[1]["data"]["id"]
record("PUT /suppliers/{id}", "PUT", "/api/suppliers/%d" % sid, {"supplier_name": "Audit sup 2", "phone": "9999900001"}, writes=True)
record("DELETE /suppliers/{id}", "DELETE", "/api/suppliers/%d" % sid, writes=True)
uid = record("POST /customers", "POST", "/api/customers", {"customer_name": "Audit cust", "phone": "9999900002"}, writes=True)[1]["data"]["id"]
record("PUT /customers/{id}", "PUT", "/api/customers/%d" % uid, {"customer_name": "Audit cust 2", "phone": "9999900002"}, writes=True)
record("DELETE /customers/{id}", "DELETE", "/api/customers/%d" % uid, writes=True)
item = {"sku": "AUD-1", "name": "Audit item", "category_id": 1, "supplier_id": 1, "unit_price": 10, "stock_quantity": 5, "min_stock_level": 1}
pid = record("POST /products", "POST", "/api/products", item, writes=True)[1]["data"]["id"]
record("PUT /products/{id}", "PUT", "/api/products/%d" % pid, dict(item, name="Audit item 2"), writes=True)
record("POST /products/{id}/photo", "POST", "/api/products/%d/photo" % pid, raw=PNG, writes=True)
record("GET /products/{id}/photo", "GET", "/api/products/%d/photo" % pid, expect_sql=False, note="image bytes, not JSON")
record("DELETE /products/{id}", "DELETE", "/api/products/%d" % pid, writes=True)
record("POST /purchases", "POST", "/api/purchases", {"supplier_id": 1, "items": [{"product_id": 6, "quantity": 5, "unit_cost": 20}]}, writes=True)
record("POST /sales", "POST", "/api/sales", {"customer_id": 1, "items": [{"product_id": 6, "quantity": 2}]}, writes=True)
record("POST /undo", "POST", "/api/undo", {}, writes=True)
uid = record("POST /users", "POST", "/api/users", {"username": "audit.user", "password": "Audit-Pw-12345", "role": "EMPLOYEE"}, writes=True)[1]["data"]["id"]
record("PUT /users/{id}", "PUT", "/api/users/%d" % uid, {"role": "ADMIN"}, writes=True)
record("DELETE /users/{id}", "DELETE", "/api/users/%d" % uid, writes=True)

print("== failing writes (must end in ROLLBACK)")
record("POST /sales oversell", "POST", "/api/sales", {"customer_id": 1, "items": [{"product_id": 6, "quantity": 99999}]}, writes=True, note="error 1644")
record("POST /products duplicate SKU", "POST", "/api/products", dict(item, sku="SKU0001"), writes=True, note="error 1062")
record("DELETE /categories/1 (in use)", "DELETE", "/api/categories/1", writes=True, note="error 1451")
record("POST /purchases bad product", "POST", "/api/purchases", {"supplier_id": 1, "items": [{"product_id": 99999, "quantity": 1, "unit_cost": 5}]}, writes=True, note="error 1452")

print("\n%-34s %-6s %-5s %-14s %-8s %-9s %-6s %s" % ("endpoint", "status", "stmts", "kinds", "outputs", "triggers", "b/a", "error"))
for r in rows:
    print("%-34s %-6s %-5s %-14s %-8s %-9s %-6s %s %s" % (r[0], r[1], r[2], r[3], r[4], r[5], r[6], "yes" if r[7] else "", r[8]))
print()
for p in problems:
    print("PROBLEM:", p)
print("%d endpoints checked, %d problems" % (len(rows), len(problems)))
sys.exit(1 if problems else 0)
