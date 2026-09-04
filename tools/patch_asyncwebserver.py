"""Plafonne la taille de la ligne de requete / des en-tetes dans ESPAsyncWebServer.

Pourquoi ce patch existe. La campagne de robustesse du 4 septembre 2026 a montre
qu'une requete HTTP dont la ligne de requete atteint quelques centaines de
kilo-octets fait redemarrer le module : `AsyncWebServerRequest::_onData`
accumule tout dans un `String` (`_temp`) sans aucun plafond, jusqu'a la
nouvelle ligne. La croissance repetee de ce `String` est en O(n^2) et bloque la
tache `async_tcp` au-dela du chien de garde de tache, qui declenche un panic
(voir docs/ROBUSTESSE_RESEAU_2026-09-04.md, defaut n°1). Le garde applicatif
(UriLengthGuard) ne peut rien : il n'est consulte qu'une fois la ligne
entierement accumulee, donc trop tard.

La bibliotheque n'expose aucune limite configurable. On l'ajoute donc a la
source, au plus pres du defaut, en reprenant exactement l'idiome que la lib
utilise deja pour avorter proprement (`_parseState = PARSE_REQ_FAIL;
_client->abort();`).

Le patch est idempotent (un marqueur empeche la double application) et
s'applique a chaque build via extra_scripts, car .pio/libdeps est regenerable
et n'est pas versionne. S'il ne trouve pas le fichier (libs pas encore
installees), il n'echoue pas : le build suivant l'appliquera.
"""
Import("env")  # noqa: F821  (fourni par PlatformIO)

import glob
import os

MARQUEUR = "AQUALOOK_MAX_REQUEST_LINE"
PLAFOND = 8192  # octets : large pour toute requete legitime, loin de l'explosion

ANCRE = "        _temp.concat(ch);\n"
INJECTION = (
    "        _temp.concat(ch);\n"
    "        // " + MARQUEUR + " : couper une ligne de requete/entete demesuree\n"
    "        // AVANT que la croissance illimitee de _temp (O(n^2)) ne bloque\n"
    "        // async_tcp au-dela du chien de garde. Meme idiome d'avortement\n"
    "        // que la lib pour un caractere nul en en-tete.\n"
    "        if (_temp.length() > " + str(PLAFOND) + ") {\n"
    "          _parseState = PARSE_REQ_FAIL;\n"
    "          _client->abort();\n"
    "          return;\n"
    "        }\n"
)


def libdeps_dir():
    try:
        return env.subst("$PROJECT_LIBDEPS_DIR")
    except Exception:
        return os.path.join(env.subst("$PROJECT_DIR"), ".pio", "libdeps")


def appliquer(_source, _target, _env):
    base = libdeps_dir()
    motif = os.path.join(base, "*", "ESPAsyncWebServer", "src", "WebRequest.cpp")
    fichiers = glob.glob(motif)
    if not fichiers:
        print("patch_asyncwebserver: WebRequest.cpp introuvable (sera applique au prochain build)")
        return
    for chemin in fichiers:
        with open(chemin, "r", encoding="utf-8", errors="replace") as f:
            contenu = f.read()
        if MARQUEUR in contenu:
            continue
        if ANCRE not in contenu:
            print("patch_asyncwebserver: ancre absente dans %s (version differente ?)" % chemin)
            continue
        contenu = contenu.replace(ANCRE, INJECTION, 1)
        with open(chemin, "w", encoding="utf-8") as f:
            f.write(contenu)
        print("patch_asyncwebserver: plafond de %d octets pose dans %s" % (PLAFOND, chemin))


# Une seule application, au chargement du pre-script : il s'execute apres
# l'installation des dependances et avant la compilation, donc la source patchee
# est bien celle qui sera compilee. On ne se re-branche PAS sur "buildprog" :
# modifier une source de lib en cours de build brouille le suivi incremental de
# SCons (echec au premier passage, succes au second). Le marqueur rend l'action
# idempotente d'un build a l'autre.
appliquer(None, None, env)
