#!/usr/bin/env python3
"""Configure le banc pour une campagne de soak de parite V4.

Coupe les notifications ntfy des zones (sinon des centaines d'alertes partent
vers le telephone) puis installe des creneaux courts et frequents afin
d'accumuler du volume de cycles. La sauvegarde prealable et le retour en etat
sont assures par restore_planning.py.

    python tools/soak/apply_soak_planning.py [--host 192.168.1.141]
"""
import argparse
import json
import sys
import time
import urllib.request

HOURS = (7, 11, 15, 19, 22)   # 5 creneaux par jour
DURATION_MIN = 2              # cycles courts
DAYS = 7


def post(host, path, payload):
    req = urllib.request.Request(
        'http://%s%s' % (host, path),
        data=json.dumps(payload).encode(),
        headers={'Content-Type': 'application/json'},
        method='POST')
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            return r.status
    except Exception as exc:  # noqa: BLE001
        return 'ERR:%s' % exc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--host', default='192.168.1.141')
    ap.add_argument('--zones', type=int, default=4)
    args = ap.parse_args()

    errors = 0
    # 1) Silence d'abord : aucun creneau ne doit notifier pendant la campagne.
    for zone in range(args.zones):
        status = post(args.host, '/api/zoneNotifications',
                      {'zone': zone, 'notifyStart': False, 'notifyStop': False})
        if status != 200:
            errors += 1
            print('  echec silence zone %d : %s' % (zone + 1, status))
        time.sleep(0.1)
    print('notifications coupees sur %d zones' % args.zones)

    # 2) Creneaux : decales de 5 min par zone pour eviter les chevauchements.
    count = 0
    for zone in range(args.zones):
        for day in range(DAYS):
            for idx, hour in enumerate(HOURS):
                status = post(args.host, '/api/dayslot', {
                    'zone': zone, 'day': day, 'slotIdx': idx,
                    'hour': hour, 'minute': zone * 5,
                    'duration': DURATION_MIN, 'enabled': True,
                })
                count += 1
                if status != 200:
                    errors += 1
                    print('  echec zone %d jour %d slot %d : %s'
                          % (zone + 1, day, idx, status))
                time.sleep(0.05)
    print('%d creneaux installes, %d erreur(s)' % (count, errors))
    print('volume attendu : %d cycles/jour' % (args.zones * len(HOURS)))
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
