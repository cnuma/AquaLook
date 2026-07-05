(() => {
  const panel = document.getElementById('fault-panel');
  const text = document.getElementById('fault-panel-text');

  if (panel && text) {
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
  }

  function getStorageBanner() {
    let banner = document.getElementById('storage-warning');
    if (banner) return banner;

    banner = document.createElement('div');
    banner.id = 'storage-warning';
    banner.setAttribute('role', 'alert');
    banner.style.cssText = [
      'display:none',
      'box-sizing:border-box',
      'width:100%',
      'padding:10px 14px',
      'border-bottom:1px solid #ffb300',
      'background:#3b2a00',
      'color:#ffe082',
      'font:600 13px/1.4 sans-serif',
      'text-align:center',
      'position:relative',
      'z-index:10001'
    ].join(';');

    document.body.insertBefore(banner, document.body.firstChild);
    return banner;
  }

  async function refreshStorageState() {
    try {
      const response = await fetch(
        '/api/storage',
        { cache: 'no-store' }
      );

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      const storage = await response.json();
      const banner = getStorageBanner();

      if (storage.status === 'ready') {
        banner.style.display = 'none';
        banner.textContent = '';
        return;
      }

      banner.textContent = `⚠ Stockage SD : ${storage.message}`;
      banner.style.display = 'block';
    } catch (error) {
      console.log('[storage-status] indisponible', error);
    }
  }

  refreshStorageState();
  setInterval(refreshStorageState, 5000);
})();
