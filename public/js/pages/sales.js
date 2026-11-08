// sales.js : new sale form, FIFO order queue card and sales history. Each sale runs as one MySQL transaction.
IMS.boot(async (content, user) => {
  const h = IMS.ui.h, f = IMS.fmt, admin = user.role === 'ADMIN';
  let customers = [], form = null, products = [];
  const formBox = h('div', {});
  const err = h('div', { class: 'form-error', style: 'display:none', role: 'alert' });
  const customerSel = IMS.ui.select([], '', { 'aria-label': 'Customer' });
  const saveBtn = IMS.ui.btn('Save sale', { variant: 'primary', onClick: save });
  const force = h('input', { type: 'checkbox', on: { change: () => form.setAllowOversell(force.checked) } });
  const names = () => new Map(customers.map((c) => [c.customer_id, c.customer_name]));
  const statusKind = { done: 'ok', failed: 'bad', pending: 'warn', undone: 'warn' };

  const queue = new IMS.ui.DataTable({ pageSize: 20, empty: 'No orders yet', columns: [
    { label: 'Order', render: (o) => '#' + o.order_id },
    { label: 'Customer', render: (o) => names().get(o.customer_id) || 'Customer ' + o.customer_id },
    { label: 'Items', key: 'items', num: true },
    { label: 'Status', render: (o) => [IMS.ui.tag(o.status, statusKind[o.status]), o.status === 'failed' ? h('div', { class: 'muted', style: 'font-size:12px' }, o.message) : null] },
    { label: 'Total', num: true, render: (o) => (o.sale_id ? f.money(o.total) : '—') },
  ] });
  const history = new IMS.ui.DataTable({ pageSize: 10, empty: 'No sales yet', columns: [
    { label: 'Sale', key: 'sale_id', sortable: true, render: (s) => '#' + s.sale_id },
    { label: 'Date', key: 'sale_date', sortable: true, render: (s) => f.date(s.sale_date) },
    { label: 'Customer', key: 'customer_name', sortable: true },
    { label: 'Units', key: 'units_sold', num: true },
    { label: 'Total', key: 'total_amount', sortable: true, num: true, render: (s) => f.money(s.total_amount) },
    { label: '', render: (s) => h('a', { class: 'btn sm', href: 'bill.html?sale_id=' + s.sale_id }, 'Bill') },
  ] });

  // enable Save only when the form is valid and stock is enough
  function syncSave() { saveBtn.disabled = !form || form.hasStockProblem(); }

  // fresh product list (stock numbers change after every sale) and an empty form
  async function newForm() {
    const res = await IMS.api.get('/api/products');
    products = res.ok ? res.data : [];
    IMS.ui.clear(formBox);
    form = IMS.itemform.create({ mode: 'sale', products, onChange: syncSave });
    formBox.appendChild(form.el);
    force.checked = false;
    syncSave();
  }

  // today's sales in three numbers (worked out from the history rows the page already has)
  const todayBox = h('div', { class: 'today' });
  function drawToday(rows) {
    const t = rows.filter((r) => String(r.sale_date).slice(0, 10) === f.today());
    const cell = (label, value) => h('div', {}, h('span', {}, label), h('b', {}, value));
    IMS.ui.clear(todayBox).append(cell('Sales today', f.int(t.length)),
      cell('Revenue today', f.money(t.reduce((a, r) => a + Number(r.total_amount || 0), 0))),
      cell('Units sold', f.int(t.reduce((a, r) => a + Number(r.units_sold || 0), 0))));
  }

  // reload the queue card and the history table
  async function loadSide() {
    const [q, s] = await Promise.all([IMS.api.get('/api/sales/queue'), IMS.api.get('/api/sales')]);
    if (q.ok) queue.setRows(q.data.orders);
    if (s.ok) { history.setRows(s.data); drawToday(s.data); }
  }

  // send the sale; the server queues it (FIFO) and runs the transaction
  async function save() {
    err.style.display = 'none';
    const bad = form.validate();
    if (bad) { err.textContent = bad; err.style.display = ''; return; }
    const res = await IMS.api.post('/api/sales', { customer_id: Number(customerSel.value), items: form.items() });
    if (!res.ok) {
      err.textContent = IMS.api.message(res) + (res.sql_log.length ? '  The transaction was rolled back.' : '');
      err.style.display = '';
      IMS.ui.toast(IMS.api.message(res), 'error');
      await loadSide();
      return;
    }
    const id = res.data.sale_id;
    IMS.ui.modal({ title: 'Sale saved', content: h('p', { style: 'margin:0' }, 'Sale #' + id + ' for ' + f.money(res.data.total) + ' was recorded. Stock and the ledger were updated by the triggers.'),
      buttons: [{ label: 'New sale', close: true, onClick: () => {} }, { label: 'Open bill', variant: 'primary', onClick: () => { location.href = 'bill.html?sale_id=' + id; } }] });
    await Promise.all([newForm(), loadSide()]);
  }

  const c = await IMS.api.get('/api/customers');
  customers = c.ok ? c.data : [];
  const walkIn = customers.find((x) => x.customer_id === 1);
  for (const cu of customers) customerSel.appendChild(h('option', { value: cu.customer_id, selected: walkIn && cu.customer_id === walkIn.customer_id }, cu.customer_name));
  await newForm();

  content.append(h('div', { class: 'grid-sales' },
    h('section', { class: 'card' }, h('div', { class: 'card-h' }, h('h2', {}, 'New sale')),
      h('div', { class: 'card-b' }, err,
        h('div', { class: 'form-grid' }, IMS.ui.field('Customer', customerSel),
          IMS.ui.field('Date', h('input', { class: 'input', type: 'date', value: f.today(), disabled: true }))),
        formBox,
        admin ? h('label', { style: 'display:flex;gap:8px;align-items:center;margin-top:12px;color:var(--muted)' }, force, 'Demo: send even if stock is too low (shows the trigger error and ROLLBACK)') : null,
        h('div', { style: 'display:flex;justify-content:flex-end;margin-top:14px' }, saveBtn))),
    h('div', { class: 'stack' },
      h('section', { class: 'card' }, h('div', { class: 'card-h' }, h('h2', {}, 'Today'), h('span', { class: 'muted' }, f.date(f.today()))), todayBox),
      h('section', { class: 'card' }, h('div', { class: 'card-h' }, h('h2', {}, 'Order queue'), IMS.ui.ads('Queue (first in, first out)')),
        h('div', { style: 'max-height:420px;overflow:auto' }, queue.el)))),
    h('section', { class: 'card', style: 'margin-top:22px' }, h('div', { class: 'card-h' }, h('h2', {}, 'Sales history')), history.el));
  await loadSide();
});
