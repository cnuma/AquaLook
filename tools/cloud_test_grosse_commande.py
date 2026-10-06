"""Verifie qu'une commande config.apply de plus de 4 Ko atteint le module.

Contexte : jusqu'au 6 octobre 2026, CloudSync tronquait la reponse de
/v1/pending-command a 4 096 octets ; un JSON tronque ne s'analysait pas et
la commande restait en attente pour toujours, sans message. Le plafond est
passe a 16 Ko (decision D014, docs/codex/02_DECISIONS.md).

Ce que fait le test, sans rien modifier sur le module :
  1. lit la revision courante sur le module (API locale, sans secret) ;
  2. depose une commande config.apply portant baseRevision juste et un
     bourrage de N octets, SANS aucun champ applicable ;
  3. attend son accuse.

Resultat attendu : etat "refused", motif "aucun champ applicable dans la
commande" -> le module a recu et analyse la commande complete.
Commande toujours "pending" apres plusieurs cycles -> tronquee.

Le jeton administrateur est lu dans la variable d'environnement
AQUALOOK_ADMIN_TOKEN et n'est jamais affiche.

Usage (PowerShell) :
  $env:AQUALOOK_ADMIN_TOKEN = '<jeton>'
  python tools/cloud_test_grosse_commande.py --module-ip 192.168.1.141
"""

import argparse
import json
import os
import sys
import time
import urllib.request

SERVEUR = "https://aqualook.alwaysdata.net"


def http_json(url, token=None, body=None, timeout=15):
    data = json.dumps(body).encode("utf-8") if body is not None else None
    req = urllib.request.Request(url, data=data, method="POST" if data else "GET")
    if data:
        req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", "Bearer " + token)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8"))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--module-ip", required=True)
    ap.add_argument("--module-id", default="Jardin-01")
    ap.add_argument("--serveur", default=SERVEUR)
    ap.add_argument("--octets", type=int, default=12000,
                    help="taille du bourrage (defaut 12000, entre 4 Ko et 16 Ko)")
    ap.add_argument("--attente-min", type=int, default=12)
    args = ap.parse_args()

    token = os.environ.get("AQUALOOK_ADMIN_TOKEN", "")
    if not token:
        sys.exit("AQUALOOK_ADMIN_TOKEN absent de l'environnement")

    statut = http_json("http://%s/api/adminStatus" % args.module_ip)
    cs = statut.get("cloudSync", {})
    if not cs.get("serverCaughtUp"):
        sys.exit("le serveur n'est pas a jour de la revision du module : "
                 "attendre une synchronisation et relancer")
    revision = cs["lastSyncedRevision"]

    commande = {"type": "config.apply", "baseRevision": revision,
                "_bourrage": "x" * args.octets}
    taille = len(json.dumps(commande))
    rep = http_json(args.serveur + "/admin/command", token,
                    {"moduleId": args.module_id, "command": commande,
                     "issuedBy": "test:grosse-commande"})
    corr = rep["correlationId"]
    print("commande deposee : %s, %d octets, baseRevision=%d" % (corr, taille, revision))

    fin = time.time() + args.attente_min * 60
    while time.time() < fin:
        time.sleep(20)
        cmds = http_json("%s/admin/commands?moduleId=%s&limit=10"
                         % (args.serveur, args.module_id), token)
        c = next((x for x in cmds if x["correlationId"] == corr), None)
        etat = c["state"] if c else "?"
        print(time.strftime("%H:%M:%S"), etat)
        if etat not in ("pending", "?"):
            print("accuse :", json.dumps(c.get("result"), ensure_ascii=False))
            ok = etat == "refused" and "aucun champ" in json.dumps(c.get("result"))
            print("RESULTAT :", "OK, commande de %d octets recue et analysee" % taille
                  if ok else "INATTENDU, a examiner")
            return 0 if ok else 1
    print("RESULTAT : ECHEC, toujours en attente apres %d min (troncature ?)"
          % args.attente_min)
    return 1


if __name__ == "__main__":
    sys.exit(main())
