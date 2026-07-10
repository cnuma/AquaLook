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

  function getBuildInfoPanel() {
    let buildInfo = document.getElementById('firmware-build-info');
    if (buildInfo) return buildInfo;

    const systemInfo = document.getElementById('sys-info');
    if (!systemInfo || !systemInfo.parentNode) return null;

    buildInfo = document.createElement('div');
    buildInfo.id = 'firmware-build-info';
    buildInfo.className = 'cfg-info';
    buildInfo.style.marginTop = '8px';
    buildInfo.textContent = 'Compilation : chargement...';
    systemInfo.insertAdjacentElement('afterend', buildInfo);
    return buildInfo;
  }

  async function refreshBuildInfo() {
    try {
      const response = await fetch(
        '/api/diagnostics',
        { cache: 'no-store' }
      );

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      const diagnostics = await response.json();
      const build = diagnostics.build || {};
      const buildInfo = getBuildInfoPanel();
      if (!buildInfo) return;

      const version = build.version || 'unknown';
      const number = build.number || 'unknown';
      const sha = build.gitSha || 'unknown';
      const branch = build.gitBranch || 'unknown';
      const backend = build.relayBackend || 'unknown';
      const compiled = [build.compiledDate, build.compiledTime]
        .filter(Boolean)
        .join(' ');

      buildInfo.innerHTML =
        `Firmware : <span>AquaLook ${version}</span><br>` +
        `Build : <span>${number}</span><br>` +
        `Commit : <span>${sha}</span><br>` +
        `Branche : <span>${branch}</span><br>` +
        `Backend relais : <span>${backend.toUpperCase()}</span><br>` +
        `Compilé le : <span>${compiled || '--'}</span>`;
    } catch (error) {
      console.log('[build-info] indisponible', error);
      const buildInfo = getBuildInfoPanel();
      if (buildInfo) buildInfo.textContent = 'Compilation : indisponible';
    }
  }

  refreshStorageState();
  setInterval(refreshStorageState, 5000);
  refreshBuildInfo();
  setInterval(refreshBuildInfo, 60000);
})();
