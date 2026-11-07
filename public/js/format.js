// format.js : rupee amounts (Indian digit grouping) and dates ("07 Nov 2026").
window.IMS = window.IMS || {};

IMS.fmt = (function () {
  const MONTHS = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

  // 165080 -> "₹1,65,080"; 12.5 -> "₹12.50"
  function money(n) {
    const v = Number(n);
    if (!isFinite(v)) return '—';
    const digits = Number.isInteger(v) ? 0 : 2;
    return '₹' + v.toLocaleString('en-IN', { minimumFractionDigits: digits, maximumFractionDigits: digits });
  }

  // 1234 -> "1,234" (Indian grouping for counts too)
  function int(n) {
    const v = Number(n);
    return isFinite(v) ? v.toLocaleString('en-IN') : '—';
  }

  // "2026-11-07" or "2026-11-07 14:05:00" -> "07 Nov 2026" (read as text: no time-zone shifts)
  function date(s) {
    const m = /^(\d{4})-(\d{2})-(\d{2})/.exec(s || '');
    return m ? m[3] + ' ' + MONTHS[Number(m[2]) - 1] + ' ' + m[1] : '—';
  }

  // "2026-11-07 14:05:00" -> "07 Nov 2026, 14:05"
  function dateTime(s) {
    const m = /^(\d{4}-\d{2}-\d{2})[ T](\d{2}:\d{2})/.exec(s || '');
    return m ? date(m[1]) + ', ' + m[2] : date(s);
  }

  // today as "YYYY-MM-DD" in local time
  function today() {
    const d = new Date();
    const p = (x) => String(x).padStart(2, '0');
    return d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate());
  }

  // first letter(s) for the avatar
  function initials(name) {
    return String(name || '?').slice(0, 2).toUpperCase();
  }

  return { money, int, date, dateTime, today, initials };
})();
