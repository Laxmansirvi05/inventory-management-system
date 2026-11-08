// dashboard.js : stat cards, restock alerts (priority queue), recent stock transactions, Undo last (stack).
IMS.boot(async (content, user) => {
  const h = IMS.ui.h, f = IMS.fmt;
  const cards = h('div', { class: 'stats' });
  const syncBox = h('div', {});
  const alertTable = new IMS.ui.DataTable({ pageSize: 8, empty: 'All products are above their minimum stock', columns: [
    { label: 'Product', render: (a) => [a.name, h('div', { class: 'muted' }, a.sku)] },
    { label: 'Stock / min', render: (a) => a.stock_quantity + ' / ' + a.min_stock_level },
    { label: 'Status', render: (a) => IMS.ui.tag(a.status, a.status === 'Critical' ? 'bad' : 'warn') },
    { label: 'Supplier', render: (a) => a.supplier_name || '—' },
  ] });
  const kinds = { SALE: 'info', PURCHASE: 'ok', UNDO: 'warn', ADJUST: '' };
  const txnTable = new IMS.ui.DataTable({ pageSize: 10, empty: 'No stock transactions yet', columns: [
    { label: 'When', render: (t) => h('span', { class: 'nowrap' }, f.dateTime(t.txn_date)) },
    { label: 'Product', key: 'product_name' },
    { label: 'Type', render: (t) => IMS.ui.tag(t.txn_type, kinds[t.txn_type]) },
    { label: 'Change', num: true, render: (t) => (t.quantity_change > 0 ? '+' : '') + t.quantity_change },
  ] });

  // undo the newest sale or purchase made since the server started (admin only): it is removed and its stock put back
  async function undo() {
    if (!(await IMS.ui.confirmDialog('Undo the newest sale or purchase made since the server started? It is removed, its stock is put back, and UNDO rows stay in the ledger.', 'Undo', true))) return;
    const res = await IMS.api.post('/api/undo');
    if (res.ok) {
      const d = res.data;
      IMS.ui.toast('Undone: ' + d.kind.toLowerCase() + ' #' + d.document_id + ' was removed and its stock put back (' + d.lines + (d.lines === 1 ? ' line)' : ' lines)'));
      refresh();
    } else IMS.ui.toast(IMS.api.message(res), 'error');
  }

  // read the products from MySQL again (admin only), after the database was changed outside this app
  async function reload() {
    const res = await IMS.api.post('/api/system/reload');
    if (res.ok) { IMS.ui.toast('Reloaded ' + res.data.products + ' products from the database'); refresh(); }
    else IMS.ui.toast(IMS.api.message(res), 'error');
  }

  // one stat card
  const stat = (label, value, sub, icon, tone) => h('div', { class: 'card stat' + (tone ? ' s-' + tone : '') },
    h('div', { class: 'label' }, label), h('div', { class: 'value' }, value), h('div', { class: 'sub' }, sub), h('div', { class: 'ico' }, IMS.ui.icon(icon)));
  const catBars = h('div', { class: 'bars' });

  // stock value per category as horizontal bars (the "stock-value" report, one GROUP BY query)
  function drawCategories(rows) {
    IMS.ui.clear(catBars);
    if (!rows.length) { catBars.appendChild(h('div', { class: 'empty' }, 'No products yet')); return; }
    const top = Math.max(...rows.map((r) => Number(r.stock_value) || 0)) || 1;
    for (const r of rows) {
      catBars.appendChild(h('div', { class: 'row' },
        h('b', {}, r.category_name),
        h('div', { class: 'track2', title: f.int(r.units) + ' units in ' + f.int(r.products) + ' products' }, h('div', { class: 'fill', style: 'width:' + Math.max(2, (Number(r.stock_value) / top) * 100).toFixed(1) + '%' })),
        h('span', { class: 'amt' }, f.money(r.stock_value))));
    }
  }

  // load all three data sets and redraw
  async function refresh() {
    const [dash, alerts, txns, cats] = await Promise.all([IMS.api.get('/api/dashboard'), IMS.api.get('/api/alerts/low-stock'), IMS.api.get('/api/transactions/recent'), IMS.api.get('/api/reports/stock-value')]);
    if (!dash.ok) { IMS.ui.toast(IMS.api.message(dash), 'error'); return; }
    const d = dash.data;
    IMS.ui.clear(syncBox);
    if (!d.in_sync) {                                              // memory and database disagree: say so, offer the fix
      syncBox.appendChild(h('div', { class: 'banner' }, 'The database was changed outside this app: it holds ' + f.int(d.database.products) + ' products and ' +
        f.int(d.database.units) + ' units, this server shows ' + f.int(d.products) + ' products and ' + f.int(d.units) + ' units. ',
        user.role === 'ADMIN' ? IMS.ui.btn('Reload from database', { small: true, onClick: reload }) : 'Ask an administrator to reload.'));
    }
    IMS.ui.clear(cards).append(
      stat('Products', f.int(d.products), f.int(d.units) + ' units in stock', 'box'),
      stat('Stock value', f.money(d.stock_value), 'at selling price', 'rupee', 'tape'),
      stat('Sales this month', f.money(d.sales_this_month.revenue), f.int(d.sales_this_month.orders) + (d.sales_this_month.orders === 1 ? ' order' : ' orders'), 'trend', 'ok'),
      stat('Low stock', f.int(d.low_stock), d.low_stock ? 'products need restocking' : 'everything above minimum', 'alert', d.low_stock ? 'bad' : 'ok'));
    if (alerts.ok) alertTable.setRows(alerts.data);
    if (txns.ok) txnTable.setRows(txns.data);
    if (cats.ok && cats.data && Array.isArray(cats.data.rows)) drawCategories(cats.data.rows);
  }

  content.append(syncBox, cards, h('div', { class: 'grid-dash' },
    h('div', { class: 'stack' },
      h('section', { class: 'card' }, h('div', { class: 'card-h' }, h('h2', {}, 'Restock alerts'), IMS.ui.ads('Priority queue')), alertTable.el),
      h('section', { class: 'card' }, h('div', { class: 'card-h' }, h('h2', {}, 'Stock value by category'), h('span', { class: 'muted' }, 'at selling price')), catBars)),
    h('section', { class: 'card' }, h('div', { class: 'card-h' }, h('h2', {}, 'Recent stock transactions'), IMS.ui.ads('Stack'),
      user.role === 'ADMIN' ? IMS.ui.btn('Undo last', { small: true, title: 'Removes the newest sale or purchase made since the server started and puts its stock back', onClick: undo }) : null), txnTable.el)));
  await refresh();
});
