/* llc-main.js — 侧边目录滚动高亮与平滑滚动 */
'use strict';

(function () {
  const links = Array.from(document.querySelectorAll('.sidebar nav a'));
  const secs = links.map(a => document.getElementById(a.getAttribute('href').slice(1))).filter(Boolean);

  function onScroll() {
    const pos = window.scrollY + 90;
    let cur = links[0];
    for (let i = 0; i < secs.length; i++) {
      if (secs[i].offsetTop <= pos) cur = links[i];
    }
    links.forEach(a => a.classList.toggle('active', a === cur));
  }
  window.addEventListener('scroll', onScroll, { passive: true });
  onScroll();
})();
