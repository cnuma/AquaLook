#!/usr/bin/env python3
"""Entasse des cycles d'arrosage pour accelerer la preuve de parite V4.

Pourquoi : le Gate 1 demande >= 100 cycles ET >= 7 jours continus. Ces deux
criteres ne mesurent pas la meme chose -- le volume prouve que V4 EXECUTE
juste, la duree prouve que le systeme TIENT (passage de minuit, resync NTP,
derive memoire, races rares). Ce pilote ne sert qu'au premier : il reprogramme
les creneaux du jour pour qu'ils retombent quelques minutes plus tard, en
boucle, ce qui donne ~40 cycles par passe sans jamais redemarrer le module --
donc sans casser l'horloge d'endurance qui, elle, continue de tourner.

Arret immediat au premier desaccord : une divergence doit etre comprise, pas
noyee dans le volume.

    python tools/soak/cram_cycles.py [--target 150] [--host 192.168.1.141]

A la fin, le planning de soak standard est reinstalle (40 cycles/jour).
"""
import argparse
import datetime as dt
import json
import os
import sys
import time
import urllib.request

SLOTS_PER_DAY = 5      # limite materielle du modele de planning
SLOT_SPACING_MIN = 8   # ecart entre deux creneaux d'une meme zone
DURATION_MIN = 2
LEAD_MIN = 4           # marge avant le premier creneau d'une passe


# Le reseau de la maison se coupe vers 3h30. Sans patience, la campagne
# mourrait sur ce blip et la nuit serait perdue : on retente longtemps
# plutot que d'echouer. Une coupure n'est pas une divergence.
OUTAGE_PATIENCE_S = 2400   # 40 min : large devant une coupure de box
RETRY_WAIT_S = 20


def _try_open(req):
    with urllib.request.urlopen(req, timeout=12) as r:
        return r.status, r.read()


def post(host, path, payload):
    req = urllib.request.Request(
        'http://%s%s' % (host, path),
        data=json.dumps(payload).encode(),
        headers={'Content-Type': 'application/json'},
        method='POST')
    deadline = time.time() + OUTAGE_PATIENCE_S
    last = None
    while time.time() < deadline:
        try:
            return _try_open(req)[0]
        except Exception as exc:  # noqa: BLE001
            last = exc
            time.sleep(RETRY_WAIT_S)
    return 'ERR:%s' % last


def get(host, path):
    """Lecture patiente : rend la main seulement apres une coupure durable."""
    req = urllib.request.Request('http://%s%s' % (host, path))
    deadline = time.time() + OUTAGE_PATIENCE_S
    last = None
    while time.time() < deadline:
        try:
            return json.loads(_try_open(req)[1])
        except Exception as exc:  # noqa: BLE001
            last = exc
            print('  reseau indisponible (%s), nouvelle tentative dans %ds'
                  % (type(last).__name__, RETRY_WAIT_S), flush=True)
            time.sleep(RETRY_WAIT_S)
    raise RuntimeError('module injoignable depuis %d s : %s'
                       % (OUTAGE_PATIENCE_S, last))


def module_now(host):
    """Heure du module, pas celle du PC : c'est elle qui declenche."""
    raw = get(host, '/api/status')['time']          # "04/09/2026 21:35:28"
    return dt.datetime.strptime(raw, '%d/%m/%Y %H:%M:%S')


def parity(host):
    p = get(host, '/api/diagnostics').get('parity') or {}
    return p.get('ok', 0), p.get('ko', 0)


def program_pass(host, zones, start):
    """Programme une passe. Chaque creneau va sur SON jour : le passage de
    minuit est ainsi gere sans cas particulier (weekday() de Python et l'index
    du module partagent la meme convention 0=lundi)."""
    last = start
    errors = 0
    for k in range(SLOTS_PER_DAY):
        slot_start = start + dt.timedelta(minutes=k * SLOT_SPACING_MIN)
        for z in range(zones):
            when = slot_start + dt.timedelta(minutes=z)
            last = max(last, when)
            status = post(host, '/api/dayslot', {
                'zone': z, 'day': when.weekday(), 'slotIdx': k,
                'hour': when.hour, 'minute': when.minute,
                'duration': DURATION_MIN, 'enabled': True,
            })
            if status != 200:
                errors += 1
            time.sleep(0.05)
    return last, errors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--host', default='192.168.1.141')
    ap.add_argument('--zones', type=int, default=8)
    ap.add_argument('--target', type=int, default=150)
    ap.add_argument('--max-passes', type=int, default=8)
    args = ap.parse_args()
    # Depuis D016 lot F, les ecritures exigent une session Web locale.
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from module_session import install_global_session
    install_global_session(args.host)

    ok0, ko0 = parity(args.host)
    print('depart : ok=%d ko=%d, cible=%d' % (ok0, ko0, args.target), flush=True)
    if ko0:
        print('desaccord deja present : on n entasse pas par dessus.', flush=True)
        return 1

    for p in range(1, args.max_passes + 1):
        ok, ko = parity(args.host)
        if ko:
            print('ARRET : desaccord detecte (ko=%d)' % ko, flush=True)
            return 1
        if ok >= args.target:
            print('cible atteinte : ok=%d' % ok, flush=True)
            break

        start = module_now(args.host) + dt.timedelta(minutes=LEAD_MIN)
        last, errors = program_pass(args.host, args.zones, start)
        window = (last - module_now(args.host)).total_seconds() + DURATION_MIN * 60 + 60
        print('passe %d : %d creneaux, %d erreur(s), fenetre %.0f min (ok=%d)'
              % (p, args.zones * SLOTS_PER_DAY, errors, window / 60.0, ok), flush=True)

        # Attente de la fin de fenetre, en surveillant les desaccords.
        deadline = time.time() + max(60.0, window)
        while time.time() < deadline:
            time.sleep(60)
            ok, ko = parity(args.host)
            if ko:
                print('ARRET EN COURS DE PASSE : ko=%d' % ko, flush=True)
                return 1
        print('  fin de passe : ok=%d ko=%d' % parity(args.host), flush=True)

    ok, ko = parity(args.host)
    print('entassement termine : ok=%d ko=%d' % (ok, ko), flush=True)

    # Retour au rythme normal : l'endurance prend le relais, minuits compris.
    print('reinstallation du planning de soak standard...', flush=True)
    import subprocess
    subprocess.run([sys.executable, 'tools/soak/apply_soak_planning.py',
                    '--host', args.host, '--zones', str(args.zones)], check=False)
    return 0 if ko == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
