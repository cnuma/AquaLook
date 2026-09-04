#include "IoExpanderConfig.h"

#include <Preferences.h>
#include <cstring>
#include "EventLog.h"

namespace IoExpander {

namespace {
constexpr char NVS_NAMESPACE[] = "aq_io";
constexpr char NVS_KEY[]       = "cfg";
constexpr uint32_t NVS_MAGIC   = 0x494F4558UL;  // "IOEX"
constexpr uint16_t SCHEMA      = 1U;

// Meme algorithme que ConfigManager, recopie ici pour rester autonome.
uint32_t crc32Bytes(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
    return ~crc;
}

// Bloc persiste : entete + config + CRC. Le CRC couvre tout ce qui precede.
struct Persisted {
    uint32_t magic;
    uint16_t schema;
    uint16_t size;      // sizeof(Persisted), garde-fou de version
    Config   config;
    uint32_t crc32;
};
}  // namespace

Config::Config()
    : schemaVersion(SCHEMA), enabled(0), pollSeconds(5) {}

const char* directionName(uint8_t direction) {
    return direction == DIR_OUTPUT ? "sortie" : "entree";
}

const char* roleName(uint8_t role) {
    switch (role) {
        case ROLE_VALVE_PRESENCE: return "presence-vanne";
        case ROLE_TOR_INPUT:      return "entree-tor";
        case ROLE_LIGHTING:       return "eclairage";
        case ROLE_VENTILATION:    return "ventilation";
        case ROLE_TOR_OUTPUT:     return "sortie-tor";
        default:                  return "aucun";
    }
}

bool roleIsInput(uint8_t role) {
    return role == ROLE_VALVE_PRESENCE || role == ROLE_TOR_INPUT;
}

bool roleIsOutput(uint8_t role) {
    return role == ROLE_LIGHTING || role == ROLE_VENTILATION || role == ROLE_TOR_OUTPUT;
}

bool validAddress(uint8_t addr) {
    return addr >= 0x20 && addr <= 0x27;
}

bool validBoard(const Board& board) {
    return board.enabled == 0 || validAddress(board.i2cAddress);
}

bool validBinding(const Config& cfg, uint8_t bindingIndex, uint8_t nbZones) {
    if (bindingIndex >= MAX_BINDINGS) return false;
    const Binding& b = cfg.bindings[bindingIndex];
    if (!b.enabled) return true;  // un binding desactive est toujours "valide"

    if (b.boardIndex >= MAX_BOARDS) return false;
    if (!cfg.boards[b.boardIndex].enabled) return false;
    if (b.pin >= PINS_PER_BOARD) return false;
    if (b.direction > DIR_OUTPUT) return false;

    // Direction et role doivent concorder.
    if (b.direction == DIR_INPUT && !roleIsInput(b.role)) return false;
    if (b.direction == DIR_OUTPUT && !roleIsOutput(b.role)) return false;

    // La presence de vanne exige une zone reelle ; les autres roles peuvent
    // s'en passer (capteur global, eclairage non lie a une zone).
    if (b.role == ROLE_VALVE_PRESENCE) {
        if (b.zone == ZONE_NONE || b.zone >= nbZones) return false;
    } else if (b.zone != ZONE_NONE && b.zone >= MAX_ZONES) {
        return false;
    }
    if (b.activeLevel > 1) return false;
    return true;
}

Config makeSafeDefault() {
    Config cfg;              // constructeurs : couche desactivee, tout vide
    cfg.schemaVersion = SCHEMA;
    cfg.enabled = 0;
    cfg.pollSeconds = 5;
    return cfg;
}

bool load(Config& out) {
    out = makeSafeDefault();

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        return false;  // namespace jamais ecrit : defauts en place
    }
    const size_t len = prefs.getBytesLength(NVS_KEY);
    // Garde de taille AVANT toute lecture : une taille inattendue (schema futur
    // ou bloc corrompu) laisse les defauts, jamais un ecrasement. Lecon de la
    // migration NVS du 3 septembre 2026.
    if (len != sizeof(Persisted)) {
        prefs.end();
        if (len != 0) {
            EventLog::log(LOG_WARN, "IoExpander: taille NVS invalide (%u/%u)",
                          (unsigned)len, (unsigned)sizeof(Persisted));
        }
        return false;
    }

    Persisted blob;
    const size_t read = prefs.getBytes(NVS_KEY, &blob, sizeof(blob));
    prefs.end();
    if (read != sizeof(blob) || blob.magic != NVS_MAGIC ||
        blob.schema != SCHEMA || blob.size != sizeof(Persisted)) {
        EventLog::log(LOG_WARN, "IoExpander: entete NVS invalide");
        return false;
    }
    const uint32_t expected =
        crc32Bytes(reinterpret_cast<const uint8_t*>(&blob), offsetof(Persisted, crc32));
    if (expected != blob.crc32) {
        EventLog::log(LOG_ERROR, "IoExpander: CRC NVS invalide");
        return false;
    }

    out = blob.config;
    return true;
}

bool save(const Config& cfg) {
    Persisted blob;
    std::memset(&blob, 0, sizeof(blob));
    blob.magic  = NVS_MAGIC;
    blob.schema = SCHEMA;
    blob.size   = sizeof(Persisted);
    blob.config = cfg;
    blob.config.schemaVersion = SCHEMA;
    blob.crc32 =
        crc32Bytes(reinterpret_cast<const uint8_t*>(&blob), offsetof(Persisted, crc32));

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        EventLog::log(LOG_ERROR, "IoExpander: ouverture NVS ecriture impossible");
        return false;
    }
    const size_t written = prefs.putBytes(NVS_KEY, &blob, sizeof(blob));
    prefs.end();
    if (written != sizeof(blob)) {
        EventLog::log(LOG_ERROR, "IoExpander: ecriture NVS incomplete (%u/%u)",
                      (unsigned)written, (unsigned)sizeof(blob));
        return false;
    }
    return true;
}

} // namespace IoExpander
