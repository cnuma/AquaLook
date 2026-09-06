#!/usr/bin/env python3
"""Annonce un redemarrage volontaire (flash, intervention) au veilleur.

Sans cela, chaque flash serait signale comme "REDEMARRAGE INEXPLIQUE" et
l'alerte, a force de crier au loup, cesserait d'etre lue -- alors que c'est
precisement le signal qui doit rester credible.

    python tools/soak/announce_reboot.py     # a lancer AVANT le flash
"""
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
    led['redemarrage_annonce'] = True
    with open(LEDGER, 'w', encoding='utf-8') as fh:
        json.dump(led, fh, indent=1)
    print('prochain redemarrage : annonce')
    return 0


if __name__ == '__main__':
    sys.exit(main())
