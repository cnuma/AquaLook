#!/usr/bin/env python3
"""Restaure le planning et les notifications du module depuis la sauvegarde.

Utilise avant/apres une campagne de soak de parite V4 : la campagne ecrase les
creneaux du banc pour generer du volume et coupe les notifications ntfy afin de
ne pas noyer l'utilisateur. Ce script remet tout exactement en etat.

    python tools/soak/restore_planning.py [--host 192.168.1.141]
                                          [--file tools/soak/backup_planning_141.json]
"""
import argparse
import json
import sys
import time
import urllib.request


def post(host, path, payload):
    req = urllib.request.Request(
        'http://%s%s' % (host, path),
        data=json.dumps(payload).encode(),
        headers={'Content-Type': 'application/json'},
        method='POST')
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            return r.status
    except Exception as exc:  # noqa: BLE001 - on veut le detail a l'ecran
        return 'ERR:%s' % exc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--host', default='192.168.1.141')
    ap.add_argument('--file', default='tools/soak/backup_planning_141.json')
    args = ap.parse_args()

    with open(args.file, encoding='utf-8') as fh:
        backup = json.load(fh)

    slots = 0
    errors = 0
    for entry in backup['zones']:
        zone = entry['zone']
        for day, day_slots in enumerate(entry['daySlots'] or []):
            for idx, slot in enumerate(day_slots):
                status = post(args.host, '/api/dayslot', {
                    'zone': zone, 'day': day, 'slotIdx': idx,
                    'hour': slot['h'], 'minute': slot['m'],
                    'duration': slot['d'], 'enabled': bool(slot['e']),
                })
                slots += 1
                if status != 200:
                    errors += 1
                    print('  echec zone %d jour %d slot %d : %s'
                          % (zone + 1, day, idx, status))
                time.sleep(0.05)

        status = post(args.host, '/api/zoneNotifications', {
            'zone': zone,
            'notifyStart': bool(entry.get('notifyStart')),
            'notifyStop': bool(entry.get('notifyStop')),
        })
        if status != 200:
            errors += 1
            print('  echec notifications zone %d : %s' % (zone + 1, status))
        time.sleep(0.1)

    print('%d creneaux restaures, %d erreur(s)' % (slots, errors))
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
