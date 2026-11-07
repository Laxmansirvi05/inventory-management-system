// users.js : administrators manage login accounts. Passwords are hashed on the server and never shown.
IMS.boot(async (content, me) => {
  const h = IMS.ui.h, f = IMS.fmt;
  const table = new IMS.ui.DataTable({ pageSize: 10, empty: 'No users', columns: [
    { label: 'Username', key: 'username', sortable: true },
    { label: 'Role', key: 'role', sortable: true, render: (u) => IMS.ui.tag(u.role === 'ADMIN' ? 'Admin' : 'Employee', u.role === 'ADMIN' ? 'info' : '') },
    { label: 'Created', render: (u) => f.date(u.created_at) },
    { label: '', render: (u) => h('div', { class: 'row-actions' },
      IMS.ui.btn('Reset password', { small: true, onClick: () => resetPassword(u) }),
      u.user_id === me.user_id ? null : IMS.ui.btn(u.role === 'ADMIN' ? 'Make employee' : 'Make admin', { small: true, onClick: () => setRole(u, u.role === 'ADMIN' ? 'EMPLOYEE' : 'ADMIN') }),
      u.user_id === me.user_id ? null : IMS.ui.btn('Delete', { small: true, variant: 'danger', onClick: () => remove(u) })) },
  ] });

  // reload the user list
  async function load() {
    const res = await IMS.api.get('/api/users');
    if (res.ok) table.setRows(res.data); else IMS.ui.toast(IMS.api.message(res), 'error');
  }

  // send a change and show the result
  async function apply(promise, okText) {
    const res = await promise;
    if (res.ok) { IMS.ui.toast(okText); load(); } else IMS.ui.toast(IMS.api.message(res), 'error');
  }

  const setRole = (u, role) => IMS.ui.confirmDialog('Change ' + u.username + ' to ' + (role === 'ADMIN' ? 'Admin' : 'Employee') + '?', 'Change').then((yes) => yes && apply(IMS.api.put('/api/users/' + u.user_id, { role }), 'Role changed'));
  const remove = (u) => IMS.ui.confirmDialog('Delete user "' + u.username + '"?', 'Delete', true).then((yes) => yes && apply(IMS.api.del('/api/users/' + u.user_id), 'User deleted'));

  // password field with the 8-character rule
  const passwordInput = () => h('input', { class: 'input', type: 'password', autocomplete: 'new-password', minlength: 8 });

  // ask for a new password
  function resetPassword(u) {
    const pw = passwordInput();
    const dlg = IMS.ui.modal({ title: 'Reset password for ' + u.username, content: IMS.ui.field('New password', pw, 'At least 8 characters'), buttons: [
      { label: 'Cancel', close: true, onClick: () => {} },
      { label: 'Reset password', variant: 'primary', onClick: async () => {
        if (pw.value.length < 8) { dlg.setError('Password must be at least 8 characters'); return false; }
        const res = await IMS.api.put('/api/users/' + u.user_id, { password: pw.value });
        if (!res.ok) { dlg.setError(IMS.api.message(res)); return false; }
        IMS.ui.toast('Password changed'); dlg.close();
      } }] });
  }

  // new user dialog
  function add() {
    const name = h('input', { class: 'input', autocomplete: 'off', maxlength: 40 }), pw = passwordInput();
    const role = IMS.ui.select([{ value: 'EMPLOYEE', label: 'Employee' }, { value: 'ADMIN', label: 'Admin' }], 'EMPLOYEE');
    const dlg = IMS.ui.modal({ title: 'Add user', content: h('div', {}, IMS.ui.field('Username', name, '3-40 letters, digits, dot, dash or underscore'),
      IMS.ui.field('Password', pw, 'At least 8 characters'), IMS.ui.field('Role', role)), buttons: [
      { label: 'Cancel', close: true, onClick: () => {} },
      { label: 'Add user', variant: 'primary', onClick: async () => {
        if (pw.value.length < 8) { dlg.setError('Password must be at least 8 characters'); return false; }
        const res = await IMS.api.post('/api/users', { username: name.value.trim(), password: pw.value, role: role.value });
        if (!res.ok) { dlg.setError(IMS.api.message(res)); return false; }
        IMS.ui.toast('User added'); dlg.close(); load();
      } }] });
  }

  content.append(h('section', { class: 'card' }, h('div', { class: 'toolbar' }, h('span', { class: 'grow muted' }, 'Accounts that can sign in'),
    IMS.ui.btn('Add user', { variant: 'primary', onClick: add })), table.el));
  await load();
});
