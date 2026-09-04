"""Veilleur de sante du module, par HTTP -- le port serie n'est pas disponible
cette nuit (USB debranche, banc sur 5 V externe).

Echantillonne /api/diagnostics a intervalle regulier et n'ecrit une ligne que
quand quelque chose bouge : un redemarrage (uptime qui retombe), un changement
de resetReason, un nouveau plancher de heap, ou une injoignabilite. Le silence
vaut donc bonne sante, et chaque ligne est un evenement a expliquer.

Sert de temoin independant pendant que les scripts d'attaque tournent : c'est
lui qui distingue "le module a encaisse" de "le module est parti en vrille".
"""
import json, sys, time, datetime, urllib.request

CHEMIN = sys.argv[1] if len(sys.argv) > 1 else "health.log"
DUREE = int(sys.argv[2]) if len(sys.argv) > 2 else 28800
PERIODE = float(sys.argv[3]) if len(sys.argv) > 3 else 5.0
URL = "http://192.168.1.141/api/diagnostics"


def note(f, msg):
    ts = datetime.datetime.now().strftime("%H:%M:%S")
    f.write("%s %s\n" % (ts, msg))
    f.flush()


fin = time.time() + DUREE
uptime_max = -1
reset_prec = None
heap_min = 10 ** 9
injoignable_depuis = None
echantillons = 0

with open(CHEMIN, "a", encoding="utf-8") as f:
    note(f, "===== veilleur demarre, periode %ss =====" % PERIODE)
    while time.time() < fin:
        t0 = time.time()
        try:
            with urllib.request.urlopen(URL, timeout=8) as r:
                d = json.loads(r.read())
            echantillons += 1
            s = d["system"]; m = d["memory"]
            up = s["uptimeSec"]; reset = s["resetReason"]
            heap = m["heapFree"]; bloc = m["heapLargestBlock"]

            if injoignable_depuis is not None:
                duree = int(time.time() - injoignable_depuis)
                note(f, "REJOIGNABLE apres %ss d'absence (uptime=%ss reset=%s)"
                     % (duree, up, reset))
                injoignable_depuis = None

            # Redemarrage : l'uptime a recule.
            if uptime_max >= 0 and up < uptime_max - 3:
                note(f, "!!! REDEMARRAGE detecte : uptime %ss -> %ss, reset=%s heap=%s"
                     % (uptime_max, up, reset, heap))
                heap_min = 10 ** 9   # nouveau cycle de vie
            uptime_max = up

            if reset != reset_prec:
                note(f, "resetReason = %s (uptime=%ss)" % (reset, up))
                reset_prec = reset

            # Nouveau plancher de heap : signal de fuite si ca ne remonte jamais.
            if heap < heap_min:
                heap_min = heap
                note(f, "nouveau plancher heap=%s bloc_max=%s (uptime=%ss)"
                     % (heap, bloc, up))
        except Exception as e:
            if injoignable_depuis is None:
                injoignable_depuis = time.time()
                note(f, "INJOIGNABLE : %s" % e)
        dt = time.time() - t0
        time.sleep(max(0.0, PERIODE - dt))
    note(f, "===== veilleur termine, %d echantillons, heap_min=%s ====="
         % (echantillons, heap_min))
