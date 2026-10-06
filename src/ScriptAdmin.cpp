#include "ScriptAdmin.h"

#include <cstring>

#include "EventLog.h"
#include "ScriptMessageCatalogue.h"
#include "StorageManager.h"

namespace ScriptAdmin {

const char* writeScript(const ScriptWrite& w, StorageManager* storage,
                        bool& sourceSaved) {
    sourceSaved = false;
    if (w.index >= ScriptStore::MAX_SCRIPTS) return "emplacement invalide";
    if (w.code == nullptr || w.codeSize == 0U) return "programme vide";
    if (w.codeSize > ScriptStore::MAX_BYTECODE) return "programme trop long";
    if (w.trigger > ScriptStore::TRIGGER_ZONE_STOP) return "declencheur inconnu";
    // Un declencheur sans cible ne partirait jamais : le refuser vaut mieux
    // que d'enregistrer une regle qui ne joue pas.
    if (w.trigger != ScriptStore::TRIGGER_NONE && w.target == 0U) {
        return "ce declencheur demande une cible : entree ou zone";
    }

    ScriptStore::Meta meta{};
    meta.used = true;
    meta.enabled = w.enabled;
    meta.trigger = w.trigger;
    meta.triggerTarget = w.target;
    meta.codeSize = w.codeSize;
    strlcpy(meta.name, w.name ? w.name : "sans nom", sizeof(meta.name));

    // ScriptStore::save revalide le bytecode (validateScriptProgram) : le
    // module ne fait confiance ni au navigateur local ni au serveur.
    const char* reason = "";
    if (!ScriptStore::save(w.index, meta, w.code, reason)) {
        return (reason && reason[0]) ? reason : "enregistrement refuse";
    }

    // Le source suit le bytecode, jamais l'inverse : si l'ecriture SD echoue,
    // le script tourne quand meme. On le signale sans faire echouer
    // l'enregistrement -- perdre le confort d'edition n'est pas perdre la
    // regle.
    if (storage && storage->isSdAvailable() && w.source && w.source[0] != '\0') {
        // openWrite cree deja le repertoire parent si besoin.
        char path[32];
        snprintf(path, sizeof(path), "/scripts/s%u.txt", (unsigned)w.index);
        FsFile f;
        if (storage->openWrite(path, f)) {
            const size_t len = strlen(w.source);
            sourceSaved = storage->writeChunk(
                f, reinterpret_cast<const uint8_t*>(w.source), len) == (int32_t)len;
            storage->closeFile(f);
        }
        if (!sourceSaved) {
            EventLog::log(LOG_WARN,
                          "Scripts: source %u non enregistree, le programme tourne quand meme",
                          (unsigned)w.index);
        }
    }
    return nullptr;
}

const char* buildPhrasesCorpus(JsonArrayConst entries, String& corpus,
                               uint8_t& count) {
    count = 0U;
    corpus = "";
    if (entries.isNull()) return "liste manquante";
    if (entries.size() > ScriptMessageCatalogue::MAX_ENTRIES) {
        return "trop d'entrees dans le catalogue";
    }

    uint16_t seen[ScriptMessageCatalogue::MAX_ENTRIES];
    for (JsonObjectConst e : entries) {
        const long code = e["code"] | 0;
        const char* texte = e["texte"] | "";
        if (code < 1 || code > 65535) return "code hors bornes (1 a 65535)";
        const size_t tlen = strlen(texte);
        if (tlen == 0U) return "phrase vide";
        if (tlen > ScriptMessageCatalogue::MAX_PHRASE) return "phrase trop longue";
        for (size_t k = 0U; k < tlen; ++k) {
            const unsigned char c = static_cast<unsigned char>(texte[k]);
            // TAB et fin de ligne casseraient le format ; les autres
            // caracteres de controle n'ont rien a faire dans une phrase.
            if (c == '\t' || c == '\n' || c == '\r' || c < 0x20) {
                return "caractere de controle interdit dans une phrase";
            }
        }
        for (uint8_t k = 0U; k < count; ++k) {
            if (seen[k] == static_cast<uint16_t>(code)) {
                return "code en double dans le catalogue";
            }
        }
        seen[count++] = static_cast<uint16_t>(code);
        corpus += String(static_cast<uint16_t>(code));
        corpus += '\t';
        corpus += texte;
        corpus += '\n';
    }
    return nullptr;
}

} // namespace ScriptAdmin
