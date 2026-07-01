const K = 'aqualook-admin-unlocked';
const A = ['sec-wifi','sec-ntp','sec-owm','sec-display','sec-system','relay-admin'];

function adminApply(v) {
  A.forEach(i => {
    const e = document.getElementById(i);
    if (e) e.style.display = v ? '' : 'none';
  });
  const b = document.getElementById('admin-btn');
  if (b) b.textContent = v ? 'Admin : déverrouillé' : 'Admin : verrouillé';
}

function adminToggle() {
  const unlocked = sessionStorage.getItem(K) === '1';
  if (unlocked) {
    sessionStorage.removeItem(K);
    adminApply(false);
    return;
  }

  const expected = ['159', '8753'].join('');
  if (prompt('Mot de passe administrateur') !== expected) {
    alert('Mot de passe incorrect');
    return;
  }

  sessionStorage.setItem(K, '1');
  adminApply(true);
}

document.addEventListener('DOMContentLoaded', () => {
  adminApply(sessionStorage.getItem(K) === '1');
});
