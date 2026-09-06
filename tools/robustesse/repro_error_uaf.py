#!/usr/bin/env python3
"""Reproduction ciblee de la corruption de tas du chemin d'erreur AsyncTCP.

Origine : plantage du 6 septembre 2026 a 10:52, trace decodee ---

    _async_service_task -> AsyncClient::_error -> lambda onDisconnect
    -> _onDisconnect -> _handleDisconnect -> ~AsyncWebServerRequest
    -> list<AsyncWebHeader>::clear() -> operator delete
    -> assert block_merge_prev (heap_tlsf.c:344)

Le rappel onDisconnect enregistre par le serveur web fait `delete req` PUIS
`delete c` -- or `c` est le `this` de AsyncClient::_error(), qui enchaine deux
rappels sans garde de reentrance. La signature (assert pendant la liberation
de la liste d'en-tetes) est celle d'une double liberation.

Ce script provoque le meme chemin sans toucher au reseau de la maison : on
ouvre des requetes AVEC en-tetes -- pour que la liste soit allouee -- puis on
les abat par RST (SO_LINGER a 0), ce qui declenche _error() cote module et non
une fermeture propre.

    python tools/robustesse/repro_error_uaf.py --host 192.168.1.141 \
                                               --rounds 40 --workers 8

Sortie : uptime du module a chaque tour. Un uptime qui retombe = plantage
reproduit.
"""
import argparse
import json
import socket
import struct
import sys
import threading
import time
import urllib.request

PARTIAL = (
    'GET /api/status HTTP/1.1\r\n'
    'Host: aqualook\r\n'
    'User-Agent: repro-uaf\r\n'
    'Accept: application/json\r\n'
    'X-Filler-1: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\r\n'
    'X-Filler-2: bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\r\n'
    'X-Filler-3: cccccccccccccccccccccccccccccccccccc\r\n'
)


def abort_after_headers(host, port, hold_s):
    """Ouvre, envoie des en-tetes, puis coupe par RST."""
    try:
        s = socket.create_connection((host, port), timeout=3)
        # SO_LINGER a 0 : la fermeture envoie un RST, pas un FIN propre.
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                     struct.pack('ii', 1, 0))
        s.sendall(PARTIAL.encode())
        time.sleep(hold_s)          # laisser le module allouer la requete
        s.close()                   # -> RST -> AsyncClient::_error()
    except Exception:  # noqa: BLE001 - un echec de connexion n'est pas grave
        pass


def uptime(host):
    try:
        with urllib.request.urlopen('http://%s/api/diagnostics' % host,
                                    timeout=4) as r:
            return json.loads(r.read())['system']['uptimeSec']
    except Exception:  # noqa: BLE001
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--host', default='192.168.1.141')
    ap.add_argument('--port', type=int, default=80)
    ap.add_argument('--rounds', type=int, default=40)
    ap.add_argument('--workers', type=int, default=8)
    ap.add_argument('--hold', type=float, default=0.12)
    args = ap.parse_args()

    base = uptime(args.host)
    if base is None:
        print('module injoignable au depart'); return 2
    print('uptime initial : %ss' % base, flush=True)

    for rnd in range(1, args.rounds + 1):
        threads = [threading.Thread(target=abort_after_headers,
                                    args=(args.host, args.port, args.hold))
                   for _ in range(args.workers)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        up = uptime(args.host)
        if up is None:
            print('tour %d : MODULE INJOIGNABLE' % rnd, flush=True)
            time.sleep(3)
            continue
        if up < base:
            print('tour %d : REDEMARRAGE -- uptime %ss < %ss : defaut reproduit'
                  % (rnd, up, base), flush=True)
            return 1
        base = up
        if rnd % 5 == 0:
            print('tour %d : uptime %ss, module debout' % (rnd, up), flush=True)
        time.sleep(0.3)

    print('%d tours sans plantage' % args.rounds, flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
