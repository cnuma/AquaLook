"""Campagne de robustesse n°2 : vecteurs inedits (corps, chunked, multipart,
connexions). Toujours par le WiFi, routes non destructrices.

Chaque test verifie la vie du module apres coup et s'arrete s'il ne repond plus,
pour ne pas mitrailler un module a terre.
"""
import socket, sys, time, json, urllib.request, datetime, threading

HOTE = "192.168.1.141"
LOG = None


def note(m):
    ts = datetime.datetime.now().strftime("%H:%M:%S")
    print(m, flush=True)
    if LOG: LOG.write("%s %s\n" % (ts, m)); LOG.flush()


def uptime():
    try:
        with urllib.request.urlopen("http://%s/api/diagnostics" % HOTE, timeout=8) as r:
            return json.loads(r.read())["system"]["uptimeSec"]
    except Exception:
        return None


def vie(ref, etq):
    for _ in range(6):
        u = uptime()
        if u is not None:
            if ref is not None and u < ref - 3:
                note("  !!! REDEMARRAGE apres [%s] : %s -> %s" % (etq, ref, u))
            return u
        time.sleep(5)
    note("  !!! INJOIGNABLE apres [%s] (30s)" % etq)
    return None


def raw(donnees, attente_reponse=True, timeout=8):
    try:
        s = socket.socket(); s.settimeout(timeout); s.connect((HOTE, 80))
        s.sendall(donnees)
        if not attente_reponse:
            return s  # laisser ouverte (appelant ferme)
        r = s.recv(200); s.close()
        return r.split(b"\r\n", 1)[0].decode("latin-1") if r else "(vide)"
    except Exception as e:
        return "EXC:%s" % e


def campagne():
    note("=== CAMPAGNE 2 : corps / chunked / multipart / connexions ===")
    ref = uptime(); note("uptime de reference : %s" % ref)

    # 1. Content-Length ment plus grand que le corps : le serveur attend un
    #    corps qui n'arrive jamais.
    note("[CL trop grand] CL=100000, 10 octets envoyes")
    r = raw(b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
            b"Content-Length: 100000\r\n\r\n{\"zone\":0}", timeout=6)
    note("  -> %s" % r)
    ref = vie(ref, "CL trop grand");  _stop(ref)

    # 2. Content-Length ment plus petit : octets en trop apres le corps.
    note("[CL trop petit] CL=5, 200 octets envoyes")
    r = raw(b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
            b"Content-Length: 5\r\n\r\n{\"zone\":0,\"mode\":0}" + b"A" * 200)
    note("  -> %s" % r)
    ref = vie(ref, "CL trop petit");  _stop(ref)

    # 3. Corps chunked avec des tailles de morceau aberrantes.
    for nom, corps in [
        ("chunked taille enorme", b"FFFFFFFF\r\n{\"zone\":0}\r\n0\r\n\r\n"),
        ("chunked non-hex", b"ZZZ\r\ndata\r\n0\r\n\r\n"),
        ("chunked sans fin", b"5\r\n{\"zo\r\n"),
        ("chunked negatif", b"-1\r\ndata\r\n0\r\n\r\n"),
    ]:
        note("[%s]" % nom)
        r = raw(b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
                b"Transfer-Encoding: chunked\r\n\r\n" + corps, timeout=6)
        note("  -> %s" % r)
        ref = vie(ref, nom);  _stop(ref)

    # 4. Multipart pathologique (le parseur multipart est un chemin distinct).
    for nom, entete, corps in [
        ("multipart boundary vide",
         b"multipart/form-data; boundary=", b"--\r\nrien\r\n----\r\n"),
        ("multipart boundary 16 Ko",
         b"multipart/form-data; boundary=" + b"A" * 16384,
         b"--" + b"A" * 16384 + b"\r\n\r\n"),
        ("multipart sans terminateur",
         b"multipart/form-data; boundary=xyz",
         b"--xyz\r\nContent-Disposition: form-data; name=\"f\"\r\n\r\n" + b"B" * 4000),
    ]:
        note("[%s]" % nom)
        req = (b"POST /api/saveSchedule HTTP/1.1\r\nHost: x\r\nContent-Type: " + entete +
               b"\r\nContent-Length: " + str(len(corps)).encode() + b"\r\n\r\n" + corps)
        r = raw(req, timeout=6)
        note("  -> %s" % r)
        ref = vie(ref, nom);  _stop(ref)

    # 5. Epuisement : 60 connexions qui envoient un debut de requete et tiennent.
    note("[epuisement] 60 connexions retenues 25 s")
    socks = []
    for _ in range(60):
        try:
            s = socket.socket(); s.settimeout(4); s.connect((HOTE, 80))
            s.sendall(b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Length: 50\r\n\r\n{")
            socks.append(s)
        except Exception:
            pass
    note("  %d connexions ouvertes" % len(socks))
    joignable = uptime() is not None
    note("  joignable pendant l'epuisement : %s" % joignable)
    for s in socks:
        try: s.close()
        except Exception: pass
    ref = vie(ref, "epuisement");  _stop(ref)

    note("=== fin campagne 2, uptime=%s ===" % ref)


def _stop(ref):
    if ref is None:
        note("Arret : module injoignable.")
        raise SystemExit(1)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        LOG = open(sys.argv[1], "a", encoding="utf-8")
    campagne()
