"""Mouchard serie en lecture seule, pour la nuit de test de robustesse.

Ne touche a rien : ouvre COM4 en posant DTR (le CDC natif du S3 reste muet
sans lui) et RTS bas, sans jamais basculer les lignes -- donc sans provoquer
de reset. Chaque ligne est horodatee et ajoutee au journal. On y relira apres
coup tout Guru Meditation, abort(), rst:, panic ou banniere de demarrage : la
preuve d'un plantage que l'interface HTTP, morte avec le module, ne montrerait
pas.
"""
import serial, sys, time, datetime

CHEMIN = sys.argv[1] if len(sys.argv) > 1 else "serial.log"
DUREE = int(sys.argv[2]) if len(sys.argv) > 2 else 28800   # 8 h par defaut

s = serial.Serial("COM4", 115200, timeout=1)
s.setDTR(True)
s.setRTS(False)

fin = time.time() + DUREE
with open(CHEMIN, "a", encoding="utf-8") as f:
    f.write("\n===== moniteur demarre %s =====\n" % datetime.datetime.now().isoformat())
    f.flush()
    while time.time() < fin:
        try:
            l = s.readline()
        except Exception as e:
            f.write("[monitor] erreur lecture: %s\n" % e); f.flush()
            time.sleep(1); continue
        if not l:
            continue
        ts = datetime.datetime.now().strftime("%H:%M:%S")
        f.write("%s %s" % (ts, l.decode("utf-8", "replace")))
        f.flush()
s.close()
