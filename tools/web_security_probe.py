#!/usr/bin/env python3
"""
Harnais de verification de securite du service Web AquaLook (AlwaysData).

A LANCER PAR LE PROPRIETAIRE, DEPUIS SA MACHINE. C'est un test de securite
AUTORISE de ta propre infrastructure. Il est volontairement :

  - NON DESTRUCTIF : aucune ecriture, aucune suppression, aucun flash.
  - A FAIBLE DEBIT : liste fixe de requetes, espacees (--delay, 0.7 s par
    defaut) ; ce n'est pas un scanner ni un fuzzer. Hebergement mutualise :
    ne pas transformer ce script en boucle agressive.
  - CIBLE : verifie des proprietes precises reperees a l'audit statique du
    10 oct. 2026 (docs/security/AUDIT_WEB_2026-10-10.md).

Il sert AUSSI de verification apres deploiement du correctif
`fix/cloud-securite-entetes-logs` (en-tetes de securite + blocage *.log).

Usage :
    python tools/web_security_probe.py
    python tools/web_security_probe.py --base https://aqualook.alwaysdata.net
    python tools/web_security_probe.py --delay 1.0
    python tools/web_security_probe.py --include-ratelimit   # voir plus bas

Sortie : un tableau PASS/ATTENTION/INFO par verification. Colle-la pour analyse.
Aucune dependance externe (urllib standard).
"""

import argparse
import json
import ssl
import sys
import time
import urllib.error
import urllib.request

DEFAULT_BASE = "https://aqualook.alwaysdata.net"

# Entetes de securite attendus apres deploiement du correctif.
SECURITY_HEADERS = [
    "content-security-policy",
    "x-frame-options",
    "x-content-type-options",
    "referrer-policy",
    "strict-transport-security",
]

# Fichiers qui ne doivent JAMAIS etre servis en clair (deny-list .htaccess +
# cas latent *.log + depot .git). Attendu : pas de 200 revelant du contenu.
MUST_NOT_SERVE = [
    "/.env", "/.env.example", "/schema.sql", "/schema-v7-liens-compte.sql",
    "/router.php", "/cleanup.php", "/README.md", "/php.log", "/error.log",
    "/.git/config", "/.gitignore",
]

# Routes protegees : sans session ni jeton, attendu 401 (ou 404), jamais 200.
PROTECTED = [
    ("GET", "/admin/modules"),
    ("GET", "/app/me"),
    ("GET", "/app/modules"),
    ("GET", "/v1/pending-command"),
]


def req(base, method, path, headers=None, body=None, timeout=12):
    """Une requete. Rend (status, headers_dict_lowercase, texte_debut)."""
    url = base.rstrip("/") + path
    data = body.encode("utf-8") if isinstance(body, str) else body
    r = urllib.request.Request(url, data=data, method=method, headers=headers or {})
    ctx = ssl.create_default_context()
    try:
        with urllib.request.urlopen(r, timeout=timeout, context=ctx) as resp:
            raw = resp.read(2048).decode("utf-8", "replace")
            return resp.status, {k.lower(): v for k, v in resp.headers.items()}, raw
    except urllib.error.HTTPError as e:
        raw = e.read(2048).decode("utf-8", "replace") if e.fp else ""
        return e.code, {k.lower(): v for k, v in (e.headers or {}).items()}, raw
    except Exception as e:  # noqa: BLE001 -- on veut juste rapporter l'echec
        return None, {}, "ERREUR: " + type(e).__name__ + " " + str(e)


def line(verdict, label, detail=""):
    print(f"  [{verdict:^9}] {label}" + (f" -- {detail}" if detail else ""))


def looks_like_stacktrace(text):
    bas = text.lower()
    return any(t in bas for t in ("fatal error", "stack trace", "/home/", "<br />", "on line"))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--base", default=DEFAULT_BASE)
    ap.add_argument("--delay", type=float, default=0.7,
                    help="secondes entre requetes (faible debit)")
    ap.add_argument("--include-ratelimit", action="store_true",
                    help="teste le plafond anti-force-brute : 11 connexions en "
                         "echec sur une adresse BIDON (ne verrouille aucun vrai "
                         "compte ; l'IP testeuse peut recevoir des 429 pendant "
                         "15 min). Opt-in.")
    args = ap.parse_args()
    base = args.base
    d = args.delay

    print(f"\nCible : {base}")
    print("Test de securite autorise, non destructif, a faible debit.\n")

    # 1. Transport + entetes de securite (verif apres deploiement).
    print("1. Transport et en-tetes de securite (sur /app.html)")
    st, h, _ = req(base, "GET", "/app.html")
    line("INFO", "GET /app.html", f"status={st}")
    for hn in SECURITY_HEADERS:
        if hn in h:
            line("PASS", hn, h[hn][:110])
        else:
            line("ATTENTION", hn, "absent (correctif pas encore deploye ?)")
    time.sleep(d)

    # 2. Fichiers sensibles : ne doivent pas etre servis.
    print("\n2. Fichiers sensibles (attendu : pas de 200 avec contenu)")
    for p in MUST_NOT_SERVE:
        st, _, body = req(base, "GET", p)
        served = st == 200 and body and "route inconnue" not in body
        verdict = "ATTENTION" if served else "PASS"
        extra = "SERVI EN CLAIR" if served else f"status={st}"
        line(verdict, f"GET {p}", extra)
        time.sleep(d)

    # 3. Routes protegees sans authentification.
    print("\n3. Controle d'acces (sans session ni jeton ; attendu 401/404)")
    for method, p in PROTECTED:
        st, _, _ = req(base, method, p)
        ok = st in (401, 403, 404)
        line("PASS" if ok else "ATTENTION", f"{method} {p}", f"status={st}")
        time.sleep(d)

    # 4. Gestion d'erreur : corps JSON malforme -> 400, jamais 500 ni trace.
    print("\n4. Gestion d'erreur (pas de fuite technique)")
    st, _, body = req(base, "POST", "/app/login", {"Content-Type": "application/json"},
                      body="ceci-n-est-pas-du-json")
    leak = looks_like_stacktrace(body)
    line("PASS" if st == 400 and not leak else "ATTENTION",
         "POST /app/login (corps invalide)", f"status={st}, trace={'oui' if leak else 'non'}")
    time.sleep(d)
    st, _, body = req(base, "GET", "/zzz-route-inexistante")
    line("PASS" if st == 404 and not looks_like_stacktrace(body) else "ATTENTION",
         "GET route inconnue", f"status={st}")
    time.sleep(d)

    # 5. CORS : l'origine d'un tiers ne doit pas etre reflechie.
    print("\n5. CORS (une origine tierce ne doit pas etre autorisee)")
    st, h, _ = req(base, "GET", "/app/me", {"Origin": "https://exemple-tiers.invalid"})
    acao = h.get("access-control-allow-origin", "")
    bad = acao in ("*", "https://exemple-tiers.invalid")
    line("ATTENTION" if bad else "PASS", "Access-Control-Allow-Origin",
         acao or "(absent)")
    time.sleep(d)

    # 6. Plafond anti-force-brute (opt-in, adresse bidon).
    if args.include_ratelimit:
        print("\n6. Plafond anti-force-brute (adresse bidon, opt-in)")
        got429 = False
        for i in range(11):
            st, _, _ = req(base, "POST", "/app/login",
                           {"Content-Type": "application/json"},
                           body=json.dumps({"email": "probe-ratelimit@exemple.invalid",
                                            "password": "x"}))
            if st == 429:
                got429 = True
                line("PASS", f"tentative {i+1}", "429 (plafond atteint)")
                break
            line("INFO", f"tentative {i+1}", f"status={st}")
            time.sleep(max(d, 0.5))
        if not got429:
            line("ATTENTION", "plafond", "aucun 429 apres 11 essais")
    else:
        print("\n6. Plafond anti-force-brute : ignore (passer --include-ratelimit)")

    print("\nTermine. Colle cette sortie pour analyse.\n")


if __name__ == "__main__":
    sys.exit(main())
