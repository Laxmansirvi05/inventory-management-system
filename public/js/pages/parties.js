// parties.js : Customers and Suppliers in two tabs. Employees add/edit customers and only view suppliers.
IMS.boot(async (content, user) => {
  const h = IMS.ui.h, admin = user.role === 'ADMIN';
  const phone = { key: 'phone', label: 'Phone', required: true, max: 10, hint: '10 digits' };
  const TABS = {
    customers: { label: 'Customers', cfg: {
      endpoint: '/api/customers', idKey: 'customer_id', nameKey: 'customer_name', noun: 'customer', addLabel: 'Add customer',
      canWrite: true, canDelete: admin, protectedId: 1,
      columns: [{ label: 'Name', key: 'customer_name' }, { label: 'Phone', key: 'phone' }, { label: 'Email', render: (c) => c.email || '—' },
        { label: 'City', render: (c) => c.city || '—' }, { label: 'Registered', render: (c) => IMS.fmt.date(c.registered_on) }],
      fields: [{ key: 'customer_name', label: 'Name', required: true }, phone, { key: 'email', label: 'Email' }, { key: 'city', label: 'City', max: 40 }] } },
    suppliers: { label: 'Suppliers', cfg: {
      endpoint: '/api/suppliers', idKey: 'supplier_id', nameKey: 'supplier_name', noun: 'supplier', addLabel: 'Add supplier',
      canWrite: admin, canDelete: admin,
      columns: [{ label: 'Supplier', key: 'supplier_name' }, { label: 'Contact', render: (s) => s.contact_person || '—' }, { label: 'Phone', key: 'phone' },
        { label: 'Email', render: (s) => s.email || '—' }, { label: 'City', render: (s) => s.city || '—' }],
      fields: [{ key: 'supplier_name', label: 'Name', required: true }, { key: 'contact_person', label: 'Contact person', max: 60 }, phone,
        { key: 'email', label: 'Email' }, { key: 'city', label: 'City', max: 40 }] } },
  };
  const tabs = h('div', { class: 'tabs', role: 'tablist', style: 'background:var(--card);border:1px solid var(--border);border-radius:12px 12px 0 0;border-bottom:0' });
  const body = h('div', {});
  content.append(tabs, body);

  // switch tab: draw its list
  async function open(key) {
    IMS.ui.clear(tabs).append(...Object.keys(TABS).map((k) =>
      h('button', { class: 'tab' + (k === key ? ' active' : ''), role: 'tab', 'aria-selected': k === key, type: 'button', on: { click: () => open(k) } }, TABS[k].label)));
    IMS.ui.clear(body);
    await IMS.crud.mount(body, TABS[key].cfg);
  }
  await open(new URLSearchParams(location.search).get('tab') === 'suppliers' ? 'suppliers' : 'customers');
});
