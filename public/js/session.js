// session.js : who is logged in, role guard, logout.
window.IMS = window.IMS || {};

IMS.session = (function () {
  let current = null;

  // ask the server who we are; null when not logged in
  async function load() {
    const res = await IMS.api.get('/api/auth/me', { noRedirect: true, silent: true });
    current = res.ok ? res.data : null;
    return current;
  }

  // end the session and go to the login page
  async function logout() {
    await IMS.api.post('/api/auth/logout', {}, { noRedirect: true, silent: true });
    location.href = 'login.html';
  }

  // the cached user (after load)
  function user() { return current; }

  // true when the user is an administrator
  function isAdmin() { return !!current && current.role === 'ADMIN'; }

  return { load, logout, user, isAdmin };
})();
