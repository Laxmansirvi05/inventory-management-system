// ui.js : DOM builder, icons, buttons, toast, modal, confirm and a sortable/paged table.
// Text is always inserted with textContent / text nodes, never innerHTML, so data cannot inject markup.
window.IMS = window.IMS || {};

IMS.ui = (function () {
  const SVG_NS = 'http://www.w3.org/2000/svg';
  const ICONS = {   // 24x24 line icons (static paths)
    layers: 'M12 2l10 5-10 5L2 7z M2 17l10 5 10-5 M2 12l10 5 10-5',
    grid: 'M3 3h7v7H3z M14 3h7v7h-7z M14 14h7v7h-7z M3 14h7v7H3z',
    box: 'M21 8l-9-5-9 5v8l9 5 9-5z M3 8l9 5 9-5 M12 13v8',
    tag: 'M20.6 13.4l-7.2 7.2a2 2 0 0 1-2.8 0L3 13V3h10l7.6 7.6a2 2 0 0 1 0 2.8z M7.5 7.5h.01',
    users: 'M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2 M9 11a4 4 0 1 0 0-8 4 4 0 0 0 0 8z M23 21v-2a4 4 0 0 0-3-3.9 M16 3.1a4 4 0 0 1 0 7.8',
    truck: 'M1 3h15v13H1z M16 8h4l3 3v5h-7z M5.5 21a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5z M18.5 21a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5z',
    cart: 'M9 22a1 1 0 1 0 0-2 1 1 0 0 0 0 2z M20 22a1 1 0 1 0 0-2 1 1 0 0 0 0 2z M1 1h4l2.7 13.4a2 2 0 0 0 2 1.6h9.7a2 2 0 0 0 2-1.6L23 6H6',
    chart: 'M18 20V10 M12 20V4 M6 20v-6',
    key: 'M21 2l-2 2m-7.6 7.6a5.5 5.5 0 1 1-7.8 7.8 5.5 5.5 0 0 1 7.8-7.8z M15.5 7.5l3 3L22 7l-3-3',
    db: 'M12 8c4.97 0 9-1.34 9-3s-4.03-3-9-3-9 1.34-9 3 4.03 3 9 3z M21 12c0 1.66-4 3-9 3s-9-1.34-9-3 M3 5v14c0 1.66 4 3 9 3s9-1.34 9-3V5',
    logout: 'M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4 M16 17l5-5-5-5 M21 12H9',
    rupee: 'M6 3h12 M6 8h12 M6 13l8.5 8 M6 13h3 M9 13c6.67 0 6.67-10 0-10',
    trend: 'M22 7l-8.5 8.5-5-5L2 17 M16 7h6v6',
    alert: 'M10.3 3.9L1.8 18a2 2 0 0 0 1.7 3h17a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z M12 9v4 M12 17h.01',
    list: 'M8 6h13 M8 12h13 M8 18h13 M3 6h.01 M3 12h.01 M3 18h.01',
    tiles: 'M3 3h7v7H3z M14 3h7v7h-7z M14 14h7v7h-7z M3 14h7v7H3z',
    pie: 'M21.2 15.9A10 10 0 1 1 8 2.8 M22 12A10 10 0 0 0 12 2v10z',
  };

  // h('div', {class:'x', on:{click:fn}}, 'text', childNode, [more]) -> element
  function h(tag, attrs, ...children) {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs || {})) {
      if (v === null || v === undefined || v === false) continue;
      if (k === 'on') for (const [ev, fn] of Object.entries(v)) el.addEventListener(ev, fn);
      else if (k === 'class') el.className = v;
      else if (k === 'value' || k === 'checked' || k === 'disabled' || k === 'selected') el[k] = v;
      else el.setAttribute(k, v === true ? '' : v);
    }
    append(el, children);
    return el;
  }

  // add strings as text nodes, nodes as they are, arrays recursively; skip empty values
  function append(el, children) {
    for (const c of children) {
      if (c === null || c === undefined || c === false) continue;
      if (Array.isArray(c)) append(el, c);
      else el.appendChild(c instanceof Node ? c : document.createTextNode(String(c)));
    }
  }

  // remove all children of a node
  function clear(el) { while (el.firstChild) el.removeChild(el.firstChild); return el; }

  // small SVG icon by name
  function icon(name) {
    const svg = document.createElementNS(SVG_NS, 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    for (const [k, v] of [['fill', 'none'], ['stroke', 'currentColor'], ['stroke-width', '2'], ['stroke-linecap', 'round'], ['stroke-linejoin', 'round']]) svg.setAttribute(k, v);
    const p = document.createElementNS(SVG_NS, 'path');
    p.setAttribute('d', ICONS[name] || '');
    svg.appendChild(p);
    return svg;
  }

  // button; an onClick that returns a promise shows a spinner and blocks double clicks
  function btn(label, opts) {
    const o = opts || {};
    const b = h('button', { type: o.type || 'button', class: 'btn' + (o.variant ? ' ' + o.variant : '') + (o.small ? ' sm' : ''), title: o.title, disabled: o.disabled }, label);
    if (o.onClick) b.addEventListener('click', async (e) => {
      if (b.disabled) return;
      b.disabled = true; b.classList.add('loading');
      try { await o.onClick(e); } finally { b.disabled = !!o.disabled; b.classList.remove('loading'); }
    });
    return b;
  }

  // label + input (+ hint) block
  function field(label, input, hint) {
    return h('div', { class: 'field' }, h('label', {}, label), input, hint ? h('span', { class: 'hint' }, hint) : null);
  }

  // <select> from [{value,label}] with the given value selected
  function select(options, value, attrs) {
    const s = h('select', Object.assign({ class: 'input' }, attrs || {}));
    for (const o of options) s.appendChild(h('option', { value: o.value, selected: String(o.value) === String(value) }, o.label));
    return s;
  }

  // colored label
  function tag(text, kind) { return h('span', { class: 'tag' + (kind ? ' ' + kind : '') }, text); }

  // small "ADS" chip with a tooltip naming the data structure shown
  function ads(text) { return h('span', { class: 'ads', title: text }, 'ADS · ' + text.split(' / ')[0]); }

  // message box that disappears by itself
  function toast(message, kind) {
    let wrap = document.querySelector('.toasts');
    if (!wrap) { wrap = h('div', { class: 'toasts', role: 'status' }); document.body.appendChild(wrap); }
    const t = h('div', { class: 'toast' + (kind === 'error' ? ' error' : kind === 'info' ? ' info' : '') }, message);
    wrap.appendChild(t);
    setTimeout(() => t.remove(), kind === 'error' ? 8000 : 4500);
  }

  // dialog: modal({title, content, buttons:[{label, variant, onClick(api)}], wide}) -> {close, setError}
  function modal(opts) {
    const errBox = h('div', { class: 'form-error', style: 'display:none' });
    const api = {
      close() { back.remove(); document.removeEventListener('keydown', onKey); },
      setError(msg) { errBox.textContent = msg || ''; errBox.style.display = msg ? '' : 'none'; },
    };
    const foot = h('div', { class: 'modal-f' }, (opts.buttons || []).map((b) =>
      btn(b.label, { variant: b.variant, onClick: async () => { if ((await b.onClick(api)) !== false && b.close) api.close(); } })));
    const dlg = h('div', { class: 'modal' + (opts.wide ? ' wide' : ''), role: 'dialog', 'aria-modal': 'true' },
      h('div', { class: 'modal-h' }, h('h2', {}, opts.title), btn('✕', { small: true, title: 'Close', onClick: () => api.close() })),
      h('div', { class: 'modal-b' }, errBox, opts.content), opts.buttons && opts.buttons.length ? foot : null);
    const back = h('div', { class: 'modal-back', on: { mousedown: (e) => { if (e.target === back) api.close(); } } }, dlg);
    const onKey = (e) => { if (e.key === 'Escape') api.close(); };
    document.addEventListener('keydown', onKey);
    document.body.appendChild(back);
    const first = dlg.querySelector('input:not([type=hidden]), select, textarea');
    if (first) first.focus();
    return api;
  }

  // "Are you sure?" -> Promise<boolean>
  function confirmDialog(message, okLabel, danger) {
    return new Promise((resolve) => {
      modal({ title: 'Please confirm', content: h('p', { style: 'margin:0' }, message),
        buttons: [{ label: 'Cancel', close: true, onClick: () => resolve(false) },
                  { label: okLabel || 'Confirm', variant: danger ? 'danger' : 'primary', close: true, onClick: () => resolve(true) }] });
    });
  }

  // Table with clickable sortable headers and 10-row pages.
  // cfg: {columns:[{key,label,sortable,num,render(row)}], pageSize, empty, onSort(key,dir) -> server-side sorting}
  class DataTable {
    constructor(cfg) {
      this.cfg = cfg; this.rows = []; this.page = 0; this.sortKey = cfg.sortKey || null; this.dir = cfg.sortDir || 'asc';
      this.el = h('div', {});
    }
    // replace the data and show page 1
    setRows(rows) { this.rows = rows.slice(); this.page = 0; this.render(); }
    // header click: flip direction, then sort here or ask the server
    sortBy(key) {
      this.dir = this.sortKey === key && this.dir === 'asc' ? 'desc' : 'asc';
      this.sortKey = key;
      if (this.cfg.onSort) { this.cfg.onSort(key, this.dir); this.render(); return; }
      const f = this.dir === 'asc' ? 1 : -1;
      this.rows.sort((a, b) => (a[key] > b[key] ? f : a[key] < b[key] ? -f : 0));
      this.render();
    }
    render() {
      const c = this.cfg, size = c.pageSize || 10, pages = Math.max(1, Math.ceil(this.rows.length / size));
      this.page = Math.min(this.page, pages - 1);
      const slice = this.rows.slice(this.page * size, this.page * size + size);
      const head = h('tr', {}, c.columns.map((col) => h('th', {
        class: (col.num ? 'num ' : '') + (col.sortable ? 'sortable' : ''), 'aria-sort': col.sortable && this.sortKey === col.key ? (this.dir === 'asc' ? 'ascending' : 'descending') : null,
        on: col.sortable ? { click: () => this.sortBy(col.key) } : {} },
        col.label, col.sortable && this.sortKey === col.key ? h('span', { class: 'arrow' }, this.dir === 'asc' ? ' ▲' : ' ▼') : null)));
      const body = slice.map((r) => h('tr', {}, c.columns.map((col) =>
        h('td', { class: col.num ? 'num' : '' }, col.render ? col.render(r) : (r[col.key] === null || r[col.key] === undefined ? '—' : String(r[col.key]))))));
      clear(this.el);
      if (!this.rows.length) { this.el.appendChild(h('div', { class: 'empty' }, c.empty || 'Nothing to show')); return; }
      this.el.appendChild(h('div', { class: 'table-wrap' }, h('table', { class: 'tbl' }, h('thead', {}, head), h('tbody', {}, body))));
      if (this.rows.length > size) {
        this.el.appendChild(h('div', { class: 'pager' },
          h('span', { class: 'grow' }, 'Showing ' + (this.page * size + 1) + '–' + (this.page * size + slice.length) + ' of ' + this.rows.length),
          btn('Previous', { small: true, disabled: this.page === 0, onClick: () => { this.page--; this.render(); } }),
          h('span', {}, 'Page ' + (this.page + 1) + ' / ' + pages),
          btn('Next', { small: true, disabled: this.page >= pages - 1, onClick: () => { this.page++; this.render(); } })));
      }
    }
  }

  return { h, clear, icon, btn, field, select, tag, ads, toast, modal, confirmDialog, DataTable };
})();
