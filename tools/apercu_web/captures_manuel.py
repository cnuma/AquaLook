#!/usr/bin/env python3
"""Captures des pages du module pour le manuel utilisateur (docs/guides/manuel).

Sert les vraies pages de data/ en local, avec des reponses fictives a la place
du module (un jardin d'exemple de 4 zones), puis les photographie avec Edge ou
Chromium sans fenetre. Rien n'est envoye a un module ; aucune ecriture n'est
transmise (les POST repondent {"ok":true}).

Comme apercu.py : le navigateur sans fenetre ignore --window-size pour la
largeur de mise en page, la page est donc rendue dans un <iframe> de la
largeur voulue. Une "scene" peut jouer quelques gestes avant la photo
(ouvrir une fenetre de reglage, le menu des parametres...).

Limite : les donnees fictives suivent le contrat de l'API 5.20.0. Si une
capture sort vide, c'est probablement une reponse absente d'ici, pas un
defaut de la page.

    python tools/apercu_web/captures_manuel.py              # toutes les captures
    python tools/apercu_web/captures_manuel.py accueil-pc   # une seule
    python tools/apercu_web/captures_manuel.py --liste
"""
import argparse
import datetime
import http.server
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib.parse

RACINE = pathlib.Path(__file__).resolve().parents[2]
DATA = RACINE / "data"
SORTIE = RACINE / "docs" / "guides" / "manuel" / "images"

NAVIGATEURS = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    "msedge", "chromium", "chromium-browser", "google-chrome",
    "/opt/pw-browsers/chromium-1194/chrome-linux/chrome",
]

# ── Jardin d'exemple (a Lyon) ────────────────────────────────────
AUJ = datetime.date.today()
EPOCH_AUJ = (AUJ - datetime.date(1970, 1, 1)).days
ESP_AUJ = AUJ.weekday()            # 0 = lundi, comme le module


def creneaux(*actifs):
    """5 creneaux ; actifs = [(h, m, duree), ...]."""
    s = [{"h": h, "m": m, "d": d, "e": True} for h, m, d in actifs]
    while len(s) < 5:
        s.append({"h": 6, "m": 0, "d": 10, "e": False})
    return s


VIDE = creneaux()
ZONES = [
    {"name": "Potager", "color": "#2e9e4f", "mode": 0, "intervalDays": 2,
     "rain": {"threshMm": 6, "hours": 24}, "hasOutput": True,
     "jours": {0: creneaux((7, 0, 15), (19, 30, 10)), 2: creneaux((7, 0, 15)),
               4: creneaux((7, 0, 15), (19, 30, 10)), 6: creneaux((7, 0, 15))}},
    {"name": "Pelouse", "color": "#3a7bd5", "mode": 1, "intervalDays": 3,
     "rain": {"threshMm": 3, "hours": 24}, "hasOutput": True,
     "intervalle": creneaux((5, 30, 20))},
    {"name": "Haies", "color": "#d98c1f", "mode": 0, "intervalDays": 2,
     "rain": {"threshMm": 2, "hours": 24}, "hasOutput": True,
     "jours": {d: creneaux((6, 15, 12)) for d in (1, 3, 5)}},
    {"name": "Pots terrasse", "color": "#b04fc4", "mode": 0, "intervalDays": 2,
     "rain": {"threshMm": 2, "hours": 24}, "hasOutput": False,
     "jours": {d: creneaux((8, 0, 5)) for d in range(7)}},
]
# Pluie annoncee : aujourd'hui 2,4 mm (bloque Haies, seuil 2), puis sec, puis
# une journee pluvieuse.
PREVISIONS = [
    (2.4, 9, 16, 60, "pluie legere", "10d"), (0.0, 7, 18, 5, "ciel degage", "01d"),
    (0.3, 8, 19, 20, "peu nuageux", "02d"), (7.8, 10, 14, 90, "pluie", "09d"),
    (0.0, 6, 17, 10, "ciel degage", "01d"),
]


def forecast():
    return [{"valid": True, "rainMm": r, "tempMin": tmin, "tempMax": tmax,
             "rainProbability": p, "description": desc, "icon": ic,
             "feelsLikeMax": tmax - 1, "humidityMax": 80, "windMaxKmh": 14,
             "gustMaxKmh": 28, "windDeg": 240, "cloudsMax": 60, "pressureAvg": 1016}
            for r, tmin, tmax, p, desc, ic in PREVISIONS]


def zone_slots(i):
    z = ZONES[i]
    jours = [z.get("jours", {}).get(d, VIDE) for d in range(7)]
    return {"daySlots": jours, "intervalSlots": z.get("intervalle", VIDE)}


def zones_config():
    out = []
    for i, z in enumerate(ZONES):
        out.append({"id": i + 1, "name": z["name"], "color": z["color"],
                    "mode": z["mode"], "intervalDays": z["intervalDays"],
                    "intervalAnchorDay": EPOCH_AUJ - 1 if z["mode"] == 1 else 0,
                    "rain": z["rain"], "hasOutput": z["hasOutput"],
                    "notificationMask": 1, "notifyStart": True, "notifyStop": False})
    return {"zones": out}


def status():
    zs = []
    for i, z in enumerate(ZONES):
        actif = (i == 0)
        bloque = (i == 2)
        zs.append({"id": i + 1, "name": z["name"], "color": z["color"],
                   "active": actif, "relayActive": actif, "schedActive": actif,
                   "elapsed": 260 if actif else 0, "remaining": 640 if actif else 0,
                   "mode": z["mode"], "intervalDays": z["intervalDays"],
                   "intervalAnchorDay": EPOCH_AUJ - 1 if z["mode"] == 1 else 0,
                   "rain": z["rain"], "hasOutput": z["hasOutput"],
                   "rainDelayedToday": bloque,
                   "reason": "Manuel 10min" if actif else ("Pluie 2.4mm" if bloque else "Planifie")})
    return {"time": AUJ.isoformat() + " 10:42:00", "synced": True, "uptime": 86400, "heap": 150000,
            "updatePending": False, "configSave": {"pending": False, "failed": False},
            "manualDurationMin": 10, "zones": zs, "forecast": forecast(),
            "weather": {"rainMm": 2.4, "status": "Pluie 2.4mm"}, "productLine": "AquaLook",
            "active": False, "unacknowledged": False, "mask": 0, "labels": {}}


MAINTENANT = int(datetime.datetime.now().timestamp())
# Meme forme que le banc CSP (tools/csp_banc_pages.py), synchro a jour.
CLOUDSYNC = {"enabled": True, "host": "aqualook.alwaysdata.net", "port": 443,
             "moduleId": "Jardin-01", "intervalMinutes": 5, "lastSyncOk": True,
             "resultSinceBoot": True, "fault": False,
             "lastAttemptEpochSec": MAINTENANT - 95, "lastSuccessEpochSec": MAINTENANT - 90,
             "lastSyncedRevision": 121, "syncInProgress": False, "forcePending": False,
             "serverCaughtUp": True}

GET = {
    "/api/status": status,
    "/api/zonesConfig": zones_config,
    "/api/forecast": lambda: {"forecast": forecast()},
    "/api/adminStatus": lambda: {"cloudSync": CLOUDSYNC, "wifi": {"rssi": -58, "ssid": "Maison"},
                                  "owm": {"city": "Lyon", "country": "FR"}},
    "/api/session/state": lambda: {"configure": True, "active": True},
    "/assets-version.json": lambda: {"version": "5.20.0"},
    "/api/diagnostics": lambda: {"build": {"version": "5.20.0", "number": "1718", "gitSha": "24fa2d1"}},
    "/api/health": lambda: {"overall": "normal"},
    "/api/storage": lambda: {"status": "ready", "sdAvailable": True, "webAssetsAvailable": True},
}

# ── Scenes : gestes joues dans la page avant la photo ────────────
# Chaque scene est du JavaScript execute dans la page (servi par le banc,
# donc 'self' : aucune entorse a la CSP). attendre(ms) est fourni.
SCENES = {
    "aucune": "",
    "zone-config": "await attendre(1500); openZoneConfigModal(0);",
    "creneaux": "await attendre(1500); openDayModal(0, 0);",
    "parametres": "await attendre(1500); openDrawer();",
    "parametres-avances": ("await attendre(1500); openDrawer(); "
                           "try { sessionStorage.removeItem('aqualook-avance'); } catch (e) {} "
                           "await adminToggle(); await attendre(300);"),
}

# nom -> (page, largeur, hauteur, scene)
CAPTURES = {
    "accueil-pc": ("/", 1200, 810, "aucune"),
    "accueil-telephone": ("/", 390, 1215, "aucune"),
    "zone-config": ("/", 1200, 800, "zone-config"),
    "creneaux": ("/", 1200, 680, "creneaux"),
    "parametres": ("/", 1200, 560, "parametres"),
    "parametres-avances": ("/", 1200, 690, "parametres-avances"),
}

SCENE_JS = """(async function () {
  function attendre(ms) { return new Promise(function (r) { setTimeout(r, ms); }); }
  try { %s } catch (e) { console.log('scene', e); }
})();"""


class Banc(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def envoyer(self, code, corps, ctype):
        if isinstance(corps, str):
            corps = corps.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(corps)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(corps)

    def do_GET(self):
        u = urllib.parse.urlsplit(self.path)
        q = urllib.parse.parse_qs(u.query)
        chemin = u.path
        if chemin == "/__scene.js":
            nom = q.get("s", ["aucune"])[0]
            return self.envoyer(200, SCENE_JS % SCENES.get(nom, ""), "application/javascript")
        if chemin == "/__cadre.html":
            page, l, h = q["p"][0], int(q["l"][0]), int(q["h"][0])
            src = page + ("&" if "?" in page else "?") + "scene=" + q.get("s", ["aucune"])[0]
            html = ('<!doctype html><meta charset="utf-8"><style>body{margin:0;background:#fff}'
                    'iframe{border:0;display:block}</style>'
                    '<iframe src="%s" width="%d" height="%d"></iframe>' % (src, l, h))
            return self.envoyer(200, html, "text/html; charset=utf-8")
        if chemin == "/api/zone":
            return self.envoyer(200, json.dumps(zone_slots(int(q.get("z", ["0"])[0]))), "application/json")
        if chemin in GET:
            return self.envoyer(200, json.dumps(GET[chemin]()), "application/json")
        if chemin.startswith("/api/"):
            return self.envoyer(200, "{}", "application/json")
        fichier = DATA / ("index.html" if chemin == "/" else chemin.lstrip("/"))
        if not fichier.is_file() or DATA not in fichier.resolve().parents:
            return self.envoyer(404, "absent", "text/plain")
        corps = fichier.read_bytes()
        ctype = {".html": "text/html; charset=utf-8", ".js": "application/javascript",
                 ".css": "text/css", ".json": "application/json", ".svg": "image/svg+xml",
                 ".png": "image/png", ".jpg": "image/jpeg"}.get(fichier.suffix, "application/octet-stream")
        if fichier.suffix == ".html" and "scene" in q:
            balise = '<script src="/__scene.js?s=%s"></script></body>' % q["scene"][0]
            corps = corps.decode("utf-8").replace("</body>", balise, 1).encode("utf-8")
        self.envoyer(200, corps, ctype)

    def do_POST(self):
        self.envoyer(200, '{"ok":true}', "application/json")


# Chromium sans fenetre (--headless=new) retire ~90 px de la hauteur de la
# fenetre pour la zone affichee, mais produit une image de la taille demandee :
# le bas reste blanc et coupe la page. On demande donc une fenetre plus haute,
# puis on recadre a la taille du cadre (Pillow ; sans lui, image non recadree).
MARGE = 200


def recadrer(image, l, h):
    try:
        from PIL import Image
    except ImportError:
        return
    if image.is_file():
        with Image.open(image) as im:
            im.crop((0, 0, l, h)).save(image, optimize=True)


def trouver_navigateur():
    for c in NAVIGATEURS:
        p = shutil.which(c) or (c if pathlib.Path(c).is_file() else None)
        if p:
            return p
    sys.exit("Aucun navigateur Edge ou Chromium trouve.")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("noms", nargs="*", help="captures a produire (toutes par defaut)")
    ap.add_argument("--liste", action="store_true")
    a = ap.parse_args()
    if a.liste:
        for n, (p, l, h, s) in CAPTURES.items():
            print("%-20s %s %dx%d scene=%s" % (n, p, l, h, s))
        return 0
    noms = a.noms or list(CAPTURES)
    inconnus = [n for n in noms if n not in CAPTURES]
    if inconnus:
        sys.exit("Captures inconnues : " + ", ".join(inconnus))

    serveur = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Banc)
    threading.Thread(target=serveur.serve_forever, daemon=True).start()
    base = "http://127.0.0.1:%d" % serveur.server_address[1]
    nav = trouver_navigateur()
    SORTIE.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as profil:
        for n in noms:
            page, l, h, scene = CAPTURES[n]
            url = base + "/__cadre.html?" + urllib.parse.urlencode({"p": page, "l": l, "h": h, "s": scene})
            image = SORTIE / (n + ".png")
            # Chromium refuse de demarrer sous root sans --no-sandbox
            # (conteneur Linux) ; sur le poste Windows, le bac a sable reste.
            bac = ["--no-sandbox"] if hasattr(os, "geteuid") and os.geteuid() == 0 else []
            subprocess.run([nav, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                            "--no-first-run", "--user-data-dir=" + profil] + bac + [
                            "--virtual-time-budget=8000", "--window-size=%d,%d" % (l, h + MARGE),
                            "--screenshot=" + str(image), url],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=90, check=False)
            recadrer(image, l, h)
            print(("ok   " if image.is_file() else "ECHEC") + " " + str(image.relative_to(RACINE)))
    serveur.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
