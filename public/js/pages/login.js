// login.js : the sign-in page. Checks the form, signs in, then swings the stockroom camera and opens the dashboard.
// The 3D stockroom on the left leans toward the pointer and drifts slowly on its own.
(function () {
  'use strict';
  const reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  const $ = (id) => document.getElementById(id);
  const form = $('loginForm'), user = $('username'), pass = $('password'), go = $('goBtn'), error = $('formError');

  // already signed in? skip the form
  IMS.session.load().then((me) => { if (me) location.href = 'dashboard.html'; });
  if (IMS.api.isMock()) $('mockBanner').hidden = false;
  user.focus();

  // show or hide the password
  $('pwToggle').addEventListener('click', (e) => {
    const show = pass.type === 'password';
    pass.type = show ? 'text' : 'password';
    e.currentTarget.textContent = show ? 'Hide' : 'Show';
    e.currentTarget.setAttribute('aria-pressed', String(show));
    pass.focus();
  });

  function fail(message, field) {
    error.textContent = message;
    error.hidden = false;
    [user, pass].forEach((f) => f.removeAttribute('aria-invalid'));
    if (field) { field.setAttribute('aria-invalid', 'true'); field.focus(); }
    form.classList.remove('shake'); void form.offsetWidth; form.classList.add('shake');
  }
  [user, pass].forEach((f) => f.addEventListener('input', () => { f.removeAttribute('aria-invalid'); error.hidden = true; }));

  form.addEventListener('submit', async (e) => {
    e.preventDefault();
    if (go.disabled) return;
    if (!user.value.trim()) return fail('Enter your username.', user);
    if (!pass.value) return fail('Enter your password.', pass);
    error.hidden = true;
    go.disabled = true; go.classList.add('busy'); go.querySelector('.go-label').textContent = 'Signing in';
    const res = await IMS.api.post('/api/auth/login', { username: user.value.trim(), password: pass.value }, { noRedirect: true, silent: true });
    if (res.ok) {
      go.querySelector('.go-label').textContent = 'Opening your workspace';
      document.body.classList.add('opening');
      setTimeout(() => { location.href = 'dashboard.html'; }, reduce ? 0 : 850);
      return;
    }
    go.disabled = false; go.classList.remove('busy'); go.querySelector('.go-label').textContent = 'Sign in';
    pass.value = '';
    const bad = res.error && res.error.code === 'INVALID_CREDENTIALS';
    fail(bad ? 'That username and password do not match. Check them and try again.' : IMS.api.message(res), pass);
  });

  // the stockroom leans toward the pointer; with no pointer it drifts slowly around the stack
  const stage = $('stage');
  if (stage && !reduce) {
    let tx = 0, ty = 0, cx = 0, cy = 0, pointing = false;
    window.addEventListener('pointermove', (e) => {
      if (e.pointerType !== 'mouse') return;
      pointing = true;
      tx = (e.clientX / innerWidth - 0.5) * 2;
      ty = (e.clientY / innerHeight - 0.5) * 2;
    });
    document.addEventListener('pointerleave', () => { pointing = false; });
    (function frame(t) {
      if (!pointing) { tx = Math.sin(t / 4200) * 0.5; ty = Math.cos(t / 5600) * 0.25; }
      cx += (tx - cx) * 0.05;
      cy += (ty - cy) * 0.05;
      stage.style.setProperty('--py', (cx * 10).toFixed(3) + 'deg');
      stage.style.setProperty('--px', (-cy * 6).toFixed(3) + 'deg');
      requestAnimationFrame(frame);
    })(0);
  }
})();
