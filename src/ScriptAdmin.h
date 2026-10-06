#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>

#include "ScriptStore.h"

class StorageManager;

// Ecriture des scripts et du catalogue de phrases, commune aux deux voies
// d'entree : les routes locales signees (/api/script-save,
// /api/script-messages) et config.apply recu du serveur (decision D014,
// docs/architecture/CLOUD_REMOTE_CONFIG.md).
//
// Une seule definition de ce qu'est un script ou un catalogue acceptable :
// la voie distante ne doit jamais accepter ce que la voie locale refuse.
// La signature HMAC, elle, reste propre a la voie locale -- elle se verifie
// AVANT d'appeler ce module.
//
// Les messages d'erreur sont ceux, inchanges, des routes locales : ils
// remontent tels quels a l'editeur ou dans l'accuse de la commande.
namespace ScriptAdmin {

struct ScriptWrite {
    uint8_t        index    = 255U;
    const char*    name     = nullptr;   // nullptr -> "sans nom"
    bool           enabled  = true;
    uint8_t        trigger  = ScriptStore::TRIGGER_INPUT_CHANGE;
    uint16_t       target   = 0U;
    const uint8_t* code     = nullptr;
    uint16_t       codeSize = 0U;
    const char*    source   = nullptr;   // vide ou nullptr : source inchange
};

// Valide puis enregistre un script (bytecode en NVS, source sur SD).
// Rend nullptr en cas de succes, sinon le motif du refus. sourceSaved dit
// si le texte a ete ecrit : son echec n'empeche pas le script de tourner.
const char* writeScript(const ScriptWrite& w, StorageManager* storage,
                        bool& sourceSaved);

// Valide un catalogue {code, texte} et construit les octets TSV exacts a
// ecrire. Rend nullptr en cas de succes, sinon le motif du refus.
const char* buildPhrasesCorpus(JsonArrayConst entries, String& corpus,
                               uint8_t& count);

} // namespace ScriptAdmin
