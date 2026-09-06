"""Correctifs de robustesse appliques a ESPAsyncWebServer / AsyncTCP au build.

La campagne de robustesse (docs/ROBUSTESSE_RESEAU_2026-09-04.md) a mis au jour
deux defauts dans la bibliotheque, tous deux exploitables sans authentification
depuis le reseau. La lib n'expose aucun reglage pour s'en premunir ; on la
corrige donc a la source, au plus pres du defaut, en reprenant ses propres
idiomes. Chaque correctif porte un marqueur qui le rend idempotent, et le tout
est rejoue a chaque build car .pio n'est pas versionne.

Patch 1 -- DoS par requete demesuree (defaut n°1).
  AsyncWebServerRequest::_onData accumule la ligne de requete / les en-tetes
  dans un String sans plafond. Quelques centaines de kilo-octets suffisaient a
  bloquer async_tcp au-dela du chien de garde (croissance O(n^2)), d'ou un
  redemarrage. On coupe la connexion des que l'accumulation depasse 8 Ko.

Patch 2 -- Debordement de tas par Content-Length mensonger (CRITIQUE).
  Un corps plus long que le Content-Length annonce corrompait la memoire :
  AsyncCallbackJsonWebHandler::handleBody fait memcpy(malloc(total)+index, data,
  len) sans borner len a total, et le parseur lui livrait la taille complete du
  segment recu au lieu du reste attendu. Les octets en trop debordaient le
  tampon (crash a 0x41414141, l'attaquant controlant les octets ecrits). On
  borne les deux : la copie dans handleBody, et la longueur livree par _parse.

N'echoue jamais le build : un fichier absent (libs pas encore installees) ou une
ancre introuvable (version differente) est signale, et le build continue.
"""
Import("env")  # noqa: F821  (fourni par PlatformIO)

import glob
import os

# (sous-chemin, ancre, remplacement, marqueur)
PATCHES = [
    # ── Patch 1 : plafond ligne/entete ───────────────────────────────────
    (
        os.path.join("ESPAsyncWebServer", "src", "WebRequest.cpp"),
        "        _temp.concat(ch);\n",
        "        _temp.concat(ch);\n"
        "        // AQUALOOK_MAX_REQUEST_LINE : couper une ligne/entete demesuree\n"
        "        // AVANT que la croissance illimitee de _temp (O(n^2)) ne bloque\n"
        "        // async_tcp au-dela du chien de garde. Meme idiome d'avortement\n"
        "        // que la lib pour un caractere nul en en-tete.\n"
        "        if (_temp.length() > 8192) {\n"
        "          _parseState = PARSE_REQ_FAIL;\n"
        "          _client->abort();\n"
        "          return;\n"
        "        }\n",
        "AQUALOOK_MAX_REQUEST_LINE",
    ),
    # ── Patch 2a : borner la longueur livree par le parseur ──────────────
    (
        os.path.join("ESPAsyncWebServer", "src", "WebRequest.cpp"),
        "        if (!_isPlainPost) {\n"
        "          if (_handler)\n"
        "            _handler->handleBody(this, (uint8_t*)buf, len, _parsedLength, _contentLength);\n"
        "          _parsedLength += len;\n",
        "        if (!_isPlainPost) {\n"
        "          // AQUALOOK_BODY_CLAMP : ne jamais livrer au handler plus que le\n"
        "          // Content-Length restant. Un corps plus long debordait sinon le\n"
        "          // tampon du handler (voir AsyncJson). Le surplus est ignore.\n"
        "          size_t aq_body = len;\n"
        "          if (_parsedLength + aq_body > _contentLength)\n"
        "            aq_body = (_parsedLength < _contentLength) ? (_contentLength - _parsedLength) : 0;\n"
        "          if (_handler)\n"
        "            _handler->handleBody(this, (uint8_t*)buf, aq_body, _parsedLength, _contentLength);\n"
        "          _parsedLength += aq_body;\n",
        "AQUALOOK_BODY_CLAMP",
    ),
    # ── Patch 2b : borner la copie dans le handler JSON ──────────────────
    (
        os.path.join("ESPAsyncWebServer", "src", "AsyncJson.cpp"),
        "    if (request->_tempObject != NULL) {\n"
        "      memcpy((uint8_t*)(request->_tempObject) + index, data, len);\n"
        "    }\n",
        "    if (request->_tempObject != NULL) {\n"
        "      // AQUALOOK_BODY_BOUND : borner la copie a la taille allouee (total).\n"
        "      // Un Content-Length plus petit que le corps reel faisait deborder ce\n"
        "      // malloc(total) et corrompait le tas (crash a 0x41414141).\n"
        "      size_t aq_len = len;\n"
        "      if (index >= total) aq_len = 0;\n"
        "      else if (index + aq_len > total) aq_len = total - index;\n"
        "      memcpy((uint8_t*)(request->_tempObject) + index, data, aq_len);\n"
        "    }\n",
        "AQUALOOK_BODY_BOUND",
    ),
    # ── Patch 3 : garde a l'acceptation d'une connexion (defaut churn) ────
    # lwIP peut invoquer le callback d'acceptation avec une erreur ou un pcb
    # nul quand une connexion est reinitialisee pendant l'acceptation --
    # frequent sous un churn de connexions (500 connect+RST rapides faisaient
    # redemarrer le module, LoadProhibited dans AsyncServer::_accept). Le code
    # d'origine ne verifiait rien et faisait new AsyncClient(pcb) sur un pcb
    # invalide. On reprend la garde standard des callbacks d'acceptation lwIP.
    (
        os.path.join("AsyncTCP", "src", "AsyncTCP.cpp"),
        "int8_t AsyncServer::_accept(tcp_pcb* pcb, int8_t err) {\n"
        "  // ets_printf(\"+A: 0x%08x\\n\", pcb);\n"
        "  if (_connect_cb) {\n",
        "int8_t AsyncServer::_accept(tcp_pcb* pcb, int8_t err) {\n"
        "  // AQUALOOK_ACCEPT_GUARD : ne jamais deferencer un pcb nul ou remis a\n"
        "  // zero pendant l'acceptation (connexion RST sous churn).\n"
        "  if (pcb == NULL || err != ERR_OK) {\n"
        "    if (pcb != NULL) { tcp_abort(pcb); }\n"
        "    return ERR_OK;\n"
        "  }\n"
        "  // ets_printf(\"+A: 0x%08x\\n\", pcb);\n"
        "  if (_connect_cb) {\n",
        "AQUALOOK_ACCEPT_GUARD",
    ),
    # -- Patch 4 : purger les evenements en file a la destruction du client --
    # Le serveur web detruit le client dans son propre rappel onDisconnect
    # (delete c). Or le destructeur d'origine ne retirait pas de la file les
    # evenements deja empiles pour ce client : le service les traitait ensuite
    # avec un pointeur libere, corrompant le tas. La corruption ne se voyait
    # qu'a la liberation suivante, loin de sa cause.
    #
    # Plantage reel du 6 septembre 2026 a 10:52, sous coupure reseau :
    #   assert block_merge_prev (heap_tlsf.c:344) dans
    #   ~AsyncWebServerRequest -> _headers.clear() -> operator delete,
    #   pile remontant a AsyncClient::_error via _async_service_task.
    # Reproduit a la demande par churn de connexions coupees en RST
    # (tools/robustesse/repro_error_uaf.py), en 3 a 6 tours.
    #
    # La fonction de purge existe deja dans la bibliotheque ; elle n'etait
    # simplement jamais appelee sur ce chemin.
    (
        os.path.join("AsyncTCP", "src", "AsyncTCP.cpp"),
        "AsyncClient::~AsyncClient() {\n"
        "  if (_pcb) {\n"
        "    _close();\n"
        "  }\n"
        "  _free_closed_slot();\n"
        "}\n",
        "AsyncClient::~AsyncClient() {\n"
        "  if (_pcb) {\n"
        "    _close();\n"
        "  }\n"
        "  // AQUALOOK_EVENT_PURGE : retirer de la file les evenements qui\n"
        "  // referencent encore ce client. Sans cela, un client detruit laisse\n"
        "  // des evenements pointant sur de la memoire liberee.\n"
        "  _remove_events_with_arg(this);\n"
        "  _free_closed_slot();\n"
        "}\n",
        "AQUALOOK_EVENT_PURGE",
    ),
]


def libdeps_dir():
    try:
        return env.subst("$PROJECT_LIBDEPS_DIR")
    except Exception:
        return os.path.join(env.subst("$PROJECT_DIR"), ".pio", "libdeps")


def appliquer(_source, _target, _env):
    base = libdeps_dir()
    for sous_chemin, ancre, remplacement, marqueur in PATCHES:
        motif = os.path.join(base, "*", sous_chemin)
        fichiers = glob.glob(motif)
        if not fichiers:
            print("patch_asyncwebserver: %s introuvable (prochain build)" % sous_chemin)
            continue
        for chemin in fichiers:
            with open(chemin, "r", encoding="utf-8", errors="replace") as f:
                contenu = f.read()
            if marqueur in contenu:
                continue
            if ancre not in contenu:
                print("patch_asyncwebserver: ancre [%s] absente dans %s (version differente ?)"
                      % (marqueur, chemin))
                continue
            contenu = contenu.replace(ancre, remplacement, 1)
            with open(chemin, "w", encoding="utf-8") as f:
                f.write(contenu)
            print("patch_asyncwebserver: [%s] pose dans %s" % (marqueur, chemin))


# Une seule application, au chargement du pre-script : il s'execute apres
# l'installation des dependances et avant la compilation, donc la source patchee
# est bien celle qui sera compilee. On ne se re-branche PAS sur "buildprog" :
# modifier une source de lib en cours de build brouille le suivi incremental de
# SCons (echec au premier passage, succes au second).
appliquer(None, None, env)
