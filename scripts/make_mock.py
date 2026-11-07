#!/usr/bin/env python3
"""make_mock.py : save real API answers as public/mock/*.json for the frontend's ?mock=1 mode.

Needs a server running on a TEST database (it performs a few writes first so the pages have activity):
    scripts/load_db.sh ims_test
    DB_NAME=ims_test PORT=8099 build/ims &
    python3 scripts/make_mock.py http://127.0.0.1:8099
The demo login is read from the comment at the top of sql/seed.sql.
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
OUT = ROOT / "public" / "mock"
OUT.mkdir(parents=True, exist_ok=True)

seed = (ROOT / "sql" / "seed.sql").read_text()
m = re.search(r"admin / (\S+) ;", seed)
if not m:
    sys.exit("could not find the demo admin login in sql/seed.sql")
opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(CookieJar()))


def call(method, path, body=None):
    """Return the server's JSON envelope for one request."""
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(BASE + path, data=data, method=method, headers={"Content-Type": "application/json"} if data else {})
    try:
        return json.load(opener.open(req))
    except urllib.error.HTTPError as e:
        return json.load(e)


def save(name, doc):
    (OUT / (name + ".json")).write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
    print("  wrote", name + ".json")


assert call("POST", "/api/auth/login", {"username": "admin", "password": m.group(1)})["ok"], "login failed"

# writes first, so the lists below show some activity
writes = {}
writes["purchase_ok"] = call("POST", "/api/purchases", {"supplier_id": 5, "items": [{"product_id": 9, "quantity": 40, "unit_cost": 20}]})
writes["undo_ok"] = call("POST", "/api/undo")
writes["sale_ok"] = call("POST", "/api/sales", {"customer_id": 1, "items": [{"product_id": 6, "quantity": 2}]})
writes["sale_fail"] = call("POST", "/api/sales", {"customer_id": 1, "items": [{"product_id": 6, "quantity": 99999}]})
sale_id = writes["sale_ok"]["data"]["sale_id"]
item = {"sku": "SKU0001", "name": "Mock", "category_id": 1, "supplier_id": 1, "unit_price": 10, "stock_quantity": 0, "min_stock_level": 1}
writes["dup_sku"] = call("POST", "/api/products", item)
writes["bad_price"] = call("POST", "/api/products", dict(item, sku="MOCK-NEG", unit_price=-5))
writes["in_use"] = call("DELETE", "/api/categories/1")
made = call("POST", "/api/categories", {"category_name": "Mock category"})
writes["ok"] = made
call("DELETE", "/api/categories/%d" % made["data"]["id"])
for k, v in writes.items():
    assert (k in ("sale_ok", "purchase_ok", "undo_ok", "ok")) == v["ok"], "unexpected result for " + k
save("writes", writes)

for name, path in [("products", "/api/products"), ("categories", "/api/categories"), ("suppliers", "/api/suppliers"),
                   ("customers", "/api/customers"), ("alerts", "/api/alerts/low-stock"), ("dashboard", "/api/dashboard"),
                   ("transactions", "/api/transactions/recent"), ("sales", "/api/sales"), ("queue", "/api/sales/queue"),
                   ("purchases", "/api/purchases"), ("users", "/api/users"), ("model", "/api/model"),
                   ("bill", "/api/sales/%d/bill" % sale_id)]:
    doc = call("GET", path)
    assert doc["ok"], path
    save(name, doc)

listing = call("GET", "/api/reports")
save("reports", {"list": listing, "reports": {r["name"]: call("GET", "/api/reports/" + r["name"]) for r in listing["data"]}})
print("done")
