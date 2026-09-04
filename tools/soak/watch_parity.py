#!/usr/bin/env python3
"""Veilleur de campagne de soak : n'emet que sur evenement notable.

Chaque ligne imprimee devient une notification, donc le filtre doit etre
selectif -- mais selectif veut dire "ce sur quoi j'agirais", pas "seulement
les bonnes nouvelles". Le silence ne doit jamais pouvoir cacher une panne :
on emet aussi bien sur divergence que sur redemarrage, coupure durable ou
derive memoire.

    python tools/soak/watch_parity.py [--host 192.168.1.141] [--interval 300]
"""
import argparse
import json
import sys
import time
import urllib.request

MILESTONES = (50, 100, 150, 200, 300, 500)
HEAP_FLOOR = 40000          # octets : en dessous, on veut le savoir
OUTAGE_ALERT_S = 900        # coupure signalee au-dela de 15 min


def snapshot(host):
    with urllib.request.urlopen('http://%s/api/diagnostics' % host, timeout=10) as r:
        d = json.loads(r.read())
    p = d.get('parity') or {}
    mem = d.get('memory') or {}
    return {
        'ok': p.get('ok', 0),
        'ko': p.get('ko', 0),
        'uptime': (d.get('system') or {}).get('uptimeSec', 0),
        'heap': mem.get('heapFree', 0),
        'heapmin': mem.get('minFreeBytesSeen', 0),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--host', default='192.168.1.141')
    ap.add_argument('--interval', type=int, default=300)
    args = ap.parse_args()

    seen_ko = 0
    seen_milestones = set()
    prev_uptime = None
    offline_since = None
    offline_reported = False
    heap_reported = False

    while True:
        try:
            s = snapshot(args.host)
        except Exception:  # noqa: BLE001 - coupure reseau, pas une divergence
            if offline_since is None:
                offline_since = time.time()
            elif not offline_reported and (time.time() - offline_since) > OUTAGE_ALERT_S:
                print('MODULE INJOIGNABLE depuis %d min'
                      % int((time.time() - offline_since) / 60), flush=True)
                offline_reported = True
            time.sleep(args.interval)
            continue

        if offline_since is not None:
            if offline_reported:
                print('RETOUR EN LIGNE apres %d min (ok=%d ko=%d)'
                      % (int((time.time() - offline_since) / 60), s['ok'], s['ko']),
                      flush=True)
            offline_since = None
            offline_reported = False

        # 1. Divergence : le seul evenement bloquant.
        if s['ko'] > seen_ko:
            print('DIVERGENCE PARITE : ko=%d (ok=%d) -- campagne a arreter'
                  % (s['ko'], s['ok']), flush=True)
            seen_ko = s['ko']

        # 2. Redemarrage : remet le compteur et l'horloge d'endurance a zero.
        if prev_uptime is not None and s['uptime'] < prev_uptime:
            print('REDEMARRAGE detecte : uptime retombe a %ds, ok=%d '
                  '-- horloge d endurance repartie de zero'
                  % (s['uptime'], s['ok']), flush=True)
        prev_uptime = s['uptime']

        # 3. Jalons de volume.
        for m in MILESTONES:
            if s['ok'] >= m and m not in seen_milestones:
                seen_milestones.add(m)
                print('JALON %d cycles atteint (ok=%d ko=%d, uptime %.2f j)'
                      % (m, s['ok'], s['ko'], s['uptime'] / 86400.0), flush=True)

        # 4. Derive memoire.
        if s['heap'] and s['heap'] < HEAP_FLOOR and not heap_reported:
            print('MEMOIRE BASSE : heapFree=%d (plancher vu %d)'
                  % (s['heap'], s['heapmin']), flush=True)
            heap_reported = True

        time.sleep(args.interval)


if __name__ == '__main__':
    sys.exit(main())
