#!/usr/bin/env python3
"""Banc CSP des pages du module (P3.11) : sans script inline, tout marche-t-il ?

Sert data/ en local sous la CSP visee (script-src 'self', rapport des
violations), ouvre chaque page dans Edge sans affichage et deroule un
scenario (lectures attendues, clics, saisies). Un script de banc injecte
(/__banc.js, servi par le banc, donc 'self') joue le scenario et renvoie
le resultat.

  - lectures GET : relayees vers le module (--module, .141 par defaut), sauf
    celles que le scenario simule (ex. /api/wifi/scan : pas de scan reel) ;
  - ECRITURES (tout ce qui n'est pas GET/HEAD) : JAMAIS transmises. Le banc
    les enregistre et repond {"ok":true} : un bouton est donc "relie" quand
    son POST attendu apparait dans le journal du banc ;
  - temoin negatif : une page avec un <script> inline et un onclick doit
    produire des violations CSP et ne rien executer.

    python tools/csp_banc_pages.py                 # toutes les pages
    python tools/csp_banc_pages.py sante logs -v   # quelques pages, detail
    python tools/csp_banc_pages.py --module ""     # sans module : GET -> 502

Code de sortie : 0 si tout passe, 1 sinon. Rien n'est ecrit dans le depot.
"""

import argparse
import http.server
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(RACINE, "data")
CSP = ("default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
       "img-src 'self' data:; connect-src 'self'; report-uri /__csp")

EDGE_CANDIDATS = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    "msedge", "microsoft-edge", "chromium", "google-chrome",
]

# Chemins "hybrides" servis par le module depuis la SD (SdStaticHandler).
ALIAS = {"/setup": "setup.html", "/logs": "logs.html"}

# Scenario : liste d'etapes jouees dans l'ordre par /__banc.js.
#   ["attendre", ms]
#   ["texte", selecteur, "non"|"egal"|"contient", valeur]   -> verification
#   ["compte", selecteur, minimum]                           -> verification
#   ["clic", selecteur, force]   force=1 : reactive un bouton desactive
#   ["saisir", selecteur, valeur, evenement]
#   ["cocher", selecteur, 0|1]   (declenche change)
#   ["classe", selecteur, "a"|"pas", classe]                -> verification
#   ["style", selecteur, propriete, contient]                -> verification
#   ["bascule", selecteur_clic, selecteur_cible, classe]     -> le clic inverse la classe
# "simule" : une valeur None repond 404 (ex. image absente -> onerror).
# "post" : ecritures qui DOIVENT avoir ete demandees (methode, debut d'URL).
# "simule" : reponses GET locales (chemin -> objet JSON) au lieu du module.
# Resultat enregistre par .141 apres l'installation 5.17.1 -> 5.17.2 (14 oct. 2026).
OTA_INSTALLE = {"valid": True, "command": "install_update", "success": True,
                "detail": "activated-app1-pending-validation", "updateAvailable": False,
                "installedVersion": "5.17.1", "availableVersion": "5.17.2",
                "webAssetsInstalledVersion": "5.17.3", "webAssetsAvailableVersion": "5.17.3",
                "recordedEpoch": 1791997071, "target": "s3", "firmwareUrl": "x", "firmwareSize": 1575296,
                "sha256": "65ac", "calculatedSha256": "65ac"}
SCENARIOS = {
    "sante": {
        "page": "/sante.html",
        "simule": {"/api/health": {"overall": "degrade", "bootLoop": {"suspectBoots": 4, "threshold": 4},
                                   "storageSd": {"state": "none"}, "notifications": {"enabled": True},
                                   "faults": [{"label": "Defaut de banc"}], "unacknowledged": True}},
        "etapes": [["attendre", 2500],
                   ["texte", "#overall", "egal", "DEGRADE"],
                   ["texte", "#ap-version", "non", "\u2026"],
                   ["texte", "#ap-version", "non", "injoignable"],
                   ["compte", "#actions button[data-go=lever]", 1],
                   ["clic", "#actions button[data-go=lever]", 0],
                   ["attendre", 800],
                   ["texte", "#msg", "contient", "Mode degrade leve"]],
        "post": [["POST", "/api/bootguard/clear"]],
    },
    "sante-acquitter": {
        "page": "/sante.html",
        "simule": {"/api/health": {"overall": "normal", "storageSd": {"state": "recovered-unacknowledged"},
                                   "unacknowledged": True, "faults": []}},
        "etapes": [["attendre", 2000],
                   ["compte", "#actions button[data-go=acquitter][data-sd='1']", 1],
                   ["clic", "#actions button[data-go=acquitter]", 0],
                   ["attendre", 800],
                   ["texte", "#msg", "egal", "Acquitte."]],
        "post": [["POST", "/api/logs/ack"], ["POST", "/api/incidents/storage-sd/ack"]],
    },
    "diagnostic": {
        "page": "/diagnostic.html",
        "etapes": [["attendre", 2500],
                   ["texte", "#s", "contient", "Uptime"],
                   ["texte", "#m", "contient", "Heap libre"],
                   ["clic", "#q", 0],
                   ["texte", "#q", "egal", "Reprendre"],
                   ["clic", "#q", 0],
                   ["texte", "#q", "egal", "Pause"]],
        "post": [],
    },
    "setup": {
        "page": "/setup",
        "simule": {"/api/wifi/scan": {"scanning": False, "networks": [
            {"ssid": "Banc-A", "rssi": -50, "secured": True},
            {"ssid": "Banc-B", "rssi": -70, "secured": False}]}},
        "etapes": [["attendre", 1500],
                   ["compte", "#networks option", 3],
                   ["texte", "#msg", "contient", "2 r\u00e9seau"],
                   ["saisir", "#networks", "Banc-B", "change"],
                   ["texte", "#ssid", "egal", "Banc-B"],
                   ["saisir", "#pwd", "motdepasse-de-banc", "input"],
                   ["clic", "#save", 0],
                   ["attendre", 800],
                   ["texte", "#msg", "contient", "Enregistr"]],
        "post": [["POST", "/api/wifi"]],
    },
    "logs": {
        "page": "/logs",
        "etapes": [["attendre", 3000],
                   ["texte", "#journal-state", "contient", "Actualis"],
                   ["texte", "#incident", "contient", "tat"],
                   ["clic", "#journal-refresh", 0],
                   ["cocher", "#timing-logs", 1],
                   ["clic", "#ack-btn", 1],
                   ["clic", "#incident-ack", 1],
                   ["attendre", 1200]],
        "post": [["POST", "/api/logConfig"], ["POST", "/api/logs/ack"],
                 ["POST", "/api/incidents/storage-sd/ack"]],
    },
    "index": {
        "page": "/index.html",
        "simule": {"/logo.png": None},
        "etapes": [["attendre", 5000],
                   ["style", "img[alt=AquaLook]", "display", "none"],
                   ["compte", "[data-onclick^=toggleManual]", 1],
                   ["clic", "[data-onclick='toggleZonesView()']", 0],
                   ["attendre", 300],
                   ["compte", "tr[data-onclick^=openZoneConfigModal]", 1],
                   ["clic", "td.zt-action .btn-run:not([disabled])", 0],
                   ["attendre", 400],
                   ["classe", "#modal", "pas", "open"],
                   ["clic", "tr[data-onclick^=openZoneConfigModal] td.zt-mode", 0],
                   ["attendre", 300],
                   ["classe", "#modal", "a", "open"],
                   ["saisir", "#zcfg-color", "#123456", "input"],
                   ["style", "#zcfg-dot", "background", "rgb(18, 52, 86)"],
                   ["clic", "#modal", 0],
                   ["classe", "#modal", "pas", "open"],
                   ["clic", "[data-onclick='toggleZonesView()']", 0],
                   ["bascule", "[data-onclick=\"toggleSection('sec-zones')\"]", "#sec-zones", "open"],
                   ["clic", "[data-onclick='openDrawer()']", 0],
                   ["classe", "#drawer", "a", "open"],
                   ["compte", "#drawer [data-onclick^=openCfgPage]", 1],
                   ["clic", "[data-onclick='closeDrawer()']", 0],
                   ["classe", "#drawer", "pas", "open"],
                   ["saisir", "[data-onchange='onDispNumericChange()']", "7", "change"],
                   ["clic", "[data-onclick='saveCfgRelaySetup(),saveCfgDisplay()']", 1],
                   ["attendre", 1500]],
        "post": [["POST", "/api/manual"], ["POST", "/api/display"], ["POST", "/api/system"]],
    },
    "cloudsync": {
        "page": "/cloudsync.html",
        "etapes": [["attendre", 2500],
                   ["texte", "#enabled", "non", "\u2026"],
                   ["texte", "#host", "non", "\u2026"],
                   ["clic", "#syncNow", 1],
                   ["attendre", 1200]],
        "post": [["POST", "/api/cloudSyncNow"]],
    },
    # P3.17 : apres une installation reussie, le resultat enregistre porte la
    # version d'AVANT le redemarrage ; la page doit afficher celle en marche.
    "ota": {
        "page": "/ota.html",
        "simule": {"/api/maintenance/last-result": OTA_INSTALLE,
                   "/api/diagnostics": {"build": {"version": "5.17.2"}}},
        "etapes": [["attendre", 2500],
                   ["texte", ".chan .chip", "egal", "installe"],
                   ["texte", "#chans", "contient", "installee : 5.17.2"]],
        "post": [],
    },
    "ota-repli": {
        "page": "/ota.html",
        "simule": {"/api/maintenance/last-result": OTA_INSTALLE, "/api/diagnostics": None},
        "etapes": [["attendre", 2500],
                   ["texte", ".chan .chip", "egal", "non verifie"],
                   ["texte", "#chans", "contient", "installee : 5.17.1"]],
        "post": [],
    },
    "ota-dispo": {
        "page": "/ota.html",
        "simule": {"/api/maintenance/last-result": dict(OTA_INSTALLE, command="check_version", updateAvailable=True,
                                                         detail="update-available"),
                   "/api/diagnostics": {"build": {"version": "5.17.1"}}},
        "etapes": [["attendre", 2500],
                   ["texte", ".chan .chip", "egal", "a installer"],
                   ["texte", "#chans", "contient", "disponible : 5.17.2"]],
        "post": [],
    },
}

BANC_JS = r"""
(function(){
  window.alert=function(){};window.confirm=function(){return true};window.prompt=function(){return ''};
  const S=window.__SCEN__,res=[];
  const ok=(c,m)=>res.push((c?'PASS ':'FAIL ')+m);
  const q=s=>document.querySelector(s);
  const pause=ms=>new Promise(r=>setTimeout(r,ms));
  async function jouer(){
    for(const e of S){
      const t=e[0];
      try{
        if(t==='attendre'){await pause(e[1]);continue}
        const el=q(e[1]);
        if(t==='compte'){const n=document.querySelectorAll(e[1]).length;ok(n>=e[2],'compte '+e[1]+' = '+n+' (>= '+e[2]+')');continue}
        if(!el){ok(false,t+' '+e[1]+' : element absent');continue}
        const v=(el.value!==undefined&&el.tagName!=='BUTTON'?el.value:el.textContent).trim();
        if(t==='texte'){
          const c=e[2]==='egal'?v===e[3]:e[2]==='contient'?v.indexOf(e[3])>=0:v!==e[3]&&v!=='';
          ok(c,'texte '+e[1]+' '+e[2]+' "'+e[3]+'" (lu : "'+v.slice(0,60)+'")');
        }else if(t==='clic'){if(e[2])el.disabled=false;el.style.display=el.style.display==='none'?'':el.style.display;el.click();ok(true,'clic '+e[1]);await pause(150)}
        else if(t==='saisir'){el.value=e[2];el.dispatchEvent(new Event(e[3],{bubbles:true}));ok(true,'saisie '+e[1])}
        else if(t==='classe'){const c=el.classList.contains(e[3]);ok(e[2]==='a'?c:!c,'classe '+e[1]+' '+e[2]+' '+e[3])}
        else if(t==='style'){const s=String(el.style[e[2]]||'');ok(s.indexOf(e[3])>=0,'style '+e[1]+' '+e[2]+' contient "'+e[3]+'" (lu : "'+s+'")')}
        else if(t==='bascule'){const c=q(e[2]);if(!c){ok(false,'bascule '+e[2]+' absent');continue}const av=c.classList.contains(e[3]);el.click();await pause(150);ok(c.classList.contains(e[3])!==av,'bascule '+e[2]+' '+e[3]+' via '+e[1])}
        else if(t==='cocher'){el.checked=!!e[2];el.dispatchEvent(new Event('change',{bubbles:true}));ok(true,'coche '+e[1])}
      }catch(x){ok(false,t+' '+e[1]+' : exception '+x.message)}
    }
    await fetch('/__result',{method:'POST',body:JSON.stringify(res)});
  }
  if(document.readyState==='complete')jouer();else window.addEventListener('load',jouer);
})();
"""

TEMOIN = ("<!doctype html><html><body><p id=t>avant</p>"
          "<script>document.getElementById('t').textContent='INLINE EXECUTE'</script>"
          "<button id=b onclick=\"document.getElementById('t').textContent='ONCLICK EXECUTE'\">b</button>"
          "<script src=/__temoin.js></script></body></html>")
TEMOIN_JS = ("setTimeout(function(){document.getElementById('b').click();setTimeout(function(){"
             "fetch('/__result',{method:'POST',body:JSON.stringify(["
             "document.getElementById('t').textContent==='avant'?'PASS temoin : inline et onclick non executes'"
             ":'FAIL temoin : '+document.getElementById('t').textContent])})},300)},300);")


class Etat:
    def __init__(self):
        self.reset(None)

    def reset(self, scen):
        self.scen = scen
        self.ecritures = []
        self.violations = []
        self.resultat = None
        self.evt = threading.Event()


ETAT = Etat()
MODULE = "192.168.1.141"


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def envoyer(self, code, corps, ctype, csp=False):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        if csp:
            self.send_header("Content-Security-Policy", CSP)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(corps)

    def lire_corps(self):
        n = int(self.headers.get("Content-Length") or 0)
        return self.rfile.read(n) if n else b""

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        chemin = self.path.split("?")[0]
        sc = ETAT.scen or {}
        if chemin == "/__banc.js":
            js = "window.__SCEN__=%s;%s" % (json.dumps(sc.get("etapes", [])), BANC_JS)
            return self.envoyer(200, js.encode(), "application/javascript")
        if chemin == "/__temoin.html":
            return self.envoyer(200, TEMOIN.encode(), "text/html; charset=utf-8", csp=True)
        if chemin == "/__temoin.js":
            return self.envoyer(200, TEMOIN_JS.encode(), "application/javascript")
        if chemin in sc.get("simule", {}):
            if sc["simule"][chemin] is None:
                return self.envoyer(404, b"absent (banc)", "text/plain")
            return self.envoyer(200, json.dumps(sc["simule"][chemin]).encode(), "application/json")
        nom = ALIAS.get(chemin, chemin.lstrip("/"))
        local = os.path.join(DATA, nom)
        if nom and os.path.isfile(local) and os.path.dirname(os.path.abspath(local)) == DATA:
            with open(local, "rb") as f:
                corps = f.read()
            if nom.endswith(".html"):
                corps = corps.replace(b"</body>", b'<script src="/__banc.js"></script></body>', 1)
                return self.envoyer(200, corps, "text/html; charset=utf-8", csp=True)
            ctype = {"js": "application/javascript", "css": "text/css", "json": "application/json"}.get(
                nom.rsplit(".", 1)[-1], "application/octet-stream")
            return self.envoyer(200, corps, ctype)
        if not MODULE:
            return self.envoyer(502, b"pas de module", "text/plain")
        try:
            with urllib.request.urlopen("http://%s%s" % (MODULE, self.path), timeout=8) as r:
                return self.envoyer(r.status, r.read(), r.headers.get("Content-Type", "text/plain"))
        except urllib.error.HTTPError as e:
            return self.envoyer(e.code, e.read(), "text/plain")
        except Exception as e:  # noqa: BLE001
            return self.envoyer(502, str(e).encode(), "text/plain")

    def do_POST(self):
        chemin = self.path.split("?")[0]
        corps = self.lire_corps()
        if chemin == "/__csp":
            try:
                ETAT.violations.append(json.loads(corps or b"{}").get("csp-report", {}))
            except ValueError:
                ETAT.violations.append({"brut": corps[:200].decode("utf-8", "replace")})
            return self.envoyer(204, b"", "text/plain")
        if chemin == "/__result":
            ETAT.resultat = json.loads(corps or b"[]")
            ETAT.evt.set()
            return self.envoyer(204, b"", "text/plain")
        # Ecriture de la page : enregistree, JAMAIS transmise au module.
        ETAT.ecritures.append((self.command, self.path))
        return self.envoyer(200, b'{"ok":true}', "application/json")

    do_PUT = do_POST
    do_DELETE = do_POST


def trouver_edge(explicite):
    for c in ([explicite] if explicite else EDGE_CANDIDATS):
        if os.path.isfile(c):
            return c
        t = shutil.which(c)
        if t:
            return t
    return None


def ouvrir(edge, url, delai):
    profil = tempfile.mkdtemp(prefix="csp_banc_")
    p = subprocess.Popen([edge, "--headless=new", "--disable-gpu", "--no-first-run",
                          "--user-data-dir=" + profil, url],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    fini = ETAT.evt.wait(delai)
    time.sleep(0.5)  # laisse arriver les derniers rapports CSP
    p.kill()
    p.wait()
    shutil.rmtree(profil, ignore_errors=True)
    return fini


def main():
    global MODULE
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pages", nargs="*", help="scenarios (defaut : tous)")
    ap.add_argument("--module", default=MODULE, help="adresse du module pour les lectures GET")
    ap.add_argument("--edge", help="chemin du navigateur")
    ap.add_argument("-v", action="store_true", help="toutes les lignes")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    MODULE = a.module
    edge = trouver_edge(a.edge)
    if not edge:
        sys.exit("Edge introuvable (--edge)")
    noms = a.pages or list(SCENARIOS)
    inconnus = [n for n in noms if n not in SCENARIOS]
    if inconnus:
        sys.exit("scenario inconnu : %s" % ", ".join(inconnus))

    # File d'attente d'ecoute : 5 par defaut ; Edge ouvre plus de connexions
    # simultanees et Windows refuse le surplus (script charge avec statut 0).
    http.server.ThreadingHTTPServer.request_queue_size = 64
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    base = "http://127.0.0.1:%d" % srv.server_address[1]
    total_ko = 0

    ETAT.reset({})
    ouvrir(edge, base + "/__temoin.html", 15)
    lignes = list(ETAT.resultat or ["FAIL temoin : pas de resultat"])
    lignes.append(("PASS" if len(ETAT.violations) >= 2 else "FAIL")
                  + " temoin : %d violation(s) CSP signalee(s) (>= 2)" % len(ETAT.violations))
    ko = sum(1 for l in lignes if l.startswith("FAIL"))
    total_ko += ko
    print("%-16s %s" % ("temoin negatif", "OK" if not ko else "%d ECHEC(S)" % ko))
    for l in lignes:
        if a.v or l.startswith("FAIL"):
            print("    " + l)

    for nom in noms:
        sc = SCENARIOS[nom]
        ETAT.reset(sc)
        fini = ouvrir(edge, base + sc["page"], 40)
        lignes = list(ETAT.resultat or [])
        if not fini:
            lignes.append("FAIL scenario non termine (script de page bloque ?)")
        for v in ETAT.violations:
            # Seules les violations de script echouent (objet de P3.11) ; les
            # autres (police externe de style-base.css...) sont des ecarts
            # signales a part, a traiter hors de ce banc.
            dire = v.get("violated-directive", "?")
            lignes.append("%s violation CSP : %s %s" % ("FAIL" if dire.startswith("script-src") else "ECART",
                                                         dire, v.get("source-file") or v.get("blocked-uri") or v))
        for meth, debut in sc.get("post", []):
            vu = any(m == meth and p.startswith(debut) for m, p in ETAT.ecritures)
            lignes.append(("PASS" if vu else "FAIL") + " ecriture demandee %s %s" % (meth, debut))
        for m, p in ETAT.ecritures:
            lignes.append("INFO ecriture interceptee (non transmise) : %s %s" % (m, p))
        ko = sum(1 for l in lignes if l.startswith("FAIL"))
        npass = sum(1 for l in lignes if l.startswith("PASS"))
        total_ko += ko
        print("%-16s %s" % (nom, "%d/%d OK" % (npass, npass + ko) if not ko else "%d ECHEC(S)" % ko))
        for l in lignes:
            if a.v or not l.startswith(("PASS", "INFO")):
                print("    " + l)

    srv.shutdown()
    print("\nResultat : %s" % ("tout passe" if not total_ko else "%d echec(s)" % total_ko))
    sys.exit(1 if total_ko else 0)


if __name__ == "__main__":
    main()
