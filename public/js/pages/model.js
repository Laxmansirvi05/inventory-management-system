// model.js : Database Model page: ER diagram, relational schema and one card per table (read from information_schema).
IMS.boot(async (content) => {
  const h = IMS.ui.h;

  // diagram that opens full size in a dialog
  function diagram(src, title) {
    return h('img', { class: 'diagram', src, alt: title, tabindex: 0, title: 'Click to enlarge', on: { click: () => IMS.ui.modal({ title, wide: true, content: h('img', { src, alt: title, style: 'max-width:100%' }) }) } });
  }

  // one table card: column, type, key tags
  function tableCard(t) {
    const rows = t.columns.map((c) => h('tr', {},
      h('td', {}, c.key === 'PRI' ? h('b', {}, c.name) : c.name),
      h('td', { class: 'muted' }, c.type),
      h('td', {}, c.key === 'PRI' ? IMS.ui.tag('PK', 'warn') : null, ' ', c.references ? IMS.ui.tag('FK → ' + c.references.table, 'info') : null, ' ', c.key === 'UNI' ? IMS.ui.tag('UQ') : null)));
    return h('div', { class: 'card tcard' }, h('h3', {}, t.name), h('table', {}, h('tbody', {}, rows)));
  }

  content.append(
    h('section', { class: 'card', style: 'margin-bottom:22px' },
      h('div', { class: 'card-h' }, h('h2', {}, 'ER diagram'), h('span', { class: 'muted' }, 'Chen notation · click to enlarge')),
      h('div', { class: 'card-b' }, diagram('img/er_diagram.svg', 'ER diagram'))),
    h('section', { class: 'card', style: 'margin-bottom:22px' },
      h('div', { class: 'card-h' }, h('h2', {}, 'Relational schema'), h('span', { class: 'muted' }, 'tables, keys and foreign-key links · click to enlarge')),
      h('div', { class: 'card-b' }, diagram('img/schema.svg', 'Relational schema'))));
  const res = await IMS.api.get('/api/model');
  if (!res.ok) { IMS.ui.toast(IMS.api.message(res), 'error'); return; }
  content.append(h('h2', { style: 'margin:8px 0 14px;font:800 26px var(--head)' }, res.data.tables.length + ' tables in the database'), h('div', { class: 'tables-grid' }, res.data.tables.map(tableCard)));
});
