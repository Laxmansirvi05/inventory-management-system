// sqlpanel.js : the Live SQL panel. Shows the SQL behind each action: statement, output,
// trigger notes, before/after rows, errors. The server logs every request; the panel only displays it.
// Oldest at the top, newest at the bottom; the panel scrolls to each new statement. The log survives page changes
// (sessionStorage) and the panel can be dragged wider or narrower between MIN_W and a maximum.
window.IMS = window.IMS || {};

IMS.sql = (function () {
  const h = IMS.ui.h;
  const KEEP = 80;                        // statements kept (oldest are dropped first)
  const MIN_W = 340, DEFAULT_W = 440;     // panel width limits in px; the maximum is maxWidth()
  const KEYWORDS = new Set(('SELECT FROM WHERE INSERT INTO VALUES UPDATE SET DELETE JOIN LEFT RIGHT INNER OUTER CROSS ON USING GROUP BY ORDER HAVING ' +
    'LIMIT OFFSET AND OR NOT NULL IS IN AS COUNT SUM AVG MIN MAX COALESCE IFNULL CASE WHEN THEN ELSE END START TRANSACTION COMMIT ROLLBACK ' +
    'SAVEPOINT DESC ASC DISTINCT LIKE ROUND BETWEEN EXISTS UNION ALL FOR SHARE LOCK CALL SHOW EXPLAIN DATE_FORMAT CURRENT_DATE NOW ' +
    'INTERVAL DAY MONTH YEAR DATE CONCAT CAST UPPER LOWER LENGTH DEFAULT').split(' '));
  // clauses that start a new line when they appear outside brackets
  const BREAKS = ['LEFT JOIN', 'RIGHT JOIN', 'INNER JOIN', 'CROSS JOIN', 'JOIN', 'FROM', 'WHERE', 'GROUP BY', 'HAVING', 'ORDER BY', 'LIMIT', 'VALUES', 'SET', 'UNION ALL', 'UNION', 'ON DUPLICATE KEY'];
  const STORE = 'liveSqlLog', WIDTH = 'liveSqlWidth';

  let actions = [];                       // oldest first: {label, ms, at, note, entries}
  let body, footCount, titleSmall, liveDot;

  // ---------- open / close and width ----------
  function isOpen() {
    try { return localStorage.getItem('liveSql') === 'on'; } catch (e) { return false; }
  }

  function setOpen(on) {
    document.body.classList.toggle('sql-open', on);
    try { localStorage.setItem('liveSql', on ? 'on' : 'off'); } catch (e) { /* storage blocked */ }
    const sw = document.getElementById('sql-switch');
    if (sw) sw.checked = on;
    if (on) scrollToEnd(false);
  }

  // the panel may take at most 60% of the window (and never more than 900px), so the page stays usable
  const maxWidth = () => Math.max(MIN_W, Math.min(900, Math.round(window.innerWidth * 0.6)));
  const clampWidth = (w) => Math.min(maxWidth(), Math.max(MIN_W, Math.round(w)));

  function setWidth(w, save) {
    const v = clampWidth(w);
    document.documentElement.style.setProperty('--sqlw', v + 'px');
    if (save) { try { localStorage.setItem(WIDTH, String(v)); } catch (e) { /* storage blocked */ } }
    return v;
  }

  function savedWidth() {
    try { return Number(localStorage.getItem(WIDTH)) || DEFAULT_W; } catch (e) { return DEFAULT_W; }
  }

  // drag handle on the left edge (also works with the arrow keys; double-click resets)
  function resizer() {
    const grip = h('div', { class: 'sql-resize', role: 'separator', 'aria-orientation': 'vertical', 'aria-label': 'Resize the Live SQL panel', tabindex: 0,
      title: 'Drag to resize. Double-click to reset.' });
    let startX = 0, startW = 0;
    const move = (e) => setWidth(startW + (startX - e.clientX), false);
    const up = (e) => {
      grip.releasePointerCapture(e.pointerId);
      grip.removeEventListener('pointermove', move);
      document.body.classList.remove('sql-resizing');
      setWidth(parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--sqlw')), true);
    };
    grip.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      startX = e.clientX;
      startW = document.getElementById('sqlpanel').getBoundingClientRect().width;
      grip.setPointerCapture(e.pointerId);
      grip.addEventListener('pointermove', move);
      grip.addEventListener('pointerup', up, { once: true });
      document.body.classList.add('sql-resizing');
    });
    grip.addEventListener('dblclick', () => setWidth(DEFAULT_W, true));
    grip.addEventListener('keydown', (e) => {
      const w = document.getElementById('sqlpanel').getBoundingClientRect().width;
      if (e.key === 'ArrowLeft') { setWidth(w + 24, true); e.preventDefault(); }
      if (e.key === 'ArrowRight') { setWidth(w - 24, true); e.preventDefault(); }
    });
    return grip;
  }

  // ---------- the log ----------
  function save() {
    try { sessionStorage.setItem(STORE, JSON.stringify(actions)); } catch (e) { /* full or blocked: the log then lasts for this page only */ }
  }

  function load() {
    try { actions = JSON.parse(sessionStorage.getItem(STORE) || '[]'); } catch (e) { actions = []; }
    if (!Array.isArray(actions)) actions = [];
  }

  const statementCount = () => actions.reduce((n, a) => n + Math.max(1, a.entries.length), 0);

  // drop the oldest actions until at most KEEP statements remain (the newest action always stays)
  function trim() {
    let dropped = false;
    while (actions.length > 1 && statementCount() > KEEP) { actions.shift(); dropped = true; }
    return dropped;
  }

  function push(action) {
    actions.push(action);
    const dropped = trim();
    save();
    if (!body) return;
    if (dropped) render();                          // something left the top: redraw
    else {
      const empty = body.querySelector('.sql-empty');
      if (empty) empty.remove();
      body.appendChild(actionBlock(action, true));
      updateCounts();
    }
    scrollToEnd(true);
  }

  // called by api.js after each request that ran SQL
  function record(label, entries, ms) {
    push({ label, ms, at: Date.now(), note: null, entries });
  }

  // an action that touched no SQL (answered from memory)
  function recordNote(label, note, ms) {
    push({ label, ms, at: Date.now(), note, entries: [] });
  }

  function scrollToEnd(smooth) {
    if (!body || !document.body.classList.contains('sql-open')) return;
    requestAnimationFrame(() => body.scrollTo({ top: body.scrollHeight, behavior: smooth ? 'smooth' : 'auto' }));
  }

  // ---------- drawing ----------
  // put the main clauses on their own lines (only outside quotes and brackets)
  function formatSql(sql) {
    let out = '', depth = 0, quote = null;
    const upper = sql.toUpperCase();
    for (let i = 0; i < sql.length; i++) {
      const c = sql[i];
      if (quote) { out += c; if (c === '\\') { out += sql[++i] || ''; } else if (c === quote) quote = null; continue; }
      if (c === "'" || c === '"' || c === '`') { quote = c; out += c; continue; }
      if (c === '(') depth++;
      if (c === ')') depth = Math.max(0, depth - 1);
      if (depth === 0 && /\s/.test(c)) {
        const rest = upper.slice(i + 1);
        const kw = BREAKS.find((k) => rest.startsWith(k) && !/[A-Z0-9_]/.test(rest.charAt(k.length)));
        if (kw && out.trim() && !(kw === 'FROM' && /DELETE\s*$/i.test(out))) { out = out.replace(/\s+$/, '') + '\n'; continue; }
      }
      out += c;
    }
    return out.replace(/[ \t]+\n/g, '\n').trim();
  }

  // color keywords, strings and numbers with DOM spans (text stays text)
  function highlight(sql) {
    const box = h('pre', { class: 'sql-code' });
    const text = formatSql(sql);
    const re = /('(?:[^'\\]|\\.)*')|(\b\d+(?:\.\d+)?\b)|([A-Za-z_][A-Za-z0-9_]*)/g;
    let last = 0, m;
    while ((m = re.exec(text))) {
      if (m.index > last) box.appendChild(document.createTextNode(text.slice(last, m.index)));
      if (m[1]) box.appendChild(h('span', { class: 'str' }, m[1]));
      else if (m[2]) box.appendChild(h('span', { class: 'nm' }, m[2]));
      else box.appendChild(KEYWORDS.has(m[3].toUpperCase()) ? h('span', { class: 'kw' }, m[3]) : document.createTextNode(m[3]));
      last = re.lastIndex;
    }
    if (last < text.length) box.appendChild(document.createTextNode(text.slice(last)));
    return box;
  }

  // "Before" / "After" box for a changed row
  function snapshot(title, s) {
    return h('div', {}, h('b', {}, title + ' · ' + s.table + ' #' + s.key),
      Object.keys(s.row).map((k) => h('div', { class: 'kv' }, h('span', {}, k), h('code', {}, String(s.row[k])))));
  }

  // result table (the server sends up to 5 rows) or "N row(s) affected"
  function output(e) {
    if (e.result) {
      const r = e.result;
      const cell = (c) => (c === null ? h('td', { class: 'null' }, 'NULL') : h('td', { title: String(c) }, String(c)));
      const shown = r.rows.length;
      return h('div', { class: 'sql-out' },
        shown ? h('div', { class: 'sql-tablewrap' }, h('table', {},
          h('thead', {}, h('tr', {}, r.columns.map((c) => h('th', {}, c)))),
          h('tbody', {}, r.rows.map((row) => h('tr', {}, row.map(cell)))))) : null,
        h('div', { class: 'sql-meta' }, r.total === 0 ? 'Empty set (0 rows)' : r.total + (r.total === 1 ? ' row in set' : ' rows in set') + (r.total > shown ? ', first ' + shown + ' shown' : '')));
    }
    if (e.rows_affected !== null && e.rows_affected !== undefined) {
      return h('div', { class: 'sql-out' }, h('div', { class: 'sql-meta ok' }, 'Query OK, ' + e.rows_affected + (e.rows_affected === 1 ? ' row' : ' rows') + ' affected'));
    }
    return e.error ? null : h('div', { class: 'sql-out' }, h('div', { class: 'sql-meta ok' }, 'Query OK'));
  }

  // one statement card
  function statement(e) {
    return h('div', { class: 'sql-stmt' + (e.error ? ' err' : '') + (e.helper ? ' helper' : '') },
      h('div', { class: 'sql-top' },
        h('span', { class: 'seq' }, '#' + e.seq),
        h('span', { class: 'kind k-' + String(e.kind).toLowerCase() }, e.kind),
        h('span', { class: 'grow' }, e.helper ? 'helper read for the Before/After boxes' : ''),
        h('span', {}, (Math.round(e.ms * 100) / 100) + ' ms')),
      highlight(e.sql), output(e),
      (e.triggers || []).map((t) => h('div', { class: 'sql-trg ' + t.level }, h('b', {}, t.name), ' ', t.note)),
      e.before || e.after ? h('div', { class: 'sql-ba' }, e.before ? snapshot('Before', e.before) : h('div', { class: 'none' }, 'No row before'), e.after ? snapshot('After', e.after) : h('div', { class: 'none' }, 'No row after')) : null,
      e.error ? h('div', { class: 'sql-error' }, 'ERROR ' + e.error.mysql_errno + ': ' + e.error.message) : null);
  }

  // one action: a header line and its statements
  function actionBlock(a, fresh) {
    const time = new Date(a.at || Date.now()).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    const n = a.entries.length;
    return h('section', { class: 'sql-action' + (fresh ? ' fresh' : '') },
      h('header', {}, h('span', { class: 'when' }, time), h('span', { class: 'what' }, a.label),
        h('span', { class: 'cost' }, (n ? n + (n === 1 ? ' statement · ' : ' statements · ') : '') + a.ms + ' ms')),
      a.note ? h('div', { class: 'sql-stmt note' }, a.note) : null,
      a.entries.map(statement));
  }

  function updateCounts() {
    const last = actions[actions.length - 1];
    footCount.textContent = statementCount() + ' statements';
    titleSmall.textContent = last ? 'last action ' + last.ms + ' ms' : 'waiting for the first action';
    liveDot.classList.toggle('on', !!last);
  }

  // redraw the whole body (first draw, after Clear, or when old actions were dropped)
  function render() {
    if (!body) return;
    IMS.ui.clear(body);
    if (!actions.length) body.appendChild(h('div', { class: 'sql-empty' }, h('b', {}, 'Nothing yet'), 'Use the page: every query it runs appears here, newest at the bottom.'));
    for (const a of actions) body.appendChild(actionBlock(a, false));
    updateCounts();
  }

  // build the panel element (layout.js adds it to the page)
  function build() {
    load();
    setWidth(savedWidth(), false);
    window.addEventListener('resize', () => setWidth(savedWidth(), false));
    body = h('div', { class: 'sql-body' });
    footCount = h('span', { class: 'grow' }, '0 statements');
    titleSmall = h('small', {}, '');
    liveDot = h('i', { class: 'live' });
    const panel = h('aside', { id: 'sqlpanel', class: 'sqlpanel', 'aria-label': 'Live SQL' }, resizer(),
      h('div', { class: 'sql-h' }, liveDot, h('b', {}, 'Live SQL'), titleSmall), body,
      h('div', { class: 'sql-f' }, footCount,
        h('button', { type: 'button', on: { click: () => { actions = []; save(); render(); } } }, 'Clear'),
        h('button', { type: 'button', on: { click: () => setOpen(false) } }, 'Collapse')));
    render();
    return panel;
  }

  return { isOpen, setOpen, record, recordNote, build, formatSql };
})();
