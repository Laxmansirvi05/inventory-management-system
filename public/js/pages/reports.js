// reports.js : pick a report on the left, see its table on the right, export it as CSV (done in the browser).
IMS.boot(async (content) => {
  const h = IMS.ui.h, f = IMS.fmt;
  const MONEY = /value|revenue|spent|bought|amount|price|cost/;   // columns shown as rupees
  const nav = h('div', { class: 'list-nav card', role: 'tablist' });
  const title = h('h2', {}, 'Report');
  const count = h('span', { class: 'muted' });
  const out = h('div', {});
  let current = null;                                              // {name, title, columns, rows}

  // "stock_value" -> "Stock value"
  const heading = (c) => { const t = c.replace(/_/g, ' ').replace(/\bid\b/g, 'ID').replace(/\bsku\b/gi, 'SKU'); return t.charAt(0).toUpperCase() + t.slice(1); };

  // draw one report result
  function show(r) {
    current = r;
    title.textContent = r.title;
    count.textContent = r.rows.length + (r.rows.length === 1 ? ' row' : ' rows');
    const table = new IMS.ui.DataTable({ pageSize: 10, empty: 'This report has no rows', columns: r.columns.map((c) => {
      const numeric = r.rows.length && typeof r.rows[0][c] === 'number';
      return { label: heading(c), key: c, sortable: true, num: numeric, render: (row) => (row[c] === null ? '—' : numeric && MONEY.test(c) ? f.money(row[c]) : numeric ? f.int(row[c]) : String(row[c])) };
    }) });
    IMS.ui.clear(out).appendChild(table.el);
    table.setRows(r.rows);
  }

  // CSV text of the current report; cells that start with = + - @ get a quote so spreadsheets do not run them
  function csv() {
    const cell = (v) => { let s = v === null ? '' : String(v); if (/^[=+\-@]/.test(s)) s = "'" + s; return '"' + s.replace(/"/g, '""') + '"'; };
    return [current.columns.map(cell).join(',')].concat(current.rows.map((r) => current.columns.map((c) => cell(r[c])).join(','))).join('\r\n');
  }

  // download the CSV
  function exportCsv() {
    const url = URL.createObjectURL(new Blob([csv()], { type: 'text/csv;charset=utf-8' }));
    const a = h('a', { href: url, download: current.name + '.csv' });
    document.body.appendChild(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  // load and show one report
  async function open(name, button) {
    nav.querySelectorAll('button').forEach((b) => { b.classList.toggle('active', b === button); b.setAttribute('aria-selected', b === button); });
    const res = await IMS.api.get('/api/reports/' + name);
    if (res.ok) show(res.data); else IMS.ui.toast(IMS.api.message(res), 'error');
  }

  const list = await IMS.api.get('/api/reports');
  if (!list.ok) { IMS.ui.toast(IMS.api.message(list), 'error'); return; }
  for (const r of list.data) nav.appendChild(h('button', { type: 'button', role: 'tab', on: { click: (e) => open(r.name, e.currentTarget) } }, r.title));
  content.append(h('div', { class: 'grid-side' }, nav,
    h('section', { class: 'card' }, h('div', { class: 'card-h' }, title, count, IMS.ui.btn('Export CSV', { small: true, onClick: () => current && exportCsv() })), out)));
  if (list.data.length) await open(list.data[0].name, nav.firstChild);
});
