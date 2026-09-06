#!/usr/bin/env python3
"""Annonce un redemarrage volontaire (flash, intervention) au veilleur.

Sans cela, chaque flash serait signale comme "REDEMARRAGE INEXPLIQUE" et
l'alerte, a force de crier au loup, cesserait d'etre lue -- alors que c'est
precisement le signal qui doit rester credible.

    python tools/soak/announce_reboot.py     # a lancer AVANT le flash
"""
import datetime as dt
import json
import os
import sys

LEDGER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ledger.json')


def main():
    try:
        with open(LEDGER, encoding='utf-8') as fh:
            led = json.load(fh)
    except Exception:  # noqa: BLE001
        led = {'cumul_avant_redemarrages': 0, 'redemarrages': 0}
    # Une fenetre, pas un drapeau a usage unique : une sequence de flash et
    # de verification enchaine plusieurs redemarrages, et le veilleur ne
    # sonde que toutes les 5 minutes. Un drapeau consomme une seule fois
    # laissait passer les suivants en 'INEXPLIQUE' -- de fausses alertes
    # qui usent la credibilite du seul signal qui doit rester ecoute.
    fin = dt.datetime.now() + dt.timedelta(minutes=20)
    led['maintenance_jusqu_a'] = fin.isoformat(timespec='seconds')
    led.pop('redemarrage_annonce', None)
    with open(LEDGER, 'w', encoding='utf-8') as fh:
        json.dump(led, fh, indent=1)
    print('fenetre de maintenance ouverte jusqu a %s' % fin.strftime('%H:%M'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
