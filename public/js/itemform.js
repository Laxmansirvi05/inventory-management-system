// itemform.js : the "add several products" form used by Sales and Purchases.
// IMS.itemform.create({mode:'sale'|'purchase', products, onChange}) -> {el, items(), total(), validate(), hasStockProblem(), setAllowOversell(bool)}
window.IMS = window.IMS || {};

IMS.itemform = (function () {
  const h = IMS.ui.h, f = IMS.fmt;

  // build the form; products come from GET /api/products
  function create(opts) {
    const sale = opts.mode === 'sale';
    const byId = new Map(opts.products.map((p) => [p.id, p]));
    const rows = [];
    let allowOversell = false;
    const tbody = h('tbody', {});
    const totalEl = h('span', { class: 'big' }, f.money(0));

    // "SKU · name (in stock N)"
    const label = (p) => p.sku + ' · ' + p.name + (sale ? (p.stock_quantity > 0 ? '  (in stock: ' + p.stock_quantity + ')' : '  (out of stock)') : '');
    // purchase cost starts at 70 % of the selling price
    const defaultCost = (p) => Math.round(p.unit_price * 70) / 100;
    const qtyOf = (r) => Number(r.qty.value);

    // recompute one row's line total and stock warning, then the grand total
    function refresh(r) {
      const p = byId.get(Number(r.sel.value));
      const q = qtyOf(r), unit = sale ? p.unit_price : Number(r.cost.value);
      r.line.textContent = f.money(Number.isFinite(q * unit) && q > 0 ? q * unit : 0);
      r.bad = sale && Number.isInteger(q) && q > p.stock_quantity;
      r.warn.textContent = r.bad ? 'Only ' + p.stock_quantity + ' in stock' : '';
      totals();
    }

    // sum of all valid lines; tells the page something changed
    function totals() {
      let t = 0;
      for (const r of rows) {
        const q = qtyOf(r), unit = sale ? byId.get(Number(r.sel.value)).unit_price : Number(r.cost.value);
        if (Number.isInteger(q) && q > 0 && unit > 0) t += q * unit;
      }
      totalEl.textContent = f.money(Math.round(t * 100) / 100);
      if (opts.onChange) opts.onChange();
    }

    // add a product row
    function addRow() {
      const first = opts.products.find((p) => !sale || p.stock_quantity > 0) || opts.products[0];
      const r = { bad: false };
      r.sel = IMS.ui.select(opts.products.map((p) => ({ value: p.id, label: label(p) })), first.id, { 'aria-label': 'Product' });
      r.qty = h('input', { class: 'input', type: 'number', min: 1, step: 1, value: 1, 'aria-label': 'Quantity', style: 'width:90px' });
      r.cost = sale ? null : h('input', { class: 'input', type: 'number', min: 0.01, step: 0.01, value: defaultCost(first), 'aria-label': 'Unit cost', style: 'width:110px' });
      r.price = h('span', {}, sale ? f.money(first.unit_price) : null);
      r.line = h('span', {});
      r.warn = h('div', { class: 'warn-row' });
      r.sel.addEventListener('change', () => {
        const p = byId.get(Number(r.sel.value));
        if (sale) r.price.textContent = f.money(p.unit_price); else r.cost.value = defaultCost(p);
        refresh(r);
      });
      for (const el of [r.qty, r.cost]) if (el) el.addEventListener('input', () => refresh(r));
      r.tr = h('tr', {}, h('td', { style: 'min-width:280px' }, r.sel, r.warn), h('td', {}, r.qty), h('td', {}, sale ? r.price : r.cost),
        h('td', { class: 'num' }, r.line), h('td', {}, IMS.ui.btn('✕', { small: true, title: 'Remove item', onClick: () => {
          if (rows.length === 1) return;
          rows.splice(rows.indexOf(r), 1); r.tr.remove(); totals();
        } })));
      rows.push(r);
      tbody.appendChild(r.tr);
      refresh(r);
    }

    // back to a single fresh row
    function reset() { rows.length = 0; IMS.ui.clear(tbody); addRow(); }

    const el = h('div', {}, h('table', { class: 'items' },
      h('thead', {}, h('tr', {}, h('th', {}, 'Product'), h('th', {}, 'Quantity'), h('th', {}, sale ? 'Price' : 'Unit cost'), h('th', { class: 'num' }, 'Line total'), h('th', {}))), tbody),
      IMS.ui.btn('Add item', { small: true, onClick: addRow }),
      h('div', { class: 'total-bar' }, h('span', { class: 'muted' }, 'Total'), totalEl));
    addRow();

    return {
      el, reset,
      total: () => totalEl.textContent,
      items: () => rows.map((r) => Object.assign({ product_id: Number(r.sel.value), quantity: qtyOf(r) }, sale ? {} : { unit_cost: Number(r.cost.value) })),
      validate: () => {
        for (const r of rows) {
          if (!Number.isInteger(qtyOf(r)) || qtyOf(r) < 1) return 'Quantity must be a whole number, 1 or more';
          if (!sale && !(Number(r.cost.value) > 0)) return 'Unit cost must be above 0';
        }
        return '';
      },
      hasStockProblem: () => sale && !allowOversell && rows.some((r) => r.bad),
      setAllowOversell: (v) => { allowOversell = v; if (opts.onChange) opts.onChange(); },
    };
  }

  return { create };
})();
