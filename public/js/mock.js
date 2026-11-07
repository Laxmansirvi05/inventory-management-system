// mock.js : stands in for the server when ?mock=1. Reads public/mock/*.json (real server answers saved by
// scripts/make_mock.py). Writes are answered with canned replies and are NOT saved.
window.IMS = window.IMS || {};

IMS.mock = (function () {
  const cache = {};

  // load mock/<name>.json once
  async function file(name) {
    if (!cache[name]) cache[name] = fetch('mock/' + name + '.json').then((r) => r.json());
    return cache[name];
  }
  const pause = () => new Promise((r) => setTimeout(r, 60));
  const clone = (x) => JSON.parse(JSON.stringify(x));
  const reply = (doc, status) => ({ ok: !!doc.ok, status: status || 200, data: doc.data === undefined ? null : doc.data, error: doc.error || null, sql_log: doc.sql_log || [] });
  const fail = (status, code, message) => ({ ok: false, status, data: null, error: { code, message, mysql_errno: null }, sql_log: [] });

  // the logged-in mock user lives in sessionStorage
  function user() {
    try { return JSON.parse(sessionStorage.getItem('ims_mock_user')); } catch (e) { return null; }
  }

  // products: search, category, sort and order are applied here like the server does
  function productList(doc, q) {
    let rows = clone(doc.data);
    const s = (q.get('search') || '').trim().toLowerCase();
    if (s) rows = rows.filter((p) => String(p.id) === s || p.name.toLowerCase().includes(s));
    if (q.get('category')) rows = rows.filter((p) => String(p.category_id) === q.get('category'));
    const field = { price: 'unit_price', qty: 'stock_quantity', name: 'name' }[q.get('sort')];
    if (field) {
      const dir = q.get('order') === 'desc' ? -1 : 1;
      rows.sort((a, b) => {
        const x = a[field], y = b[field];
        const c = typeof x === 'number' ? x - y : String(x).toLowerCase().localeCompare(String(y).toLowerCase());
        return c * dir;
      });
    }
    return { ok: true, data: rows, sql_log: [] };
  }

  // GET requests
  async function read(p, q) {
    if (p === '/api/products') return reply(productList(await file('products'), q));
    let m = /^\/api\/products\/(\d+)$/.exec(p);
    if (m) { const hit = (await file('products')).data.find((x) => String(x.id) === m[1]); return hit ? reply({ ok: true, data: hit }) : fail(404, 'NOT_FOUND', 'Product not found'); }
    m = /^\/api\/reports\/([a-z-]+)$/.exec(p);
    if (m) { const all = await file('reports'); return all.reports[m[1]] ? reply(all.reports[m[1]]) : fail(404, 'NOT_FOUND', 'Unknown report'); }
    if (p === '/api/reports') return reply((await file('reports')).list);
    if (/^\/api\/sales\/\d+\/bill$/.test(p)) return reply(await file('bill'));
    const names = { '/api/categories': 'categories', '/api/suppliers': 'suppliers', '/api/customers': 'customers',
      '/api/alerts/low-stock': 'alerts', '/api/dashboard': 'dashboard', '/api/transactions/recent': 'transactions',
      '/api/sales': 'sales', '/api/sales/queue': 'queue', '/api/purchases': 'purchases', '/api/users': 'users', '/api/model': 'model' };
    return names[p] ? reply(await file(names[p])) : fail(404, 'NOT_FOUND', 'No such API endpoint');
  }

  // POST / PUT / DELETE: canned answers from mock/writes.json
  async function write(method, p, body) {
    const w = await file('writes');
    if (method === 'POST' && p === '/api/sales') {
      const prods = (await file('products')).data;
      const items = (body && body.items) || [];
      const bad = items.find((it) => { const pr = prods.find((x) => x.id === it.product_id); return !pr || it.quantity > pr.stock_quantity; });
      if (bad) { const r = clone(w.sale_fail); r.error.message = 'Insufficient stock for product ' + bad.product_id; return reply(r, 409); }
      const r = clone(w.sale_ok);
      r.data.total = items.reduce((s, it) => s + it.quantity * ((prods.find((x) => x.id === it.product_id) || {}).unit_price || 0), 0);
      return reply(r, 201);
    }
    if (method === 'POST' && p === '/api/purchases') return reply(w.purchase_ok, 201);
    if (method === 'POST' && p === '/api/undo') return reply(w.undo_ok);
    if (method === 'POST' && p === '/api/products') {
      const prods = (await file('products')).data;
      if (prods.some((x) => x.sku === (body || {}).sku)) return reply(w.dup_sku, 409);
      if (!((body || {}).unit_price > 0)) return reply(w.bad_price, 400);
      return reply({ ok: true, data: Object.assign({ id: 999, has_photo: false, category_name: '', supplier_name: '' }, body), sql_log: w.ok.sql_log }, 201);
    }
    const cat = /^\/api\/categories\/(\d+)$/.exec(p);
    if (method === 'DELETE' && cat) {
      const row = (await file('categories')).data.find((c) => String(c.category_id) === cat[1]);
      if (row && row.product_count > 0) return reply(w.in_use, 409);
    }
    return reply(w.ok, method === 'POST' ? 201 : 200);
  }

  // entry point used by api.js
  async function handle(method, fullPath, body) {
    await pause();
    const u = new URL(fullPath, location.href), p = u.pathname;
    if (p === '/api/auth/login') {
      const name = ((body || {}).username || '').trim();
      if (!name || !(body || {}).password) return fail(401, 'INVALID_CREDENTIALS', 'Invalid username or password');
      const me = { user_id: name === 'employee' ? 2 : 1, username: name, role: name === 'employee' ? 'EMPLOYEE' : 'ADMIN' };
      sessionStorage.setItem('ims_mock_user', JSON.stringify(me));
      return reply({ ok: true, data: me });
    }
    if (p === '/api/auth/logout') { sessionStorage.removeItem('ims_mock_user'); return reply({ ok: true, data: {} }); }
    const me = user();
    if (!me) return fail(401, 'UNAUTHENTICATED', 'Please log in');
    if (p === '/api/auth/me') return reply({ ok: true, data: me });
    return method === 'GET' ? read(p, u.searchParams) : write(method, p, body);
  }

  return { handle };
})();
