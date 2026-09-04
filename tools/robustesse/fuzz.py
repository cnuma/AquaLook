"""Batterie de robustesse HTTP pour AquaLook (.141), par le reseau seulement.

Ne vise QUE des entrees hostiles censees etre rejetees, ou des lectures : le
but est de voir si le module plante, gele ou fuit, pas de modifier sa
configuration valide. Les routes destructrices (wifi, resetConfig, cloudSync,
maj, deploy, manual) sont exclues par construction -- elles ne figurent nulle
part ici.

Apres chaque test agressif, une sonde de vie relit /api/diagnostics : si
l'uptime a recule, le test a fait redemarrer le module, et on le nomme. Si le
module reste injoignable, on s'arrete pour ne pas mitrailler un module a terre.

Usage : python fuzz.py <batterie: http|json|bornes|charge|tout>
"""
import socket, sys, time, json, urllib.request, datetime, threading

HOTE = "192.168.1.141"
PORT = 80
BASE = "http://%s" % HOTE
LOG = None


def note(msg):
    ts = datetime.datetime.now().strftime("%H:%M:%S")
    ligne = "%s %s" % (ts, msg)
    print(ligne)
    if LOG:
        LOG.write(ligne + "\n"); LOG.flush()


def uptime():
    """Uptime du module, ou None si injoignable."""
    try:
        with urllib.request.urlopen(BASE + "/api/diagnostics", timeout=8) as r:
            return json.loads(r.read())["system"]["uptimeSec"]
    except Exception:
        return None


def vie(ref, etiquette):
    """Sonde de vie apres un test. ref = uptime d'avant. Rend l'uptime courant."""
    for essai in range(6):
        up = uptime()
        if up is not None:
            if ref is not None and up < ref - 3:
                note("  !!! REDEMARRAGE apres [%s] : uptime %s -> %s" % (etiquette, ref, up))
            return up
        time.sleep(5)
    note("  !!! INJOIGNABLE apres [%s] (30s) -- arret de la batterie" % etiquette)
    return None


def brut(donnees, lire=True, timeout=8):
    """Envoie des octets bruts sur le port 80, rend la reponse (ou '')."""
    try:
        s = socket.socket()
        s.settimeout(timeout)
        s.connect((HOTE, PORT))
        s.sendall(donnees)
        if not lire:
            s.close(); return "envoye"
        rep = b""
        while len(rep) < 4096:
            b = s.recv(4096)
            if not b: break
            rep += b
        s.close()
        return rep.decode("latin-1")[:200]
    except Exception as e:
        return "EXC:%s" % e


def premiere_ligne(rep):
    return rep.split("\r\n", 1)[0] if rep else "(vide)"


# ── Batterie HTTP : abus du protocole ──────────────────────────────────────
def batterie_http():
    note("=== BATTERIE HTTP (abus de protocole) ===")
    ref = uptime(); note("uptime de reference : %s" % ref)
    tests = [
        ("uri geante (64 Ko)",
         b"GET /" + b"A" * 65536 + b" HTTP/1.1\r\nHost: x\r\n\r\n"),
        ("en-tete geant (64 Ko)",
         b"GET / HTTP/1.1\r\nHost: x\r\nX-Bomb: " + b"A" * 65536 + b"\r\n\r\n"),
        ("2000 en-tetes",
         b"GET / HTTP/1.1\r\nHost: x\r\n" + b"X-H: v\r\n" * 2000 + b"\r\n"),
        ("ligne de requete absurde",
         b"\x01\x02\x03 GARBAGE\r\n\r\n"),
        ("octets nuls dans l'uri",
         b"GET /\x00\x00/../etc HTTP/1.1\r\nHost: x\r\n\r\n"),
        ("methode inconnue",
         b"BREW / HTTP/1.1\r\nHost: x\r\n\r\n"),
        ("TRACE",
         b"TRACE / HTTP/1.1\r\nHost: x\r\n\r\n"),
        ("version http absurde",
         b"GET / HTTP/9.9\r\nHost: x\r\n\r\n"),
        ("sans Host",
         b"GET / HTTP/1.1\r\n\r\n"),
        ("Content-Length negatif",
         b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Length: -1\r\n\r\n{}"),
        ("Content-Length non numerique",
         b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Length: abc\r\n\r\n{}"),
        ("Content-Length enorme, corps minuscule",
         b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Length: 10000000\r\n\r\n{}"),
        ("double Content-Length",
         b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Length: 2\r\nContent-Length: 100\r\n\r\n{}"),
        ("corps 512 Ko",
         b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Length: 524288\r\n\r\n" + b"A" * 524288),
        ("requetes en pipeline",
         b"GET /api/status HTTP/1.1\r\nHost: x\r\n\r\nGET /api/status HTTP/1.1\r\nHost: x\r\n\r\n"),
    ]
    for nom, donnees in tests:
        rep = brut(donnees)
        note("  [%s] -> %s" % (nom, premiere_ligne(rep) if not rep.startswith("EXC") else rep))
        ref = vie(ref, nom)
        if ref is None:
            return False
    note("=== fin batterie HTTP, module vivant, uptime=%s ===" % ref)
    return True


# ── Batterie slowloris : en-tetes au compte-gouttes, connexions retenues ────
def batterie_charge():
    note("=== BATTERIE CHARGE (slowloris + tempete de connexions) ===")
    ref = uptime(); note("uptime de reference : %s" % ref)

    # Slowloris : 30 connexions qui envoient un octet d'en-tete toutes les 2 s.
    note("  slowloris : 30 connexions lentes, 20 s")
    socks = []
    for _ in range(30):
        try:
            s = socket.socket(); s.settimeout(5)
            s.connect((HOTE, PORT))
            s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\n")
            socks.append(s)
        except Exception:
            pass
    note("    %d connexions retenues" % len(socks))
    accessible = True
    for _ in range(10):
        for s in socks:
            try: s.sendall(b"X-a: b\r\n")
            except Exception: pass
        if uptime() is None:
            accessible = False
        time.sleep(2)
    for s in socks:
        try: s.close()
        except Exception: pass
    note("    module joignable pendant le slowloris : %s" % accessible)
    ref = vie(ref, "slowloris")
    if ref is None:
        return False

    # Tempete : 300 requetes GET en 30 fils.
    note("  tempete : 300 requetes /api/status en 30 fils")
    res = {"ok": 0, "ko": 0}
    verrou = threading.Lock()

    def rafale(n):
        o = k = 0
        for _ in range(n):
            try:
                with urllib.request.urlopen(BASE + "/api/status", timeout=8) as r:
                    r.read(); o += 1
            except Exception:
                k += 1
        with verrou:
            res["ok"] += o; res["ko"] += k

    fils = [threading.Thread(target=rafale, args=(10,)) for _ in range(30)]
    t0 = time.time()
    for f in fils: f.start()
    for f in fils: f.join()
    note("    %d ok, %d ko en %.1fs" % (res["ok"], res["ko"], time.time() - t0))
    ref = vie(ref, "tempete")
    note("=== fin batterie CHARGE, uptime=%s ===" % ref)
    return ref is not None


if __name__ == "__main__":
    quoi = sys.argv[1] if len(sys.argv) > 1 else "http"
    chemin = sys.argv[2] if len(sys.argv) > 2 else None
    if chemin:
        LOG = open(chemin, "a", encoding="utf-8")
    fns = {"http": batterie_http, "charge": batterie_charge}
    if quoi == "tout":
        for f in (batterie_http, batterie_charge):
            if not f(): break
    elif quoi in fns:
        fns[quoi]()
    else:
        note("batterie inconnue : %s" % quoi)
