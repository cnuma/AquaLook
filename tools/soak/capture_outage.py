#!/usr/bin/env python3
"""Capture serie programmee, pour observer une coupure reseau du module.

Pendant une coupure, l'API HTTP est inaccessible : le port serie est le seul
oeil disponible. La capture est donc armee a l'avance et tourne toute seule,
sans dependre d'une intervention au bon moment.

Robuste par construction : si le port disparait (re-enumeration USB apres un
redemarrage), on le rouvre au lieu d'abandonner -- une capture qui meurt en
cours de test ne vaut rien.

    python tools/soak/capture_outage.py --start 14:50 --stop 15:45 \
                                        --out capture.log
"""
import argparse
import datetime as dt
import sys
import time

try:
    import serial
except ImportError:  # noqa: BLE001
    print('pyserial absent'); sys.exit(2)


def parse_hhmm(value):
    h, m = value.split(':')
    now = dt.datetime.now()
    moment = now.replace(hour=int(h), minute=int(m), second=0, microsecond=0)
    # Une minute deja entamee vaut "maintenant", pas "demain" : passer l'heure
    # courante en --start faisait sinon attendre 24 h, et la capture manquait
    # l'evenement qu'elle devait observer (constate le 6 septembre 2026, en
    # pleine coupure reseau).
    if moment < now - dt.timedelta(minutes=1):
        moment += dt.timedelta(days=1)
    return max(moment, now)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', default='COM4')
    ap.add_argument('--start', required=True, help='HH:MM')
    ap.add_argument('--stop', required=True, help='HH:MM')
    ap.add_argument('--out', required=True)
    args = ap.parse_args()

    start = parse_hhmm(args.start)
    stop = parse_hhmm(args.stop)
    if stop <= start:
        stop += dt.timedelta(days=1)

    print('capture armee : %s -> %s sur %s'
          % (start.strftime('%H:%M'), stop.strftime('%H:%M'), args.port), flush=True)

    while dt.datetime.now() < start:
        time.sleep(min(30, (start - dt.datetime.now()).total_seconds() + 1))

    print('debut de capture', flush=True)
    lines = 0
    with open(args.out, 'w', encoding='utf-8') as out:
        ser = None
        while dt.datetime.now() < stop:
            try:
                if ser is None:
                    ser = serial.Serial(args.port, 115200, timeout=0.5)
                    ser.setDTR(True)
                    ser.setRTS(False)
                    out.write('--- port ouvert %s ---\n'
                              % dt.datetime.now().strftime('%H:%M:%S'))
                    out.flush()
                raw = ser.readline()
            except Exception as exc:  # noqa: BLE001 - port perdu, on repart
                out.write('--- port perdu (%s), reouverture ---\n' % type(exc).__name__)
                out.flush()
                try:
                    if ser:
                        ser.close()
                except Exception:  # noqa: BLE001
                    pass
                ser = None
                time.sleep(2)
                continue

            if raw:
                text = raw.decode('utf-8', 'replace').rstrip()
                if text.strip():
                    lines += 1
                    out.write('%s %s\n'
                              % (dt.datetime.now().strftime('%H:%M:%S'), text))
                    out.flush()
        if ser:
            ser.close()

    print('capture terminee : %d ligne(s) dans %s' % (lines, args.out), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
