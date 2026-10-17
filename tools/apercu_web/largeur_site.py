"""Debordement des pages du site AlwaysData a une largeur de telephone (P3.28).

Regle du proprietaire (16 oct. 2026) : toute page Web s'affiche sans rien
couper ni depasser sur un ecran de type iPhone (390 px). Ce banc sert
cloud/php-mutualized en local (fichiers statiques, sans PHP ni base : les
appels d'API echouent et chaque page montre son etat "non connecte"), rend
chaque page dans un <iframe> de la largeur voulue (Edge sans fenetre ignore
--window-size pour la mise en page, cf. apercu.py) et mesure :

  - la largeur de la page (scrollWidth) face a la largeur visible ;
  - les elements visibles qui sortent a droite, hors blocs qui defilent ou
    coupent eux-memes (overflow autre que visible).

Les vues connectees de app.html se controlent avec apercu.py (donnees
fictives, debordements signales sur l'image).

Usage :
    python tools/apercu_web/largeur_site.py                 # toutes, 390 px
    python tools/apercu_web/largeur_site.py --largeur 320 maj
Code de sortie : 0 si rien ne deborde, 1 sinon. Rien n'est ecrit dans le depot.
"""
import argparse
import http.server
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib.parse

RACINE = pathlib.Path(__file__).resolve().parents[2]
SITE = RACINE / "cloud" / "php-mutualized"
EDGE = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
]
# Nom -> (page, actions jouees avant la mesure). Les vues connectees de la
# console se jouent sur des reponses fictives (CONSOLE ci-dessous).
CONNEXION = "document.getElementById('tok').value='banc';document.getElementById('btn-login').click();"
PAGES = {
    "accueil": ("/accueil.html", ""),
    "maj": ("/maj.html", ""),
    "admin": ("/admin.html", ""),
    "app": ("/app.html", ""),
    "admin-modules": ("/admin.html", CONNEXION),
    "admin-module": ("/admin.html", CONNEXION + "setTimeout(function(){document.querySelector('.mod').click()},600);"),
    "admin-parametres": ("/admin.html", CONNEXION + "setTimeout(function(){document.getElementById('btn-settings').click()},600);"),
    "admin-comptes": ("/admin.html", CONNEXION + "setTimeout(function(){document.getElementById('btn-users').click()},600);"),
}
# Reponses fictives de la console, textes volontairement longs (pire cas).
CONSOLE = {
    "/admin/modules": [
        {"module_id": "Jardin-01", "firmware": "5.17.12", "last_seen": "2026-10-16 20:00:00",
         "hw_id": "S3-7C9EBDF2A1B0-4F2D", "hw_id_conflict": None},
        {"module_id": "Potager-du-fond-de-jardin", "firmware": "5.17.2", "last_seen": None},
    ],
    "/admin/messages": [{"ts": "2026-10-16 20:00:00", "type": "report",
                         "payload": {"revision": 117, "zones": 5, "uptime": 720, "heap": 175520}}],
    "/admin/commands": [{"issuedAt": "2026-10-16 19:58:00", "state": "pending",
                         "correlationId": "c-8f2a61d0-4b1e-4c55-9d8e-2f3b7a1c9e04",
                         "command": {"type": "apply_schedule", "zone": 3}}],
    "/admin/config": {"ts": "2026-10-16 20:00:00", "payload": {"revision": 117}},
    "/admin/settings": [
        {"name": "MAIL_FROM", "label": "Adresse d'expédition des mails", "value": "aqualook@aqualook.alwaysdata.net",
         "source": "base", "secret": False},
        {"name": "SMTP_PASSWORD", "label": "Mot de passe du serveur de mail", "secret": True,
         "stored": "chiffre", "defined": True, "source": "base"},
        {"name": "ENROLL_TTL", "label": "Durée de validité d'un code d'enrôlement (s)", "value": "900",
         "source": "defaut", "secret": False},
    ],
    "/admin/mail-log": [{"ts": "2026-10-16 18:00:00", "kind": "invitation", "ok": 0,
                         "error": "SMTP 550 adresse refusée par le serveur distant"}],
    "/admin/users": [{"email": "emmanuel.exemple-tres-long@exemple-de-domaine.fr", "label": "Jardin principal",
                      "modules": 2, "last_login": "2026-10-16 20:00:00", "user_id": 1}],
}

MESURE_JS = r"""
try{(new Function(window.__ACTIONS__||''))()}catch(e){}
setTimeout(function(){
  var W=document.documentElement.clientWidth,L=document.documentElement.scrollWidth,trop=[];
  function nom(x){return x.tagName.toLowerCase()+(x.id?'#'+x.id:'')+
    (typeof x.className==='string'&&x.className.trim()?'.'+x.className.trim().split(/\s+/).join('.'):'');}
  document.querySelectorAll('body *').forEach(function(x){
    var r=x.getBoundingClientRect();
    if(!r.width||r.right<=W+1||r.left>=W)return;
    for(var p=x.parentElement;p&&p!==document.body;p=p.parentElement)
      if(getComputedStyle(p).overflowX!=='visible')return;
    trop.push(x);
  });
  var f=trop.filter(function(x){return !trop.some(function(y){return y!==x&&x.contains(y);});});
  fetch('/__result',{method:'POST',body:JSON.stringify({largeur:W,page:L,
    deborde:f.slice(0,6).map(function(x){return nom(x)+' ('+Math.round(x.getBoundingClientRect().right)+' px)';})})});
},3500);
"""

RESULTAT = {}
EVT = threading.Event()
LARGEUR = 390
ACTIONS = ""


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def envoyer(self, code, corps, ctype):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(corps)

    def do_GET(self):
        chemin = urllib.parse.urlsplit(self.path).path
        if chemin == "/__cadre.html":
            page = urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query)["p"][0]
            return self.envoyer(200, ('<!doctype html><body style="margin:0"><iframe src="%s" '
                                      'style="width:%dpx;height:844px;border:0"></iframe>'
                                      % (page, LARGEUR)).encode(), "text/html; charset=utf-8")
        if chemin == "/__mesure.js":
            js = "window.__ACTIONS__=%s;%s" % (json.dumps(ACTIONS), MESURE_JS)
            return self.envoyer(200, js.encode(), "application/javascript")
        if chemin in CONSOLE:
            return self.envoyer(200, json.dumps(CONSOLE[chemin]).encode(), "application/json")
        fichier = (SITE / chemin.lstrip("/")).resolve()
        if fichier.parent != SITE.resolve() or not fichier.is_file() or fichier.suffix == ".php":
            return self.envoyer(404, b"absent (banc statique)", "text/plain")
        corps = fichier.read_bytes()
        if fichier.suffix == ".html":
            corps = corps.replace(b"</body>", b'<script src="/__mesure.js"></script></body>', 1)
        ctype = {".html": "text/html; charset=utf-8", ".css": "text/css",
                 ".js": "application/javascript", ".svg": "image/svg+xml"}.get(
                     fichier.suffix, "application/octet-stream")
        return self.envoyer(200, corps, ctype)

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        corps = self.rfile.read(n)
        if self.path == "/__result":
            RESULTAT.update(json.loads(corps or b"{}"))
            EVT.set()
            return self.envoyer(204, b"", "text/plain")
        return self.envoyer(401, b'{"error":"banc statique"}', "application/json")


def main():
    global LARGEUR, ACTIONS
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pages", nargs="*", help="pages (defaut : toutes) : " + ", ".join(PAGES))
    ap.add_argument("--largeur", type=int, default=390)
    a = ap.parse_args()
    LARGEUR = a.largeur
    edge = next((c for c in EDGE if pathlib.Path(c).exists()), None)
    if not edge:
        sys.exit("Microsoft Edge est introuvable ; ce banc en depend.")
    noms = a.pages or list(PAGES)
    http.server.ThreadingHTTPServer.request_queue_size = 64
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    base = "http://127.0.0.1:%d" % srv.server_address[1]
    echecs = 0
    for nom in noms:
        RESULTAT.clear()
        EVT.clear()
        page, ACTIONS = PAGES[nom]
        profil = tempfile.mkdtemp(prefix="largeur_site_")
        p = subprocess.Popen([edge, "--headless=new", "--disable-gpu", "--no-first-run",
                              "--user-data-dir=" + profil,
                              base + "/__cadre.html?p=" + urllib.parse.quote(page, safe="")],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        fini = EVT.wait(30)
        p.kill()
        p.wait()
        shutil.rmtree(profil, ignore_errors=True)
        if not fini:
            print("%-8s ECHEC : pas de mesure" % nom)
            echecs += 1
            continue
        ok = RESULTAT["page"] <= RESULTAT["largeur"] and not RESULTAT["deborde"]
        echecs += not ok
        print("%-8s %s : page %d px pour %d visibles%s" % (
            nom, "OK" if ok else "DEBORDE", RESULTAT["page"], RESULTAT["largeur"],
            "" if ok else " ; " + ", ".join(RESULTAT["deborde"])))
    srv.shutdown()
    sys.exit(1 if echecs else 0)


if __name__ == "__main__":
    main()
