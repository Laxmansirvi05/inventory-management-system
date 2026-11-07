// login.js : username/password form, then go to the dashboard.
(function () {
  const h = IMS.ui.h;

  // build the form and wire the submit button
  function show() {
    const error = h('div', { class: 'form-error', style: 'display:none', role: 'alert' });
    const user = h('input', { class: 'input', id: 'username', autocomplete: 'username', required: true });
    const pass = h('input', { class: 'input', id: 'password', type: 'password', autocomplete: 'current-password', required: true });
    const go = IMS.ui.btn('Sign in', { variant: 'primary', type: 'submit' });
    go.style.width = '100%';

    const form = h('form', { novalidate: true, on: { submit: async (e) => {
      e.preventDefault();
      error.style.display = 'none';
      if (!user.value.trim() || !pass.value) { error.textContent = 'Enter your username and password'; error.style.display = ''; return; }
      go.disabled = true;
      const res = await IMS.api.post('/api/auth/login', { username: user.value.trim(), password: pass.value }, { noRedirect: true, silent: true });
      go.disabled = false;
      if (res.ok) { location.href = 'index.html'; return; }
      error.textContent = res.error && res.error.code === 'INVALID_CREDENTIALS' ? 'Invalid username or password' : IMS.api.message(res);
      error.style.display = '';
      pass.value = '';
      pass.focus();
    } } }, error, IMS.ui.field('Username', user), IMS.ui.field('Password', pass), go);

    document.body.appendChild(h('div', { class: 'login-wrap' },
      h('div', { class: 'card login-card' },
        h('span', { class: 'brand-logo', style: 'width:38px;height:38px;display:grid;place-items:center;border-radius:10px;background:var(--accent);color:#fff' }, IMS.ui.icon('layers')),
        h('h1', {}, 'Inventory Management'), h('p', { class: 'muted', style: 'margin:0 0 20px' }, 'Sign in to continue'),
        IMS.api.isMock() ? h('div', { class: 'banner' }, 'Mock mode: any username and password works. Use the name "employee" for the employee view.') : null,
        form)));
    user.focus();
  }

  // already signed in? skip the form
  IMS.session.load().then((me) => { if (me) location.href = 'index.html'; else show(); });
})();
