"""Fuzzing du parseur JSON et des bornes d'index, sur les routes NON destructrices.

Envoie surtout des entrees censees etre rejetees : elles n'ecrivent donc pas de
configuration valide. Les rares valeurs limites utilisent un indice de zone
hors bornes (255), rejete avant toute ecriture. Le but reste de faire planter
le parseur ou la validation, pas de deranger la config.
"""
import socket, sys, time, json, urllib.request, datetime

HOTE = "192.168.1.141"
BASE = "http://%s" % HOTE
LOG = None


def note(msg):
    ts = datetime.datetime.now().strftime("%H:%M:%S")
    print(msg, flush=True)
    if LOG:
        LOG.write("%s %s\n" % (ts, msg)); LOG.flush()


def uptime():
    try:
        with urllib.request.urlopen(BASE + "/api/diagnostics", timeout=8) as r:
            return json.loads(r.read())["system"]["uptimeSec"]
    except Exception:
        return None


def vie(ref, etq):
    for _ in range(6):
        up = uptime()
        if up is not None:
            if ref is not None and up < ref - 3:
                note("  !!! REDEMARRAGE apres [%s] : %s -> %s" % (etq, ref, up))
            return up
        time.sleep(5)
    note("  !!! INJOIGNABLE apres [%s]" % etq)
    return None


def post(chemin, corps, ctype="application/json"):
    """POST brut, rend (code, debut de reponse)."""
    if isinstance(corps, str):
        corps = corps.encode("latin-1", "replace")
    req = (b"POST " + chemin.encode() + b" HTTP/1.1\r\nHost: x\r\n"
           b"Content-Type: " + ctype.encode() + b"\r\n"
           b"Content-Length: " + str(len(corps)).encode() + b"\r\n"
           b"Connection: close\r\n\r\n" + corps)
    try:
        s = socket.socket(); s.settimeout(10); s.connect((HOTE, 80))
        s.sendall(req)
        rep = b""
        while len(rep) < 512:
            b = s.recv(512)
            if not b: break
            rep += b
        s.close()
        ligne = rep.split(b"\r\n", 1)[0].decode("latin-1")
        return ligne
    except Exception as e:
        return "EXC:%s" % e


def batterie_json():
    note("=== BATTERIE JSON (parseur) ===")
    ref = uptime(); note("uptime de reference : %s" % ref)
    cible = "/api/mode"
    A = "A" * 100000
    cas = [
        ("objet imbrique x3000", "{" * 3000 + "}" * 3000),
        ("tableau imbrique x3000", "[" * 3000 + "]" * 3000),
        ("chaine de 100 Ko", '{"zone":0,"mode":"%s"}' % A),
        ("corps de 100 Ko non-json", A),
        ("nombre demesure", '{"zone":1e400,"mode":-1e400}'),
        ("NaN / Infinity bruts", '{"zone":NaN,"mode":Infinity}'),
        ("json tronque", '{"zone":0,"mode":'),
        ("cle dupliquee", '{"zone":0,"zone":1,"zone":2,"mode":0}'),
        ("chaine de format", '{"zone":0,"mode":"%s%n%x%x%x"}'),
        ("octets nuls", '{"zone":0,\x00"mode":0}'),
        ("corps vide", ""),
        ("json scalaire", "12345"),
        ("json = true", "true"),
        ("ctype menteur (texte)", ("{\"zone\":0,\"mode\":0}", "text/plain")),
        ("unicode profond", '{"zone":0,"mode":"' + chr(0xFFFF) + chr(0xD800) + '"}'),
    ]
    for nom, c in cas:
        if isinstance(c, tuple):
            corps, ct = c
            rep = post(cible, corps, ct)
        else:
            rep = post(cible, c)
        note("  [%s] -> %s" % (nom, rep))
        ref = vie(ref, nom)
        if ref is None:
            return False
    note("=== fin JSON, uptime=%s ===" % ref)
    return True


def batterie_bornes():
    note("=== BATTERIE BORNES (indices hors plage, rejet attendu) ===")
    ref = uptime(); note("uptime de reference : %s" % ref)

    # GET /api/zone avec des indices absurdes (lecture seule).
    for z in ["255", "-1", "abc", "", "999999999999999999999", "0x10", "1e9"]:
        try:
            with urllib.request.urlopen(BASE + "/api/zone?z=" + z, timeout=8) as r:
                code = r.status; corps = r.read()[:60]
        except urllib.error.HTTPError as e:
            code = e.code; corps = b""
        except Exception as e:
            code = "EXC"; corps = str(e).encode()
        note("  GET /api/zone?z=%-24s -> %s %s" % (z, code, corps[:40]))
    ref = vie(ref, "zone GET bornes")
    if ref is None: return False

    # POST avec zone hors plage : doit etre rejete sans ecrire.
    posts = [
        ("/api/mode", '{"zone":255,"mode":255}'),
        ("/api/mode", '{"zone":-1,"mode":0}'),
        ("/api/dayslot", '{"zone":255,"day":255,"slot":255,"hour":255,"minute":255,"duration":999999,"enabled":true}'),
        ("/api/intervalslot", '{"zone":255,"slot":255,"hour":99,"minute":99,"duration":-5,"enabled":true}'),
        ("/api/rain", '{"zone":255,"threshMm":-100,"hours":99999}'),
        ("/api/manualDuration", '{"minutes":999999}'),
        ("/api/manualDuration", '{"minutes":-5}'),
        ("/api/zoneName", '{"zone":255,"name":"%s%n"}'),
        ("/api/intervalAnchor", '{"zone":255,"anchorDay":999999}'),
    ]
    for chemin, corps in posts:
        rep = post(chemin, corps)
        note("  POST %-22s %s -> %s" % (chemin, corps[:40], rep))
        ref = vie(ref, "POST " + chemin)
        if ref is None: return False
    note("=== fin BORNES, uptime=%s ===" % ref)
    return True


if __name__ == "__main__":
    quoi = sys.argv[1] if len(sys.argv) > 1 else "json"
    if len(sys.argv) > 2:
        LOG = open(sys.argv[2], "a", encoding="utf-8")
    if quoi == "json":
        batterie_json()
    elif quoi == "bornes":
        batterie_bornes()
    elif quoi == "tout":
        batterie_json() and batterie_bornes()
