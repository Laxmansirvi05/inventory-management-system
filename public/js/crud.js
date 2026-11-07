// crud.js : a list + add/edit modal + delete for one simple table (categories, customers, suppliers).
// IMS.crud.mount(container, cfg) -> {reload}
// cfg: {endpoint, idKey, nameKey, columns, fields:[{key,label,required,type,hint,wide}], canWrite, canDelete,
//       addLabel, noun, usageKey (column holding "how many records use this row"), protectedId}
window.IMS = window.IMS || {};

IMS.crud = (function () {
  const h = IMS.ui.h;

  // build the list card inside `container`
  function mount(container, cfg) {
    let rows = [];
    const table = new IMS.ui.DataTable({ pageSize: 10, empty: 'No ' + cfg.noun + 's yet', columns: cfg.columns.concat(
      cfg.canWrite || cfg.canDelete ? [{ label: '', render: (r) => h('div', { class: 'row-actions' },
        cfg.canWrite ? IMS.ui.btn('Edit', { small: true, onClick: () => form(r) }) : null,
        cfg.canDelete && r[cfg.idKey] !== cfg.protectedId ? IMS.ui.btn('Delete', { small: true, variant: 'danger', onClick: () => remove(r) }) : null) }] : []) });
    const find = h('input', { class: 'input grow', type: 'search', placeholder: 'Search ' + cfg.noun + 's', 'aria-label': 'Search',
      on: { input: () => show() } });

    // apply the search box to the loaded rows
    function show() {
      const t = find.value.trim().toLowerCase();
      table.setRows(t ? rows.filter((r) => Object.values(r).some((v) => v !== null && String(v).toLowerCase().includes(t))) : rows);
    }

    // fetch the list again
    async function reload() {
      const res = await IMS.api.get(cfg.endpoint);
      if (!res.ok) { IMS.ui.toast(IMS.api.message(res), 'error'); return; }
      rows = res.data;
      show();
    }

    // delete one row; the MySQL foreign-key error explains why it is blocked
    async function remove(r) {
      if (!(await IMS.ui.confirmDialog('Delete ' + cfg.noun + ' "' + r[cfg.nameKey] + '"? This cannot be undone.', 'Delete', true))) return;
      const res = await IMS.api.del(cfg.endpoint + '/' + r[cfg.idKey]);
      if (res.ok) { IMS.ui.toast(cfg.noun[0].toUpperCase() + cfg.noun.slice(1) + ' deleted'); reload(); return; }
      const used = cfg.usageKey && r[cfg.usageKey] ? ' In use by ' + r[cfg.usageKey] + ' record(s).' : '';
      IMS.ui.toast(res.error && res.error.code === 'IN_USE' ? 'Cannot delete: it is still referenced.' + used + ' ' + IMS.api.message(res) : IMS.api.message(res), 'error');
    }

    // add / edit dialog
    function form(r) {
      const edit = !!r, inputs = {}, errs = {};
      const body = h('div', { class: 'form-grid' }, cfg.fields.map((fd) => {
        inputs[fd.key] = h('input', { class: 'input', type: fd.type || 'text', value: edit && r[fd.key] !== null ? r[fd.key] : '', maxlength: fd.max || 80 });
        errs[fd.key] = h('span', { class: 'err' });
        const d = IMS.ui.field(fd.label + (fd.required ? '' : ' (optional)'), inputs[fd.key], fd.hint);
        d.appendChild(errs[fd.key]);
        if (fd.wide) d.classList.add('wide');
        return d;
      }));
      const dlg = IMS.ui.modal({ title: (edit ? 'Edit ' : 'Add ') + cfg.noun, content: body, buttons: [
        { label: 'Cancel', close: true, onClick: () => {} },
        { label: edit ? 'Save changes' : cfg.addLabel, variant: 'primary', onClick: async () => {
          dlg.setError('');
          let bad = false;
          const data = {};
          for (const fd of cfg.fields) {
            const v = inputs[fd.key].value.trim();
            errs[fd.key].textContent = fd.required && !v ? fd.label + ' is required' : '';
            if (fd.required && !v) bad = true;
            data[fd.key] = v;
          }
          if (bad) return false;
          const res = edit ? await IMS.api.put(cfg.endpoint + '/' + r[cfg.idKey], data) : await IMS.api.post(cfg.endpoint, data);
          if (!res.ok) { dlg.setError(IMS.api.message(res)); return false; }
          IMS.ui.toast(edit ? 'Changes saved' : cfg.noun[0].toUpperCase() + cfg.noun.slice(1) + ' added');
          dlg.close();
          reload();
        } }] });
    }

    container.append(h('section', { class: 'card' }, h('div', { class: 'toolbar' }, find,
      cfg.canWrite ? IMS.ui.btn(cfg.addLabel, { variant: 'primary', onClick: () => form(null) }) : null), table.el));
    return reload().then(() => ({ reload }));
  }

  return { mount };
})();
