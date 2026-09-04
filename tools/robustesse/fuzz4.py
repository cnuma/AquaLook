"""Campagne de robustesse n°3 : vecteurs plus retors -- courses, confusion de
protocole, churn de connexions, inondation en pipeline. WiFi seul.
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
    for _ in range(8):
        u = uptime()
        if u is not None:
            if ref is not None and u < ref - 3:
                note("  !!! REDEMARRAGE apres [%s] : %s -> %s" % (etq, ref, u))
            return u
        time.sleep(5)
    note("  !!! INJOIGNABLE apres [%s]" % etq)
    return None


def raw(donnees, timeout=6):
    try:
        s = socket.socket(); s.settimeout(timeout); s.connect((HOTE, 80))
        s.sendall(donnees)
        r = s.recv(300); s.close()
        return r.split(b"\r\n", 1)[0].decode("latin-1") if r else "(vide)"
    except Exception as e:
        return "EXC:%s" % e


def zone_mode(z=0):
    try:
        with urllib.request.urlopen("http://%s/api/zone?z=%d" % (HOTE, z), timeout=8) as r:
            return json.loads(r.read()).get("mode")
    except Exception:
        return None


# 1. Course sur ecritures concurrentes de config : integrite preservee ?
def test_course():
    note("[course] 40 POST /api/mode concurrents sur la zone 0")
    ref = uptime()
    resultats = {"codes": {}}
    verrou = threading.Lock()

    def ecrire(mode):
        body = ('{"zone":0,"mode":%d}' % mode).encode()
        try:
            req = urllib.request.Request(
                "http://%s/api/mode" % HOTE, data=body,
                headers={"Content-Type": "application/json"}, method="POST")
            with urllib.request.urlopen(req, timeout=8) as r:
                code = r.status
        except urllib.error.HTTPError as e:
            code = e.code
        except Exception:
            code = "EXC"
        with verrou:
            resultats["codes"][code] = resultats["codes"].get(code, 0) + 1

    fils = [threading.Thread(target=ecrire, args=(i % 2,)) for i in range(40)]
    for f in fils: f.start()
    for f in fils: f.join()
    note("  codes: %s" % resultats["codes"])
    time.sleep(2)
    m = zone_mode(0)
    note("  mode zone0 apres course: %s (attendu 0 ou 1, pas de valeur corrompue)" % m)
    ok = m in (0, 1)
    note("  integrite: %s" % ("OK" if ok else "CORROMPU"))
    return vie(ref, "course")


# 2. Confusion de protocole / smuggling.
def test_protocole():
    ref = uptime()
    cas = [
        ("double Content-Length",
         b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
         b"Content-Length: 10\r\nContent-Length: 40\r\n\r\n{\"zone\":0,\"mode\":0}"),
        ("CL + Transfer-Encoding",
         b"POST /api/mode HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
         b"Content-Length: 20\r\nTransfer-Encoding: chunked\r\n\r\n5\r\n{\"zo\r\n0\r\n\r\n"),
        ("preface HTTP/2",
         b"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"),
        ("Expect 100-continue",
         b"POST /api/mode HTTP/1.1\r\nHost: x\r\nExpect: 100-continue\r\n"
         b"Content-Type: application/json\r\nContent-Length: 18\r\n\r\n{\"zone\":0,\"mode\":0}"),
        ("double Host",
         b"GET /api/status HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n"),
        ("espaces avant deux-points",
         b"GET /api/status HTTP/1.1\r\nHost : x\r\nContent-Length : 5\r\n\r\n"),
        ("methode en minuscules",
         b"get /api/status HTTP/1.1\r\nHost: x\r\n\r\n"),
    ]
    for nom, d in cas:
        note("  [%s] -> %s" % (nom, raw(d)))
        ref = vie(ref, nom)
        if ref is None: return None
    return ref


# 3. URL percent-encoding retors.
def test_url():
    ref = uptime()
    for u in ["/api/status%00", "/api/status%0d%0aInjected:1",
              "/%2e%2e/%2e%2e/etc", "/api/zone?z=%31", "/api/%73tatus",
              "/api/status?" + "a=1&" * 500]:
        try:
            with urllib.request.urlopen("http://%s%s" % (HOTE, u), timeout=6) as r:
                code = r.status
        except urllib.error.HTTPError as e:
            code = e.code
        except Exception as e:
            code = "EXC:%s" % str(e)[:30]
        note("  GET %-40s -> %s" % (u[:40], code))
    return vie(ref, "url encoding")


# 4. Churn : 500 connexions ouvertes puis fermees aussitot (RST).
def test_churn():
    note("[churn] 500 connexions connect+close rapides")
    ref = uptime()
    for _ in range(500):
        try:
            s = socket.socket(); s.settimeout(2); s.connect((HOTE, 80))
            s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                         __import__("struct").pack("ii", 1, 0))  # RST a la fermeture
            s.close()
        except Exception:
            pass
    note("  termine")
    return vie(ref, "churn")


# 5. Inondation en pipeline : 200 requetes valides sur une seule connexion.
def test_pipeline():
    note("[pipeline] 200 GET /api/status sur une connexion")
    ref = uptime()
    try:
        s = socket.socket(); s.settimeout(10); s.connect((HOTE, 80))
        req = b"GET /api/status HTTP/1.1\r\nHost: x\r\n\r\n" * 200
        s.sendall(req)
        recu = 0
        s.settimeout(8)
        try:
            while True:
                b = s.recv(4096)
                if not b: break
                recu += len(b)
        except Exception:
            pass
        s.close()
        note("  %d octets recus en reponse" % recu)
    except Exception as e:
        note("  EXC: %s" % e)
    return vie(ref, "pipeline")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        LOG = open(sys.argv[1], "a", encoding="utf-8")
    note("=== CAMPAGNE 3 : courses / protocole / url / churn / pipeline ===")
    note("uptime de reference : %s" % uptime())
    for fn in (test_course, test_protocole, test_url, test_churn, test_pipeline):
        if fn() is None:
            note("Arret : module injoignable.")
            break
    note("=== fin campagne 3, uptime=%s ===" % uptime())
