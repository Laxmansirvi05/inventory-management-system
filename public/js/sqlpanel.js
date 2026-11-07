// sqlpanel.js : the Live SQL panel. Shows the SQL behind the last actions: statement, output,
// trigger notes, before/after rows, errors. The server logs every request; the panel only displays it.
window.IMS = window.IMS || {};

IMS.sql = (function () {
  const h = IMS.ui.h;
  const KEEP = 50;                        // statements kept in memory for this page
  const KEYWORDS = new Set(('SELECT FROM WHERE INSERT INTO VALUES UPDATE SET DELETE JOIN LEFT RIGHT INNER OUTER ON GROUP BY ORDER HAVING ' +
    'LIMIT AND OR NOT NULL IS IN AS COUNT SUM COALESCE START TRANSACTION COMMIT ROLLBACK DESC ASC DISTINCT LIKE ROUND').split(' '));
  let actions = [];                       // newest first: {label, ms, note, entries}
  let body, footCount, titleSmall;

  // is the panel switched on? (saved in localStorage)
  function isOpen() {
    try { return localStorage.getItem('liveSql') === 'on'; } catch (e) { return false; }
  }

  // show or hide the panel and remember the choice
  function setOpen(on) {
    document.body.classList.toggle('sql-open', on);
    try { localStorage.setItem('liveSql', on ? 'on' : 'off'); } catch (e) { /* storage blocked */ }
    const sw = document.getElementById('sql-switch');
    if (sw) sw.checked = on;
  }

  // keep only the newest KEEP statements
  function trim() {
    let n = 0;
    actions = actions.filter((a) => { n += Math.max(1, a.entries.length); return n <= KEEP || a === actions[0]; });
  }

  // called by api.js after each request that ran SQL
  function record(label, entries, ms) {
    actions.unshift({ label, ms, note: null, entries });
    trim();
    render();
  }

  // an action that touched no SQL (answered from memory)
  function recordNote(label, note, ms) {
    actions.unshift({ label, ms, note, entries: [] });
    trim();
    render();
  }

  // color SQL keywords, strings and numbers with DOM spans (text stays text)
  function highlight(sql) {
    const box = h('div', { class: 'sql-code' });
    const re = /('(?:[^'\\]|\\.)*')|(\b\d+(?:\.\d+)?\b)|([A-Za-z_]+)/g;
    let last = 0, m;
    while ((m = re.exec(sql))) {
      if (m.index > last) box.appendChild(document.createTextNode(sql.slice(last, m.index)));
      if (m[1]) box.appendChild(h('span', { class: 'str' }, m[1]));
      else if (m[2]) box.appendChild(h('span', { class: 'nm' }, m[2]));
      else box.appendChild(KEYWORDS.has(m[3].toUpperCase()) ? h('span', { class: 'kw' }, m[3]) : document.createTextNode(m[3]));
      last = re.lastIndex;
    }
    if (last < sql.length) box.appendChild(document.createTextNode(sql.slice(last)));
    return box;
  }

  // "Before" / "After" box for a changed row
  function snapshot(title, s) {
    const lines = Object.keys(s.row).map((k) => k + ' = ' + s.row[k]);
    return h('div', {}, h('b', {}, title + ' · ' + s.table + ' #' + s.key), lines.join('\n'));
  }

  // result table (up to 5 rows) or "N row(s) affected"
  function output(e) {
    if (e.result) {
      const r = e.result;
      const rows = r.rows.map((row) => h('tr', {}, row.map((c) => h('td', {}, c === null ? 'NULL' : String(c)))));
      return h('div', { class: 'sql-out' },
        h('table', {}, h('thead', {}, h('tr', {}, r.columns.map((c) => h('th', {}, c)))), h('tbody', {}, rows)),
        r.total + (r.total === 1 ? ' row' : ' rows') + (r.total > r.rows.length ? ' (first ' + r.rows.length + ' shown)' : ''));
    }
    if (e.rows_affected !== null && e.rows_affected !== undefined) return h('div', { class: 'sql-out' }, e.rows_affected + ' row(s) affected');
    return e.error ? null : h('div', { class: 'sql-out' }, 'OK');
  }

  // one statement card
  function statement(e) {
    return h('div', { class: 'sql-stmt' + (e.error ? ' err' : '') },
      h('div', { class: 'sql-top' }, h('span', {}, '#' + e.seq), h('span', { class: 'grow' }, e.kind), h('span', {}, (Math.round(e.ms * 100) / 100) + ' ms')),
      highlight(e.sql), output(e),
      (e.triggers || []).map((t) => h('div', { class: 'sql-trg ' + t.level }, h('b', {}, t.name + ': '), t.note)),
      e.before || e.after ? h('div', { class: 'sql-ba' }, e.before ? snapshot('Before', e.before) : h('div', {}), e.after ? snapshot('After', e.after) : h('div', {})) : null,
      e.error ? h('div', { class: 'sql-error' }, 'MySQL error ' + e.error.mysql_errno + ': ' + e.error.message) : null);
  }

  // redraw the panel body
  function render() {
    if (!body) return;
    IMS.ui.clear(body);
    if (!actions.length) body.appendChild(h('div', { class: 'sql-empty' }, 'Nothing yet. Do something on the page and its SQL shows up here.'));
    let count = 0;
    for (const a of actions) {
      body.appendChild(h('div', { class: 'sql-action' }, h('span', {}, a.label), h('span', {}, a.ms + ' ms')));
      if (a.note) body.appendChild(h('div', { class: 'sql-stmt' }, h('div', { class: 'sql-out' }, a.note)));
      for (const e of a.entries) { body.appendChild(statement(e)); count++; }
    }
    footCount.textContent = 'Statements: ' + count;
    titleSmall.textContent = actions.length ? ' · last action · ' + actions[0].ms + ' ms' : ' · no action yet';
  }

  // build the panel element (layout.js adds it to the page)
  function build() {
    body = h('div', { class: 'sql-body' });
    footCount = h('span', { class: 'grow' }, 'Statements: 0');
    titleSmall = h('small', {}, '');
    const panel = h('aside', { id: 'sqlpanel', class: 'sqlpanel', 'aria-label': 'Live SQL' },
      h('div', { class: 'sql-h' }, 'Live SQL', titleSmall), body,
      h('div', { class: 'sql-f' }, footCount,
        h('button', { type: 'button', on: { click: () => { actions = []; render(); } } }, 'Clear'),
        h('button', { type: 'button', on: { click: () => setOpen(false) } }, 'Collapse')));
    render();
    return panel;
  }

  return { isOpen, setOpen, record, recordNote, build };
})();
