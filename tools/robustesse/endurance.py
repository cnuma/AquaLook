"""Test d'endurance doux : cherche une fuite memoire lente sur la nuit.

Sequentiel et lent (une requete toutes les 2 s) -- deliberement sous le seuil
qui affame la tache async_tcp, pour eprouver la duree et non la charge. Chaque
route sollicite un chemin d'allocation different (JSON construit, lecture SD,
scan...). Un heap qui descend regulierement sur des milliers de requetes
trahirait une fuite ; le veilleur en parallele enregistre le plancher.

N'ecrit rien sur le module : que des GET.
"""
import time, json, urllib.request, datetime, sys

BASE = "http://192.168.1.141"
CHEMIN = sys.argv[1] if len(sys.argv) > 1 else "endurance.log"
DUREE = int(sys.argv[2]) if len(sys.argv) > 2 else 21600   # 6 h

ROUTES = [
    "/api/status", "/api/diagnostics", "/api/adminStatus",
    "/api/zone?z=0", "/api/zone?z=1", "/api/zone?z=2", "/api/zone?z=3",
    "/api/display", "/api/notifications", "/api/logs.txt",
    "/api/debug/heap-info", "/api/debug/nvs-stats", "/",
]


def heap():
    try:
        with urllib.request.urlopen(BASE + "/api/diagnostics", timeout=8) as r:
            m = json.loads(r.read())["memory"]
            return m["heapFree"], m["heapLargestBlock"]
    except Exception:
        return None, None


def note(f, msg):
    ts = datetime.datetime.now().strftime("%H:%M:%S")
    f.write("%s %s\n" % (ts, msg)); f.flush()


fin = time.time() + DUREE
n = 0
ok = ko = 0
prochaine_mesure = 0
h0, b0 = heap()

with open(CHEMIN, "a", encoding="utf-8") as f:
    note(f, "== endurance demarree, heap initial=%s bloc=%s ==" % (h0, b0))
    while time.time() < fin:
        route = ROUTES[n % len(ROUTES)]
        n += 1
        try:
            with urllib.request.urlopen(BASE + route, timeout=8) as r:
                r.read(); ok += 1
        except Exception:
            ko += 1
        # Point memoire toutes les 60 s.
        if time.time() >= prochaine_mesure:
            prochaine_mesure = time.time() + 60
            h, b = heap()
            delta = (h - h0) if (h is not None and h0 is not None) else None
            note(f, "n=%d ok=%d ko=%d heap=%s bloc=%s delta_vs_debut=%s"
                 % (n, ok, ko, h, b, delta))
        time.sleep(2)
    h, b = heap()
    note(f, "== endurance terminee : n=%d ok=%d ko=%d heap_final=%s (debut=%s) =="
         % (n, ok, ko, h, h0))
