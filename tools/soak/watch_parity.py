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
import datetime as dt
import json
import os
import sys
import time
import urllib.request

MILESTONES = (50, 100, 150, 200, 300, 500)
HEAP_FLOOR = 40000          # octets : en dessous, on veut le savoir
OUTAGE_ALERT_S = 900        # coupure signalee au-dela de 15 min

# Le module se redemarre volontairement chaque jour (verification de mise a
# jour, cf. UpdateCheckScheduler), ce qui remet son compteur de parite a zero.
# Les cycles doivent donc etre cumules HORS du module, sinon la campagne
# semblerait repartir de rien chaque matin.
LEDGER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ledger.json')


def ledger_read():
    try:
        with open(LEDGER, encoding='utf-8') as fh:
            return json.load(fh)
    except Exception:  # noqa: BLE001 - premier demarrage
        return {'cumul_avant_redemarrages': 0, 'redemarrages': 0,
                'debut': dt.datetime.now().isoformat(timespec='seconds')}


def ledger_write(d):
    with open(LEDGER, 'w', encoding='utf-8') as fh:
        json.dump(d, fh, indent=1)


def in_maintenance(led):
    """Une fenetre declaree couvre TOUS les redemarrages qu'elle contient."""
    fin = led.get('maintenance_jusqu_a')
    if not fin:
        return False
    try:
        return dt.datetime.now() <= dt.datetime.fromisoformat(fin)
    except Exception:  # noqa: BLE001
        return False


def now_hour():
    return dt.datetime.now().hour


def snapshot(host):
    with urllib.request.urlopen('http://%s/api/diagnostics' % host, timeout=10) as r:
        d = json.loads(r.read())
    p = d.get('parity') or {}
    mem = d.get('memory') or {}
    return {
        'reset': (d.get('system') or {}).get('resetReason', ''),
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
    # Les jalons deja franchis avant ce demarrage ne sont pas des nouvelles :
    # sans cela, chaque relance du veilleur les reannoncerait tous.
    seen_milestones = {m for m in MILESTONES
                       if m <= ledger_read().get('cumul_avant_redemarrages', 0)}
    prev_uptime = None
    last_ok = 0
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
            led = ledger_read()
            led['cumul_avant_redemarrages'] += last_ok
            led['redemarrages'] += 1
            ledger_write(led)
            # Un flash ou une intervention se declare a l'avance (drapeau
            # du registre) : sans cela chaque flash crierait au loup, et
            # l'alerte perdrait la valeur qui fait tout son interet.
            # Le module sait lui-meme distinguer un redemarrage voulu d'un
            # accident : un plantage laisse "panic / exception", et la garde
            # anti-boucle ne compte comme suspects que les demarrages non
            # planifies. S'en servir evite de crier au loup sur une simple
            # application de configuration -- et rend credible l'alerte quand
            # elle survient vraiment.
            reset = (s.get('reset') or '').lower()
            if 'panic' in reset or 'exception' in reset:
                attendu = 'PLANTAGE (%s)' % s.get('reset')
            elif in_maintenance(led):
                attendu = 'annonce (fenetre de maintenance)'
            elif 3 <= now_hour() <= 4:
                attendu = 'attendu (maintenance)'
            elif 'logiciel' in reset:
                attendu = 'volontaire (%s)' % s.get('reset')
            else:
                attendu = 'INEXPLIQUE'
            ledger_write(led)
            print('REDEMARRAGE (%s) : uptime %ds, %d cycles reportes, '
                  'cumul campagne=%d'
                  % (attendu, s['uptime'], last_ok, led['cumul_avant_redemarrages']),
                  flush=True)
            # Re-amorcer sur le nouveau cumul, sans effacer : vider ferait
            # reannoncer chaque nuit des jalons deja acquis.
            seen_milestones = {m for m in MILESTONES
                               if m <= led['cumul_avant_redemarrages']}
        prev_uptime = s['uptime']
        last_ok = s['ok']

        # 3. Jalons de volume.
        cumul = ledger_read()['cumul_avant_redemarrages'] + s['ok']
        for m in MILESTONES:
            if cumul >= m and m not in seen_milestones:
                seen_milestones.add(m)
                print('JALON %d cycles cumules (session ok=%d ko=%d, uptime %.2f j)'
                      % (m, s['ok'], s['ko'], s['uptime'] / 86400.0), flush=True)

        # 4. Derive memoire.
        if s['heap'] and s['heap'] < HEAP_FLOOR and not heap_reported:
            print('MEMOIRE BASSE : heapFree=%d (plancher vu %d)'
                  % (s['heap'], s['heapmin']), flush=True)
            heap_reported = True

        time.sleep(args.interval)


if __name__ == '__main__':
    sys.exit(main())
