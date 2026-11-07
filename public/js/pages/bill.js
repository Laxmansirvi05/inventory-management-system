// bill.js : printable bill for one sale (bill.html?sale_id=N).
IMS.boot(async (content) => {
  const h = IMS.ui.h, f = IMS.fmt;
  const id = new URLSearchParams(location.search).get('sale_id');
  const res = id && /^\d+$/.test(id) ? await IMS.api.get('/api/sales/' + id + '/bill') : null;
  if (!res || !res.ok) {
    content.appendChild(h('div', { class: 'card card-b' }, res ? IMS.api.message(res) : 'No sale selected.', ' ', h('a', { href: 'sales.html' }, 'Back to sales')));
    return;
  }
  const { shop, sale, lines } = res.data;
  const rows = lines.map((l) => h('tr', {}, h('td', {}, l.product_name, h('div', { class: 'muted' }, l.sku)),
    h('td', { class: 'num' }, l.quantity), h('td', { class: 'num' }, f.money(l.unit_price)), h('td', { class: 'num' }, f.money(l.line_total))));
  content.append(
    h('div', { class: 'no-print', style: 'display:flex;gap:10px;margin-bottom:16px' },
      IMS.ui.btn('Print bill', { variant: 'primary', onClick: () => window.print() }), h('a', { class: 'btn', href: 'sales.html' }, 'Back to sales')),
    h('div', { class: 'card bill' },
      h('div', { class: 'bill-head' }, h('h2', {}, shop), h('div', { class: 'muted' }, 'Sales bill')),
      h('div', { class: 'bill-meta' }, h('div', {}, h('b', {}, 'Bill #' + sale.sale_id), h('div', { class: 'muted' }, f.date(sale.sale_date))),
        h('div', { style: 'text-align:right' }, h('b', {}, sale.customer_name), h('div', { class: 'muted' }, sale.phone))),
      h('table', { class: 'tbl' }, h('thead', {}, h('tr', {}, h('th', {}, 'Item'), h('th', { class: 'num' }, 'Qty'), h('th', { class: 'num' }, 'Price'), h('th', { class: 'num' }, 'Amount'))), h('tbody', {}, rows)),
      h('div', { class: 'bill-total' }, 'Total ' + f.money(sale.total_amount)),
      h('p', { class: 'muted', style: 'text-align:center;margin:24px 0 0' }, 'Thank you for your purchase')));
});
