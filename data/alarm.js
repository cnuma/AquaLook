(() => {
  const panel = document.getElementById('fault-panel');
  const text = document.getElementById('fault-panel-text');

  if (!panel || !text) return;

  // Le point discret historique ouvrait /api/logs, qui est la vue brute.
  // Il doit désormais ouvrir la vraie page du journal avec acquittement.
  document
    .querySelectorAll('a[href="/api/logs"]')
    .forEach(link => {
      link.setAttribute('href', '/logs');
      link.setAttribute(
        'title',
        'Journal et acquittement des alarmes'
      );
    });

  async function refreshFaultPanel() {
    try {
      const response = await fetch(
        '/api/faults',
        { cache: 'no-store' }
      );

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      const fault = await response.json();

      panel.classList.remove(
        'fault-hidden',
        'fault-unacknowledged',
        'fault-acknowledged'
      );

      if (fault.unacknowledged) {
        panel.classList.add('fault-unacknowledged');
        text.textContent =
          fault.active
            ? 'Erreur active — consulter et acquitter'
            : 'Erreur mémorisée — consulter et acquitter';
        return;
      }

      if (fault.active) {
        panel.classList.add('fault-acknowledged');
        text.textContent =
          'Erreur acquittée — défaut toujours présent';
        return;
      }

      panel.classList.add('fault-hidden');
    } catch (error) {
      console.log('[fault-panel] indisponible', error);
      panel.classList.add('fault-hidden');
    }
  }

  refreshFaultPanel();
  setInterval(refreshFaultPanel, 2000);
})();
