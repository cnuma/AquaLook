// Ajoute un marqueur visuel aux cartes et lignes des zones en mode intervalle.
// Ce fichier reste séparé de app.js afin de limiter la modification au rendu.
(function () {
  function applyIntervalClasses() {
    const zones = (typeof status !== 'undefined' && status && status.zones) || [];
    const container = document.getElementById('zones-container');
    if (!container) return;

    const cards = container.querySelectorAll('.zone-card');
    cards.forEach((card, index) => {
      card.classList.toggle('zone-mode-interval', zones[index]?.mode === 1);
    });

    const rows = container.querySelectorAll('.zones-table tbody tr');
    rows.forEach((row, index) => {
      row.classList.toggle('zone-mode-interval', zones[index]?.mode === 1);
    });
  }

  document.addEventListener('DOMContentLoaded', () => {
    const container = document.getElementById('zones-container');
    if (!container) return;

    const observer = new MutationObserver(applyIntervalClasses);
    observer.observe(container, { childList: true, subtree: true });
    applyIntervalClasses();
  });
})();
