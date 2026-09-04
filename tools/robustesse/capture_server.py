"""Ecouteur HTTP jetable pour la demonstration d'exfiltration.

Le module, si on redirige sa synchronisation cloud vers ce PC en HTTP clair,
vient y poster son rapport avec l'en-tete Authorization: Bearer <jeton>. Ce
serveur note la requete complete -- en-tetes compris -- et repond un 200
minimal pour que l'echange se termine proprement.

Ne sert qu'a prouver la fuite : il est ferme et la configuration du module
restauree juste apres.
"""
import socket, sys, datetime

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8123
CHEMIN = sys.argv[2] if len(sys.argv) > 2 else "capture.log"
DUREE = int(sys.argv[3]) if len(sys.argv) > 3 else 200

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", PORT))
srv.listen(8)
srv.settimeout(DUREE)

reponse = (b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
           b"Content-Length: 2\r\nConnection: close\r\n\r\n{}")

with open(CHEMIN, "a", encoding="utf-8") as f:
    f.write("== ecouteur sur %d, %ss ==\n" % (PORT, DUREE)); f.flush()
    import time
    fin = time.time() + DUREE
    while time.time() < fin:
        try:
            c, addr = srv.accept()
        except socket.timeout:
            break
        c.settimeout(5)
        data = b""
        try:
            while b"\r\n\r\n" not in data and len(data) < 8192:
                bout = c.recv(4096)
                if not bout:
                    break
                data += bout
            # Lire un eventuel corps annonce, pour ne rien laisser trainer.
            c.sendall(reponse)
        except Exception as e:
            f.write("[erreur] %s\n" % e)
        finally:
            c.close()
        ts = datetime.datetime.now().strftime("%H:%M:%S")
        txt = data.decode("latin-1")
        f.write("\n--- %s depuis %s ---\n%s\n" % (ts, addr[0], txt))
        f.flush()
srv.close()
