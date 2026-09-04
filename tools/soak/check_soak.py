#!/usr/bin/env python3
"""Point de situation d'une campagne de soak de parite V4.

Affiche en une fois ce qui decide du Gate 1 : cumul accord/desaccord, duree de
fonctionnement continue, sante memoire, et toute divergence journalisee.
Le verdict est porte par le niveau de log : une ligne PARITE en WARN est un
desaccord, donc un blocage.

    python tools/soak/check_soak.py [--host 192.168.1.141]
"""
import argparse
import json
import sys
import urllib.request


def fetch_json(host, path):
    with urllib.request.urlopen('http://%s%s' % (host, path), timeout=10) as r:
        return json.loads(r.read())


def fetch_text(host, path):
    with urllib.request.urlopen('http://%s%s' % (host, path), timeout=10) as r:
        return r.read().decode('utf-8', 'replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--host', default='192.168.1.141')
    args = ap.parse_args()

    try:
        diag = fetch_json(args.host, '/api/diagnostics')
    except Exception as exc:  # noqa: BLE001
        print('module injoignable : %s' % exc)
        return 2

    parity = diag.get('parity') or {}
    ok = parity.get('ok', 0)
    ko = parity.get('ko', 0)
    uptime = diag.get('system', {}).get('uptimeSec', 0)
    days = uptime / 86400.0

    try:
        log = fetch_text(args.host, '/api/logs.txt')
        divergences = [l for l in log.splitlines() if 'PARITE' in l and 'WARN' in l]
    except Exception:  # noqa: BLE001
        divergences = []

    print('parite     : ok=%d  ko=%d' % (ok, ko))
    print('uptime     : %.2f jour(s)  (%d s)' % (days, uptime))
    heap = diag.get('memory') or diag.get('heap')
    if heap:
        print('memoire    : %s' % json.dumps(heap)[:120])

    # Criteres du Gate 1 (cf. AQUALOOK_V4_CRITERE_FIABILITE.md).
    checks = [
        ('desaccord = 0', ko == 0),
        ('>= 100 cycles', ok >= 100),
        ('>= 7 jours continus', days >= 7.0),
    ]
    print('--- Gate 1 ---')
    for label, passed in checks:
        print('  [%s] %s' % ('x' if passed else ' ', label))

    if divergences:
        print('DIVERGENCES journalisees (%d) :' % len(divergences))
        for line in divergences[:5]:
            print('   ', line)
        return 1
    if ko:
        print('ATTENTION : ko=%d alors qu aucune ligne WARN n est encore'
              ' visible dans le journal circulaire.' % ko)
        return 1
    print('aucune divergence.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
