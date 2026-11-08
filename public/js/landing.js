// landing.js : small touches for the landing page. No libraries.
//  1. the sign-in buttons say "Open your dashboard" when you are already signed in
//  2. the stockroom scene follows the pointer a little
//  3. the screen ring turns by itself, by dragging, or with the arrows
//  4. the feature cards tilt toward the pointer and show their small 3D objects at work
(function () {
  'use strict';
  const reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  // 1. already signed in? offer the dashboard instead of the sign-in page
  fetch('/api/auth/me', { credentials: 'same-origin' }).then((r) => {
    if (!r.ok) return;
    document.querySelectorAll('[data-cta]').forEach((a) => {
      a.href = 'dashboard.html';
      a.textContent = a.classList.contains('btn-sm') || a.closest('footer') ? 'Dashboard' : 'Open your dashboard';
    });
  }).catch(() => { /* offline or server asleep: leave the sign-in links as they are */ });

  // 2. the scene leans towards the pointer (the camera moves a few degrees, nothing more)
  const hero = document.querySelector('.hero');
  const stage = document.getElementById('stage');
  if (hero && stage && !reduce) {
    let tx = 0, ty = 0, cx = 0, cy = 0;
    hero.addEventListener('pointermove', (e) => {
      const r = hero.getBoundingClientRect();
      tx = ((e.clientX - r.left) / r.width - 0.5) * 2;
      ty = ((e.clientY - r.top) / r.height - 0.5) * 2;
    });
    let pointing = false;
    hero.addEventListener('pointerenter', () => { pointing = true; });
    hero.addEventListener('pointerleave', () => { pointing = false; });
    (function frame(t) {
      if (!pointing) {                     // nobody pointing: the camera drifts slowly around the stack
        tx = Math.sin(t / 4200) * 0.45;
        ty = Math.cos(t / 5600) * 0.25;
      }
      cx += (tx - cx) * 0.05;
      cy += (ty - cy) * 0.05;
      stage.style.setProperty('--py', (cx * 11).toFixed(3) + 'deg');
      stage.style.setProperty('--px', (-cy * 6).toFixed(3) + 'deg');
      requestAnimationFrame(frame);
    })(0);
  }

  // 3. the ring of screens
  const ring = document.getElementById('ring');
  const wrap = document.getElementById('ringStage');
  if (ring && wrap) {
    const STEP = 72;                       // five screens, 360 / 5 degrees apart
    let angle = -12, goal = null, dragging = false, hovering = false, lastX = 0;
    const draw = () => { ring.style.transform = 'rotateY(' + angle.toFixed(2) + 'deg)'; };
    const snapTo = (dir) => { goal = Math.round(angle / STEP) * STEP + dir * STEP; };
    document.getElementById('ringNext').addEventListener('click', () => snapTo(-1));
    document.getElementById('ringPrev').addEventListener('click', () => snapTo(1));
    wrap.addEventListener('pointerenter', () => { hovering = true; });
    wrap.addEventListener('pointerleave', () => { hovering = false; });
    wrap.addEventListener('pointerdown', (e) => { dragging = true; goal = null; lastX = e.clientX; wrap.classList.add('drag'); wrap.setPointerCapture(e.pointerId); });
    wrap.addEventListener('pointermove', (e) => { if (!dragging) return; angle += (e.clientX - lastX) * 0.32; lastX = e.clientX; draw(); });
    const release = () => { dragging = false; wrap.classList.remove('drag'); };
    wrap.addEventListener('pointerup', release);
    wrap.addEventListener('pointercancel', release);
    draw();
    (function frame() {
      if (goal !== null) {                 // glide to the chosen screen
        angle += (goal - angle) * 0.12;
        if (Math.abs(goal - angle) < 0.05) { angle = goal; goal = null; }
        draw();
      } else if (!reduce && !dragging && !hovering) {   // slow turn when nobody is touching it
        angle -= 0.07;
        draw();
      }
      requestAnimationFrame(frame);
    })();
  }
  // 4. feature cards: tilt toward the mouse; on touch screens they play once when they scroll into view
  const cards = document.querySelectorAll('.feat');
  const fine = window.matchMedia('(hover: hover) and (pointer: fine)').matches;
  if (fine && !reduce) {
    cards.forEach((card) => {
      card.addEventListener('pointermove', (e) => {
        const r = card.getBoundingClientRect();
        const x = (e.clientX - r.left) / r.width, y = (e.clientY - r.top) / r.height;
        card.style.setProperty('--ty', ((x - 0.5) * 12).toFixed(2) + 'deg');
        card.style.setProperty('--tx', ((0.5 - y) * 9).toFixed(2) + 'deg');
        card.style.setProperty('--mx', (x * 100).toFixed(1) + '%');
        card.style.setProperty('--my', Math.min(100, y * 160).toFixed(1) + '%');
      });
      card.addEventListener('pointerleave', () => {
        ['--tx', '--ty'].forEach((k) => card.style.setProperty(k, '0deg'));
        card.style.setProperty('--mx', '50%'); card.style.setProperty('--my', '40%');
      });
    });
  } else if (!fine && 'IntersectionObserver' in window) {
    const io = new IntersectionObserver((list) => list.forEach((en) => en.target.classList.toggle('on', en.isIntersecting)), { threshold: 0.7 });
    cards.forEach((card) => io.observe(card));
  }
})();
