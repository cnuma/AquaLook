#include "ScriptStore.h"

#include <Preferences.h>
#include <string.h>

#include "EventLog.h"
#include "domain/ScriptVm.h"

namespace ScriptStore {
namespace {

constexpr const char* NVS_NAMESPACE = "aqualook";
constexpr const char* NVS_KEY = "aq_script";
constexpr uint32_t NVS_MAGIC = 0x53435254UL;  // "SCRT"
constexpr uint16_t SCHEMA = 1U;

struct Slot {
    Meta meta;
    uint8_t code[MAX_BYTECODE];
};

struct Persisted {
    uint32_t magic;
    uint16_t schema;
    uint16_t size;
    Slot slots[MAX_SCRIPTS];
    uint32_t crc32;
};

uint32_t crc32Bytes(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8U; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (~(crc & 1U) + 1U));
        }
    }
    return ~crc;
}

// Le bloc pese quelques kilo-octets : il est alloue sur le tas, jamais sur la
// pile. Une pile de tache Arduino ne tient pas 2,5 Ko de plus sans risque.
bool readBlob(Persisted& out) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return false;
    const size_t len = prefs.getBytesLength(NVS_KEY);
    if (len != sizeof(Persisted)) {
        prefs.end();
        if (len != 0U) {
            EventLog::log(LOG_WARN, "Scripts: taille NVS %u inattendue, ignoree",
                          (unsigned)len);
        }
        return false;
    }
    const size_t read = prefs.getBytes(NVS_KEY, &out, sizeof(out));
    prefs.end();
    if (read != sizeof(out) || out.magic != NVS_MAGIC ||
        out.schema != SCHEMA || out.size != sizeof(Persisted)) {
        EventLog::log(LOG_WARN, "Scripts: entete NVS invalide, ignoree");
        return false;
    }
    if (crc32Bytes(reinterpret_cast<const uint8_t*>(&out),
                   offsetof(Persisted, crc32)) != out.crc32) {
        EventLog::log(LOG_WARN, "Scripts: CRC NVS invalide, ignoree");
        return false;
    }
    return true;
}

void blankBlob(Persisted& blob) {
    memset(&blob, 0, sizeof(blob));
    blob.magic = NVS_MAGIC;
    blob.schema = SCHEMA;
    blob.size = sizeof(Persisted);
}

bool writeBlob(Persisted& blob) {
    blob.magic = NVS_MAGIC;
    blob.schema = SCHEMA;
    blob.size = sizeof(Persisted);
    blob.crc32 = crc32Bytes(reinterpret_cast<const uint8_t*>(&blob),
                            offsetof(Persisted, crc32));
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        EventLog::log(LOG_ERROR, "Scripts: ouverture NVS impossible");
        return false;
    }
    const size_t written = prefs.putBytes(NVS_KEY, &blob, sizeof(blob));
    prefs.end();
    if (written != sizeof(blob)) {
        EventLog::log(LOG_ERROR, "Scripts: ecriture NVS incomplete");
        return false;
    }
    return true;
}

// Cache RAM des en-tetes, tenu a jour par save()/erase(). loadAllMeta() est
// appelee a CHAQUE tour de boucle principale (ScriptRunner::update()) pour
// detecter les declencheurs -- or readBlob() transfere tout le blob NVS, y
// compris le bytecode des 6 scripts (plusieurs Ko), alors que l'executeur n'a
// besoin que des en-tetes. Sans ce cache, ce transfert NVS complet a chaque
// tour a ete mesure a l'origine de decrochages de boucle jusqu'a 350 ms
// (bien au-dela du seuil de 100 ms), constate le 26 sept. 2026 -- ce que le
// commentaire de loadAllMeta() dans ScriptStore.h promettait d'eviter sans
// que l'implementation ne le fasse reellement.
Meta g_metaCache[MAX_SCRIPTS];
bool g_metaCacheValid = false;

void refreshMetaCacheFromBlob() {
    Persisted* blob = static_cast<Persisted*>(malloc(sizeof(Persisted)));
    if (!blob) return;  // reessaiera au prochain appel, cache pas marque valide
    if (readBlob(*blob)) {
        for (uint8_t i = 0U; i < MAX_SCRIPTS; ++i) g_metaCache[i] = blob->slots[i].meta;
    } else {
        // Pas de blob (premier demarrage) : magasin vide, pas une erreur.
        for (uint8_t i = 0U; i < MAX_SCRIPTS; ++i) g_metaCache[i] = Meta();
    }
    free(blob);
    g_metaCacheValid = true;
}

} // namespace

bool save(uint8_t index, const Meta& meta, const uint8_t* code,
          const char*& reason) {
    reason = "";
    if (index >= MAX_SCRIPTS) { reason = "emplacement invalide"; return false; }
    if (meta.codeSize == 0U || meta.codeSize > MAX_BYTECODE || !code) {
        reason = "programme vide ou trop long";
        return false;
    }

    // La verification a lieu AVANT l'ecriture : un programme refuse ne doit
    // pas remplacer un programme qui marchait.
    const AquaLook::Domain::ScriptAbort verdict =
        AquaLook::Domain::validateScriptProgram(
            AquaLook::Domain::ScriptProgram(code, meta.codeSize));
    if (verdict != AquaLook::Domain::ScriptAbort::NONE) {
        reason = AquaLook::Domain::scriptAbortName(verdict);
        EventLog::log(LOG_WARN, "Scripts: programme %u refuse (%s)",
                      (unsigned)index, reason);
        return false;
    }

    Persisted* blob = static_cast<Persisted*>(malloc(sizeof(Persisted)));
    if (!blob) { reason = "memoire insuffisante"; return false; }
    if (!readBlob(*blob)) blankBlob(*blob);

    blob->slots[index].meta = meta;
    blob->slots[index].meta.used = true;
    memset(blob->slots[index].code, 0, MAX_BYTECODE);
    memcpy(blob->slots[index].code, code, meta.codeSize);

    const Meta savedMeta = blob->slots[index].meta;
    const bool ok = writeBlob(*blob);
    free(blob);
    if (ok) {
        // Ecrit directement le cache plutot que de le marquer invalide : une
        // relecture NVS ici annulerait l'interet du cache pour le prochain
        // declenchement, potentiellement dans la meme seconde.
        g_metaCache[index] = savedMeta;
        g_metaCacheValid = true;
        EventLog::log(LOG_INFO, "Scripts: programme %u enregistre (%u octets)",
                      (unsigned)index, (unsigned)meta.codeSize);
    } else {
        reason = "ecriture impossible";
    }
    return ok;
}

bool load(uint8_t index, Meta& meta, uint8_t* code, uint16_t capacity) {
    if (index >= MAX_SCRIPTS) return false;
    Persisted* blob = static_cast<Persisted*>(malloc(sizeof(Persisted)));
    if (!blob) return false;
    bool ok = false;
    if (readBlob(*blob) && blob->slots[index].meta.used) {
        meta = blob->slots[index].meta;
        const uint16_t n = meta.codeSize < capacity ? meta.codeSize : capacity;
        if (code && n > 0U) memcpy(code, blob->slots[index].code, n);
        ok = true;
    }
    free(blob);
    return ok;
}

bool erase(uint8_t index) {
    if (index >= MAX_SCRIPTS) return false;
    Persisted* blob = static_cast<Persisted*>(malloc(sizeof(Persisted)));
    if (!blob) return false;
    bool ok = false;
    if (readBlob(*blob)) {
        memset(&blob->slots[index], 0, sizeof(Slot));
        ok = writeBlob(*blob);
    }
    free(blob);
    if (ok) {
        g_metaCache[index] = Meta();
        g_metaCacheValid = true;
    }
    return ok;
}

void loadAllMeta(Meta* metas, uint8_t capacity) {
    if (!metas) return;
    for (uint8_t i = 0U; i < capacity; ++i) metas[i] = Meta();

    if (!g_metaCacheValid) refreshMetaCacheFromBlob();
    const uint8_t n = capacity < MAX_SCRIPTS ? capacity : MAX_SCRIPTS;
    for (uint8_t i = 0U; i < n; ++i) metas[i] = g_metaCache[i];
}

} // namespace ScriptStore
