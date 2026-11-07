// purchases.js : record stock coming in from a supplier (several items), plus the purchase history.
IMS.boot(async (content) => {
  const h = IMS.ui.h, f = IMS.fmt;
  let form = null;
  const formBox = h('div', {});
  const err = h('div', { class: 'form-error', style: 'display:none', role: 'alert' });
  const supplierSel = IMS.ui.select([], '', { 'aria-label': 'Supplier' });
  const history = new IMS.ui.DataTable({ pageSize: 10, empty: 'No purchases yet', columns: [
    { label: 'Purchase', key: 'purchase_id', sortable: true, render: (p) => '#' + p.purchase_id },
    { label: 'Date', key: 'purchase_date', sortable: true, render: (p) => f.date(p.purchase_date) },
    { label: 'Supplier', key: 'supplier_name', sortable: true },
    { label: 'Lines', key: 'item_lines', num: true },
    { label: 'Total', key: 'total_amount', sortable: true, num: true, render: (p) => f.money(p.total_amount) },
  ] });

  // fresh product list and an empty form
  async function newForm() {
    const res = await IMS.api.get('/api/products');
    IMS.ui.clear(formBox);
    form = IMS.itemform.create({ mode: 'purchase', products: res.ok ? res.data : [] });
    formBox.appendChild(form.el);
  }

  // reload the history table
  async function loadHistory() {
    const res = await IMS.api.get('/api/purchases');
    if (res.ok) history.setRows(res.data);
  }

  // send the purchase; the trigger raises stock inside the transaction
  async function save() {
    err.style.display = 'none';
    const bad = form.validate();
    if (bad) { err.textContent = bad; err.style.display = ''; return; }
    const res = await IMS.api.post('/api/purchases', { supplier_id: Number(supplierSel.value), items: form.items() });
    if (!res.ok) { err.textContent = IMS.api.message(res); err.style.display = ''; IMS.ui.toast(IMS.api.message(res), 'error'); return; }
    IMS.ui.toast('Purchase #' + res.data.purchase_id + ' saved: ' + f.money(res.data.total) + '. Stock increased.');
    await Promise.all([newForm(), loadHistory()]);
  }

  const s = await IMS.api.get('/api/suppliers');
  for (const sp of s.ok ? s.data : []) supplierSel.appendChild(h('option', { value: sp.supplier_id }, sp.supplier_name));
  await newForm();
  content.append(
    h('section', { class: 'card' }, h('div', { class: 'card-h' }, h('h2', {}, 'New purchase (stock in)')),
      h('div', { class: 'card-b' }, err, h('div', { class: 'form-grid' }, IMS.ui.field('Supplier', supplierSel),
        IMS.ui.field('Date', h('input', { class: 'input', type: 'date', value: f.today(), disabled: true }))),
      formBox, h('div', { style: 'display:flex;justify-content:flex-end;margin-top:14px' }, IMS.ui.btn('Save purchase', { variant: 'primary', onClick: save })))),
    h('section', { class: 'card', style: 'margin-top:20px' }, h('div', { class: 'card-h' }, h('h2', {}, 'Purchase history')), history.el));
  await loadHistory();
});
