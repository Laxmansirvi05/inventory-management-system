// layout.js : builds the shell (sidebar, top bar, Live SQL panel) on every page except login,
// checks the session and role, then hands the content area to the page script: IMS.boot(async (content, user) => {...}).
window.IMS = window.IMS || {};

IMS.layout = (function () {
  const h = IMS.ui.h;
  const NAV = [
    { page: 'dashboard', href: 'dashboard.html', label: 'Dashboard', icon: 'grid' },
    { page: 'products', href: 'products.html', label: 'Products', icon: 'box' },
    { page: 'categories', href: 'categories.html', label: 'Categories', icon: 'tag', admin: true },
    { page: 'parties', href: 'parties.html', label: 'Customers & Suppliers', icon: 'users' },
    { page: 'purchases', href: 'purchases.html', label: 'Purchases', icon: 'truck' },
    { page: 'sales', href: 'sales.html', label: 'Sales', icon: 'cart' },
    { page: 'reports', href: 'reports.html', label: 'Reports', icon: 'chart' },
    { page: 'users', href: 'users.html', label: 'Users', icon: 'key', admin: true },
    { page: 'model', href: 'model.html', label: 'Database Model', icon: 'db', admin: true },
  ];
  let connEl = null;

  // update the "Database connected" badge (called by api.js after every request)
  function setConnection(reachable) {
    if (!connEl) return;
    const mock = IMS.api.isMock();
    connEl.className = 'conn' + (mock ? ' mock' : reachable ? '' : ' bad');
    connEl.textContent = mock ? 'Mock data' : reachable ? 'Database connected' : 'Server unreachable';
  }

  // left sidebar: logo, links the role may see, user box
  function sidebar(user, page) {
    const links = NAV.filter((n) => !n.admin || user.role === 'ADMIN').map((n) =>
      h('a', { href: n.href, class: n.page === page ? 'active' : '', 'aria-current': n.page === page ? 'page' : null }, IMS.ui.icon(n.icon), n.label));
    return h('aside', { class: 'sidebar no-print' },
      h('a', { class: 'brand', href: 'dashboard.html', style: 'text-decoration:none' }, h('img', { class: 'brand-logo', src: 'img/favicon.svg', alt: '' }), 'Inventory'),
      h('nav', { class: 'nav', 'aria-label': 'Main' }, links),
      h('div', { class: 'userbox' }, h('span', { class: 'avatar' }, IMS.fmt.initials(user.username)),
        h('div', { class: 'who' }, h('b', {}, user.username), h('span', { class: 'muted' }, user.role === 'ADMIN' ? 'Admin' : 'Employee')),
        IMS.ui.btn(IMS.ui.icon('logout'), { small: true, title: 'Log out', onClick: () => IMS.session.logout() })));
  }

  // top bar: page title, connection badge, Live SQL switch
  function topbar(title) {
    connEl = h('span', { class: 'conn' }, 'Database connected');
    const input = h('input', { type: 'checkbox', id: 'sql-switch', role: 'switch', on: { change: (e) => IMS.sql.setOpen(e.target.checked) } });
    return h('header', { class: 'topbar' }, h('h1', {}, title), connEl,
      h('label', { class: 'switch' }, input, h('span', { class: 'track' }), 'Live SQL'));
  }

  // check login and role, draw the shell, run the page
  async function boot(init) {
    const user = await IMS.session.load();
    if (!user) { location.href = 'login.html'; return; }
    const d = document.body.dataset;
    if (d.role === 'admin' && user.role !== 'ADMIN') { location.href = 'dashboard.html'; return; }
    const content = h('main', { id: 'content' });
    document.body.appendChild(sidebar(user, d.page));
    document.body.appendChild(h('div', { class: 'main' }, topbar(d.title || document.title), content));
    document.body.appendChild(IMS.sql.build());
    IMS.sql.setOpen(IMS.sql.isOpen());
    setConnection(true);
    if (IMS.api.isMock()) {
      content.appendChild(h('div', { class: 'banner no-print' }, 'Mock mode: data comes from sample files and nothing is saved. ',
        h('a', { href: '?mock=0' }, 'Leave mock mode')));
    }
    try {
      await init(content, user);
    } catch (e) {
      console.error(e);
      content.appendChild(h('div', { class: 'card card-b' }, 'This page could not be loaded: ' + e.message));
    }
  }

  return { boot, setConnection };
})();

IMS.boot = IMS.layout.boot;
