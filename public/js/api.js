// api.js : every call to the server goes through here.
// Returns {ok, status, data, error, sql_log} and feeds sql_log to the Live SQL panel.
// With ?mock=1 the answers come from public/mock/*.json instead (see mock.js); the flag is kept for the browser tab.
window.IMS = window.IMS || {};

IMS.api = (function () {
  const MEMORY = [/^GET \/api\/products(\?|$)/, /^GET \/api\/alerts\//, /^GET \/api\/sales\/queue$/, /^GET \/api\/products\/(sku-check|\d+\?)/];   // answered from C++ data structures, no SQL

  (function readMockFlag() {   // ?mock=1 switches on, ?mock=0 off; stored per tab
    const q = new URLSearchParams(location.search).get('mock');
    try {
      if (q === '1') sessionStorage.setItem('ims_mock', '1');
      else if (q === '0') sessionStorage.removeItem('ims_mock');
    } catch (e) { /* storage blocked: mock flag then only lasts for this page */ }
  })();

  // true while mock mode is on
  function isMock() {
    try { return sessionStorage.getItem('ims_mock') === '1'; } catch (e) { return false; }
  }

  // send a request to the real server and normalise the answer
  async function realFetch(method, path, body, raw) {
    const init = { method, credentials: 'same-origin', headers: {} };
    if (raw) { init.body = raw; init.headers['Content-Type'] = raw.type || 'application/octet-stream'; }
    else if (body !== undefined && body !== null) { init.body = JSON.stringify(body); init.headers['Content-Type'] = 'application/json'; }
    try {
      const res = await fetch(path, init);
      let doc = null;
      try { doc = await res.json(); } catch (e) { /* not JSON */ }
      if (!doc) return { ok: false, status: res.status, data: null, error: { code: 'BAD_RESPONSE', message: 'Unexpected answer from the server (' + res.status + ')' }, sql_log: [] };
      return { ok: !!doc.ok, status: res.status, data: doc.data, error: doc.error || null, sql_log: doc.sql_log || [] };
    } catch (e) {
      return { ok: false, status: 0, data: null, error: { code: 'NETWORK', message: 'Cannot reach the server. Is it running?' }, sql_log: [] };
    }
  }

  // one request; opts: {raw: File, silent: true (not shown in the SQL panel), noRedirect: true}
  async function request(method, path, body, opts) {
    const o = opts || {};
    const t0 = performance.now();
    const res = isMock() ? await IMS.mock.handle(method, path, body) : await realFetch(method, path, body, o.raw);
    const ms = Math.round(performance.now() - t0);
    if (IMS.layout) IMS.layout.setConnection(res.status !== 0);
    if (!o.silent && IMS.sql) {
      const label = method + ' ' + path.split('?')[0];
      if (res.sql_log.length) IMS.sql.record(label, res.sql_log, ms);
      else if (res.ok && MEMORY.some((re) => re.test(method + ' ' + path))) IMS.sql.recordNote(label, 'No SQL: answered from the in-memory data structures (hash map, merge sort, priority queue).', ms);
    }
    if (res.status === 401 && !o.noRedirect && !location.pathname.endsWith('login.html')) location.href = 'login.html';
    return res;
  }

  // text to show the user for a failed call
  function message(res) {
    if (!res.error) return 'Something went wrong';
    return res.error.message + (res.error.mysql_errno ? ' (MySQL error ' + res.error.mysql_errno + ')' : '');
  }

  return {
    isMock, request, message,
    get: (path, o) => request('GET', path, null, o),
    post: (path, body, o) => request('POST', path, body === undefined ? {} : body, o),
    put: (path, body, o) => request('PUT', path, body, o),
    del: (path, o) => request('DELETE', path, null, o),
  };
})();
