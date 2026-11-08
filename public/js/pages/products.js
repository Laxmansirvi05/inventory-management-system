// products.js : product table with search (name/id), category filter, sort, add/edit/delete and photo upload.
// Search and sort run on the server in C++ (hash map, linear search, merge sort); this page only asks.
IMS.boot(async (content, user) => {
  const h = IMS.ui.h, f = IMS.fmt, admin = user.role === 'ADMIN';
  const SORTS = [['', 'Default order'], ['price:asc', 'Price: low to high'], ['price:desc', 'Price: high to low'], ['qty:asc', 'Stock: low to high'],
    ['qty:desc', 'Stock: high to low'], ['name:asc', 'Name: A to Z'], ['name:desc', 'Name: Z to A']];
  const API_SORT = { name: 'name', unit_price: 'price', stock_quantity: 'qty' };   // table column -> API sort field
  const state = { search: '', category: '', sort: '', order: 'asc' };
  let categories = [], suppliers = [], timer = null;

  const count = h('span', { class: 'muted' });
  const search = h('input', { class: 'input grow', type: 'search', placeholder: 'Search by name, SKU or product id', 'aria-label': 'Search products',
    on: { input: () => { clearTimeout(timer); timer = setTimeout(() => { state.search = search.value; load(); }, 250); } } });
  const catSel = IMS.ui.select([{ value: '', label: 'All categories' }], '', { 'aria-label': 'Category' });
  catSel.addEventListener('change', () => { state.category = catSel.value; load(); });
  const sortSel = IMS.ui.select(SORTS.map((s) => ({ value: s[0], label: s[1] })), '', { 'aria-label': 'Sort' });
  sortSel.addEventListener('change', () => { const [s, o] = sortSel.value.split(':'); state.sort = s || ''; state.order = o || 'asc'; table.sortKey = null; load(); });

  // table: header clicks ask the server to sort
  const table = new IMS.ui.DataTable({ pageSize: 10, empty: 'No products found', onSort: (key, dir) => {
    state.sort = API_SORT[key]; state.order = dir; sortSel.value = state.sort + ':' + dir; load();
  }, columns: [
    { label: 'Product', key: 'name', sortable: true, render: (p) => h('div', { class: 'pcell' }, thumb(p), h('div', {}, h('b', {}, p.name), h('span', { class: 'muted' }, p.sku))) },
    { label: 'Category', key: 'category_name' },
    { label: 'Supplier', key: 'supplier_name' },
    { label: 'Price', key: 'unit_price', sortable: true, num: true, render: (p) => f.money(p.unit_price) },
    { label: 'Stock', key: 'stock_quantity', sortable: true, num: true, render: (p) => [level(p) ? IMS.ui.tag(level(p).label, level(p).tone) : null, ' ', h('b', {}, String(p.stock_quantity))] },
    { label: 'Min', key: 'min_stock_level', num: true },
    { label: 'Expiry', render: (p) => (p.expiry_date ? f.date(p.expiry_date) : '—') },
    admin ? { label: '', render: (p) => h('div', { class: 'row-actions' }, IMS.ui.btn('Edit', { small: true, onClick: () => form(p) }),
      IMS.ui.btn('Delete', { small: true, variant: 'danger', onClick: () => remove(p) })) } : null,
  ].filter(Boolean) });

  // stock status: Critical at half the minimum or less, Low at the minimum or less
  function level(p) {
    if (p.stock_quantity > p.min_stock_level) return null;
    return p.stock_quantity * 2 <= p.min_stock_level ? { label: 'Critical', tone: 'bad' } : { label: 'Low', tone: 'warn' };
  }

  let photoVer = 1;                                                // bumped after an upload so the browser fetches the new photo
  const photoUrl = (p) => (p.has_photo && !IMS.api.isMock() ? '/api/products/' + p.id + '/photo?v=' + photoVer : null);

  // photo thumbnail, or a letter box when there is no photo
  function thumb(p) {
    return photoUrl(p)
      ? h('img', { class: 'thumb', src: photoUrl(p), alt: '', loading: 'lazy' })
      : h('span', { class: 'thumb', style: 'display:inline-grid;place-items:center;font-weight:800;color:var(--muted)' }, p.name.slice(0, 1).toUpperCase());
  }

  // grid view: one card per product with its photo, price and a stock meter (stock against three times the minimum)
  const grid = h('div', { class: 'pgrid' });
  let rows = [];
  function card(p) {
    const lv = level(p);
    const pct = Math.min(100, Math.round((p.stock_quantity / Math.max(1, p.min_stock_level * 3)) * 100));
    return h('article', { class: 'pcard' },
      h('div', { class: 'pic' },
        photoUrl(p) ? h('img', { src: photoUrl(p), alt: p.name, loading: 'lazy' }) : h('span', { class: 'letter' }, p.name.slice(0, 1).toUpperCase()),
        h('span', { class: 'tag' }, p.category_name),
        lv ? IMS.ui.tag(lv.label, lv.tone) : null),
      h('div', { class: 'info' },
        h('div', { class: 'name' }, p.name),
        h('div', { class: 'sku' }, p.sku + ' · ' + p.supplier_name),
        h('div', { class: 'line' }, h('span', { class: 'price' }, f.money(p.unit_price)), h('span', { class: 'stock' }, f.int(p.stock_quantity) + ' in stock, min ' + p.min_stock_level)),
        h('div', { class: 'meter', title: 'Stock against three times the minimum level' }, h('i', { class: lv ? lv.tone : '', style: 'width:' + Math.max(3, pct) + '%' }))),
      admin ? h('div', { class: 'acts' }, IMS.ui.btn('Edit', { small: true, onClick: () => form(p) }), IMS.ui.btn('Delete', { small: true, variant: 'danger', onClick: () => remove(p) })) : null);
  }

  // table or grid (the choice is remembered in this browser)
  let view = 'grid';
  try { view = localStorage.getItem('productsView') || 'grid'; } catch (e) { /* storage blocked */ }
  const body = h('div', {});
  const segBtn = (v, icon, label) => h('button', { type: 'button', 'data-v': v, 'aria-pressed': String(view === v), class: view === v ? 'on' : '', on: { click: () => setView(v) } }, IMS.ui.icon(icon), label);
  const seg = h('div', { class: 'seg', role: 'group', 'aria-label': 'View' }, segBtn('grid', 'tiles', 'Grid'), segBtn('table', 'list', 'Table'));
  function setView(v) {
    view = v;
    try { localStorage.setItem('productsView', v); } catch (e) { /* storage blocked */ }
    seg.querySelectorAll('button').forEach((b) => { const on = b.dataset.v === v; b.classList.toggle('on', on); b.setAttribute('aria-pressed', String(on)); });
    draw();
  }
  function draw() {
    IMS.ui.clear(body);
    if (view === 'table') { body.appendChild(table.el); return; }
    IMS.ui.clear(grid);
    if (!rows.length) { body.appendChild(h('div', { class: 'empty' }, 'No products found')); return; }
    rows.forEach((p) => grid.appendChild(card(p)));
    body.appendChild(grid);
  }

  // ask the server for the current search/filter/sort
  async function load() {
    const q = new URLSearchParams();
    for (const k of ['search', 'category', 'sort']) if (state[k]) q.set(k, state[k]);
    if (state.sort) q.set('order', state.order);
    const res = await IMS.api.get('/api/products' + (q.toString() ? '?' + q : ''));
    if (!res.ok) { IMS.ui.toast(IMS.api.message(res), 'error'); return; }
    rows = res.data;
    table.setRows(res.data);
    draw();
    count.textContent = res.data.length + (res.data.length === 1 ? ' product' : ' products');
  }

  // delete after confirmation; MySQL's foreign key error is shown if the product has history
  async function remove(p) {
    if (!(await IMS.ui.confirmDialog('Delete "' + p.name + '"? This cannot be undone.', 'Delete', true))) return;
    const res = await IMS.api.del('/api/products/' + p.id);
    if (res.ok) { IMS.ui.toast('Product deleted'); load(); } else IMS.ui.toast(IMS.api.message(res), 'error');
  }

  // add / edit dialog (p = product to edit, or null for a new one)
  function form(p) {
    const edit = !!p, errs = {};
    const inp = (id, attrs) => h('input', Object.assign({ class: 'input', id: 'pf-' + id }, attrs));
    const sku = inp('sku', { value: edit ? p.sku : '', maxlength: 20 }), name = inp('name', { value: edit ? p.name : '', maxlength: 80 });
    const cat = IMS.ui.select(categories.map((c) => ({ value: c.category_id, label: c.category_name })), edit ? p.category_id : (categories[0] || {}).category_id);
    const sup = IMS.ui.select(suppliers.map((s) => ({ value: s.supplier_id, label: s.supplier_name })), edit ? p.supplier_id : (suppliers[0] || {}).supplier_id);
    const price = inp('price', { type: 'number', step: '0.01', value: edit ? p.unit_price : '' });
    const stock = inp('stock', { type: 'number', step: '1', value: edit ? p.stock_quantity : 0, disabled: edit });
    const min = inp('min', { type: 'number', step: '1', value: edit ? p.min_stock_level : 10 });
    const exp = inp('exp', { type: 'date', value: edit && p.expiry_date ? p.expiry_date : '' });
    const desc = h('textarea', { class: 'input', rows: 2, maxlength: 150 }, edit ? p.description || '' : '');
    const photo = inp('photo', { type: 'file', accept: 'image/png,image/jpeg,image/gif,image/webp' });
    // field block with an inline error line
    const fld = (key, label, input, hint, cls) => { errs[key] = h('span', { class: 'err' }); const d = IMS.ui.field(label, input, hint); d.appendChild(errs[key]); if (cls) d.classList.add(cls); return d; };

    const dlg = IMS.ui.modal({ title: edit ? 'Edit product' : 'Add product', content: h('div', { class: 'form-grid' },
      fld('sku', 'SKU', sku), fld('name', 'Name', name), fld('cat', 'Category', cat), fld('sup', 'Supplier', sup),
      fld('price', 'Unit price (₹)', price), fld('stock', 'Stock quantity', stock, edit ? 'Changes through purchases and sales' : null), fld('min', 'Minimum stock', min), fld('exp', 'Expiry date (optional)', exp),
      fld('desc', 'Description (optional)', desc, null, 'wide'),
      fld('photo', 'Photo (optional, image up to 1 MB)', photo, edit && p.has_photo ? 'A photo is stored; choosing a file replaces it' : null, 'wide')),
    buttons: [{ label: 'Cancel', close: true, onClick: () => {} }, { label: edit ? 'Save changes' : 'Add product', variant: 'primary', onClick: save }] });

    // hash-set check while typing: tells at once that a SKU is taken (the server answers from memory, no SQL)
    sku.addEventListener('blur', async () => {
      const v = sku.value.trim();
      if (!v || (edit && v === p.sku)) return;
      const r = await IMS.api.get('/api/products/sku-check?sku=' + encodeURIComponent(v));
      errs.sku.textContent = r.ok && r.data.exists ? 'This SKU is already in use (checked with a hash set)' : '';
    });

    // check the inputs, send them, then upload the photo
    async function save() {
      Object.values(errs).forEach((e) => { e.textContent = ''; });
      dlg.setError('');
      let bad = false;
      const need = (key, ok, msg) => { if (!ok) { errs[key].textContent = msg; bad = true; } };
      need('sku', sku.value.trim(), 'SKU is required'); need('name', name.value.trim(), 'Name is required');
      need('price', price.value !== '' && Number(price.value) > 0, 'Enter a price above 0');
      need('stock', Number.isInteger(Number(stock.value)) && Number(stock.value) >= 0, 'Whole number, 0 or more');
      need('min', Number.isInteger(Number(min.value)) && Number(min.value) >= 0, 'Whole number, 0 or more');
      const file = photo.files[0];
      need('photo', !file || (file.type.startsWith('image/') && file.size <= 1048576), 'Choose an image file up to 1 MB');
      if (bad) return false;
      const body = { sku: sku.value.trim(), name: name.value.trim(), category_id: Number(cat.value), supplier_id: Number(sup.value),
        unit_price: Number(price.value), stock_quantity: Number(stock.value), min_stock_level: Number(min.value), expiry_date: exp.value || null, description: desc.value.trim() };
      const res = edit ? await IMS.api.put('/api/products/' + p.id, body) : await IMS.api.post('/api/products', body);
      if (!res.ok) { dlg.setError(IMS.api.message(res)); return false; }
      if (file) {
        const up = await IMS.api.request('POST', '/api/products/' + res.data.id + '/photo', null, { raw: file });
        if (!up.ok) IMS.ui.toast('Product saved, but the photo failed: ' + IMS.api.message(up), 'error');
        else photoVer = Date.now();
      }
      IMS.ui.toast(edit ? 'Product updated' : 'Product added');
      dlg.close(); load();
    }
  }

  const [c, s] = await Promise.all([IMS.api.get('/api/categories'), IMS.api.get('/api/suppliers')]);
  categories = c.ok ? c.data : []; suppliers = s.ok ? s.data : [];
  for (const cat of categories) catSel.appendChild(h('option', { value: cat.category_id }, cat.category_name));
  content.append(h('section', { class: 'card' },
    h('div', { class: 'toolbar' }, search, catSel, sortSel, admin ? IMS.ui.btn('Add product', { variant: 'primary', onClick: () => form(null) }) : null),
    h('div', { class: 'toolbar sub' }, seg, count, h('span', { class: 'grow' }), h('a', { href: 'credits.html', class: 'muted', style: 'font-size:13px' }, 'Photo credits'), IMS.ui.ads('Hash map / Merge sort / Binary search')), body));
  await load();
});
