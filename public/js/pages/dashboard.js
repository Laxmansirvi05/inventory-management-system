// dashboard.js : stat cards, restock alerts (priority queue), recent stock transactions, Undo last (stack).
IMS.boot(async (content, user) => {
  const h = IMS.ui.h, f = IMS.fmt;
  const cards = h('div', { class: 'stats' });
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

  // undo the newest stock transaction (admin only)
  async function undo() {
    if (!(await IMS.ui.confirmDialog('Undo the last stock transaction? Stock is reversed and an UNDO row is added to the ledger.', 'Undo', true))) return;
    const res = await IMS.api.post('/api/undo');
    if (res.ok) { IMS.ui.toast('Undone. Stock is now ' + res.data.stock_quantity + ' for product ' + res.data.product_id); refresh(); }
    else IMS.ui.toast(IMS.api.message(res), 'error');
  }

  // one stat card
  const stat = (label, value, sub) => h('div', { class: 'card stat' }, h('div', { class: 'label' }, label), h('div', { class: 'value' }, value), h('div', { class: 'sub' }, sub));

  // load all three data sets and redraw
  async function refresh() {
    const [dash, alerts, txns] = await Promise.all([IMS.api.get('/api/dashboard'), IMS.api.get('/api/alerts/low-stock'), IMS.api.get('/api/transactions/recent')]);
    if (!dash.ok) { IMS.ui.toast(IMS.api.message(dash), 'error'); return; }
    const d = dash.data;
    IMS.ui.clear(cards).append(
      stat('Products', f.int(d.products), 'in catalogue'),
      stat('Stock value', f.money(d.stock_value), 'at selling price'),
      stat('Sales this month', f.money(d.sales_this_month.revenue), f.int(d.sales_this_month.orders) + ' orders'),
      stat('Low stock', f.int(d.low_stock), d.low_stock ? 'need restocking' : 'all good'));
    if (alerts.ok) alertTable.setRows(alerts.data);
    if (txns.ok) txnTable.setRows(txns.data);
  }

  content.append(cards, h('div', { class: 'grid-2' },
    h('section', { class: 'card' }, h('div', { class: 'card-h' }, h('h2', {}, 'Restock alerts'), IMS.ui.ads('Priority queue')), alertTable.el),
    h('section', { class: 'card' }, h('div', { class: 'card-h' }, h('h2', {}, 'Recent stock transactions'), IMS.ui.ads('Stack'),
      user.role === 'ADMIN' ? IMS.ui.btn('Undo last', { small: true, onClick: undo }) : null), txnTable.el)));
  await refresh();
});
