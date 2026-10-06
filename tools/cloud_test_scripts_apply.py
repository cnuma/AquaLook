"""Banc de config.apply pour les scripts et les phrases (decision D014).

Joue, contre un module reel et le serveur en service, quatre commandes qui
ne changent RIEN au comportement du module :

  1. script avec un code hexadecimal invalide   -> refusee
  2. commande mixte (script + zones)              -> refusee
  3. catalogue de phrases renvoye a l'identique   -> acceptee
  4. script renvoye a l'identique (bytecode, source et entetes relus sur
     le module), avec ~10 Ko de bourrage ignore  -> acceptee ; prouve au
     passage qu'une commande de plus de 4 Ko arrive entiere (plafond 16 Ko)

Les acceptations font monter la revision du module (version unique, D014) :
c'est attendu, et c'est ce que le banc verifie aussi.

Le jeton administrateur est lu dans AQUALOOK_ADMIN_TOKEN, jamais affiche.

Usage (PowerShell) :
  $env:AQUALOOK_ADMIN_TOKEN = '<jeton>'
  python tools/cloud_test_scripts_apply.py --module-ip 192.168.1.141 --slot 3
"""

import argparse
import json
import os
import sys
import time
import urllib.request

SERVEUR = "https://aqualook.alwaysdata.net"


def http(url, token=None, body=None, timeout=15, raw=False):
    data = json.dumps(body).encode("utf-8") if body is not None else None
    req = urllib.request.Request(url, data=data, method="POST" if data else "GET")
    if data:
        req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", "Bearer " + token)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        txt = r.read().decode("utf-8")
        return txt if raw else json.loads(txt)


def revision_a_jour(ip, attente_s=600):
    """Attend que le serveur connaisse la revision courante du module."""
    fin = time.time() + attente_s
    while time.time() < fin:
        cs = http("http://%s/api/adminStatus" % ip)["cloudSync"]
        if cs.get("serverCaughtUp"):
            return cs["lastSyncedRevision"]
        time.sleep(10)
    sys.exit("le serveur n'a pas rattrape la revision du module en %d s" % attente_s)


def jouer(a, token, nom, commande, attendu_etat, attendu_texte):
    commande = dict(commande, type="config.apply",
                    baseRevision=revision_a_jour(a.module_ip))
    taille = len(json.dumps(commande))
    corr = http(a.serveur + "/admin/command", token,
                {"moduleId": a.module_id, "command": commande,
                 "issuedBy": "test:scripts-apply"})["correlationId"]
    print("-- %s : %d octets, base=%d" % (nom, taille, commande["baseRevision"]))
    fin = time.time() + a.attente_min * 60
    while time.time() < fin:
        time.sleep(15)
        cmds = http("%s/admin/commands?moduleId=%s&limit=10"
                    % (a.serveur, a.module_id), token)
        c = next((x for x in cmds if x["correlationId"] == corr), None)
        if c and c["state"] != "pending":
            detail = (c.get("result") or {}).get("detail", "")
            ok = c["state"] == attendu_etat and attendu_texte in detail
            print("   %s / %s -> %s" % (c["state"], detail, "OK" if ok else "INATTENDU"))
            return ok
    print("   toujours en attente apres %d min -> ECHEC" % a.attente_min)
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--module-ip", required=True)
    ap.add_argument("--module-id", default="Jardin-01")
    ap.add_argument("--serveur", default=SERVEUR)
    ap.add_argument("--slot", type=int, default=3,
                    help="emplacement (0-5) renvoye a l'identique ; choisir un script inactif")
    ap.add_argument("--attente-min", type=int, default=12)
    a = ap.parse_args()

    token = os.environ.get("AQUALOOK_ADMIN_TOKEN", "")
    if not token:
        sys.exit("AQUALOOK_ADMIN_TOKEN absent de l'environnement")

    ip = a.module_ip
    one = http("http://%s/api/script-one?i=%d" % (ip, a.slot))
    if not one.get("ok"):
        sys.exit("emplacement %d vide sur le module" % a.slot)
    source = http("http://%s/api/script-source?i=%d" % (ip, a.slot), raw=True)
    phrases = http("http://%s/api/script-messages" % ip)["entries"]
    script = {"i": a.slot, "nom": one["nom"], "actif": one["actif"],
              "declencheur": one["declencheur"], "cible": one["cible"],
              "code": "".join("%02x" % b for b in one["code"]), "source": source}
    print("emplacement %d '%s' (%d octets), %d phrase(s)"
          % (a.slot, one["nom"], len(one["code"]), len(phrases)))

    resultats = [
        jouer(a, token, "hexadecimal invalide",
              {"scripts": [dict(script, code="zz")]}, "refused", "code hexadecimal invalide"),
        jouer(a, token, "commande mixte",
              {"scripts": [script], "zones": [{"i": 0}]}, "refused", "commande-mixte"),
        jouer(a, token, "phrases a l'identique",
              {"phrases": [{"code": p["code"], "texte": p["texte"]} for p in phrases]},
              "accepted", "phrases: %d enregistree" % len(phrases)),
        jouer(a, token, "script a l'identique + bourrage 10 Ko",
              {"scripts": [script], "_bourrage": "x" * 10000},
              "accepted", "script %d enregistre (%d octets)" % (a.slot + 1, len(one["code"]))),
    ]

    apres = http("http://%s/api/script-one?i=%d" % (ip, a.slot))
    identique = all(apres.get(k) == one.get(k)
                    for k in ("nom", "actif", "declencheur", "cible", "code"))
    print("emplacement relu apres coup : %s" % ("identique" if identique else "DIFFERENT"))
    ok = all(resultats) and identique
    print("RESULTAT :", "OK" if ok else "ECHEC")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
