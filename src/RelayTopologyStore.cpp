#include "RelayTopologyStore.h"

#include <Preferences.h>
#include <stddef.h>
#include <string.h>

#include "EventLog.h"

namespace RelayTopologyStore {

namespace {

constexpr const char* NVS_NS  = "aq_topo";
constexpr const char* NVS_KEY = "cfg";
constexpr uint32_t NVS_MAGIC  = 0x52544F50UL;  // "RTOP"
constexpr uint16_t SCHEMA     = 2U;  // 2 : ajout du transport aux cartes

uint32_t crc32Bytes(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8U; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return ~crc;
}

// --- Schema 1 : avant l ajout du transport ---------------------------
// Conserve tel quel pour MIGRER une topologie deja enregistree au lieu de la
// jeter. Elargir la garde de longueur est le point ou l on se brule : une
// garde restee stricte efface silencieusement la configuration.
struct RelayBoardConfigV1 {
    bool    enabled;
    uint8_t controller;
    uint8_t i2cAddress;
    uint8_t channelCount;
    uint8_t logic;
};

struct RelayTopologyConfigV1 {
    RelayBoardConfigV1 boards[RelayTopology::MAX_RELAY_BOARDS];
    RelayTopology::RelayAssignment assignments[RelayTopology::MAX_RELAY_ASSIGNMENTS];
};

// Bloc persiste : entete + topologie + CRC. Le CRC couvre tout ce qui precede.
struct Persisted {
    uint32_t magic;
    uint16_t schema;
    uint16_t size;  // sizeof(Persisted), garde-fou de version
    RelayTopology::RelayTopologyConfig topology;
    uint32_t crc32;
};

// Une topologie n'est retenue que si elle tient debout : au moins une carte
// activee ET valide, aucune carte activee incoherente, aucun doublon de voie.
// Au moindre doute on refuse -- l'appelant laisse alors la topologie vide,
// et le module annonce qu'il n'est pas cable plutot que de piloter au juge.
bool coherent(const RelayTopology::RelayTopologyConfig& topology, uint8_t nbZones) {
    uint8_t validBoards = 0U;
    for (uint8_t b = 0U; b < RelayTopology::MAX_RELAY_BOARDS; ++b) {
        const RelayTopology::RelayBoardConfig& board = topology.boards[b];
        if (!board.enabled) continue;
        if (!RelayTopology::validateBoard(board)) return false;
        validBoards++;
    }
    if (validBoards == 0U) return false;
    if (RelayTopology::hasDuplicateAssignments(topology)) return false;
    if (RelayTopology::hasDuplicateMappings(topology, nbZones)) return false;
    return true;
}

struct PersistedV1 {
    uint32_t magic;
    uint16_t schema;
    uint16_t size;
    RelayTopologyConfigV1 topology;
    uint32_t crc32;
};

// Recopie champ a champ : le transport prend sa valeur par defaut (I2C local),
// qui est ce que decrivaient toutes les topologies du schema 1.
void migrateV1(const RelayTopologyConfigV1& src, RelayTopology::RelayTopologyConfig& dst) {
    RelayTopology::clear(dst);
    for (uint8_t b = 0U; b < RelayTopology::MAX_RELAY_BOARDS; ++b) {
        dst.boards[b].enabled      = src.boards[b].enabled;
        dst.boards[b].controller   = src.boards[b].controller;
        dst.boards[b].i2cAddress   = src.boards[b].i2cAddress;
        dst.boards[b].channelCount = src.boards[b].channelCount;
        dst.boards[b].logic        = src.boards[b].logic;
        dst.boards[b].transport    = RelayTopology::TRANSPORT_I2C_LOCAL;
        dst.boards[b].node         = 0U;
    }
    for (uint8_t a = 0U; a < RelayTopology::MAX_RELAY_ASSIGNMENTS; ++a) {
        dst.assignments[a] = src.assignments[a];
    }
}

}  // namespace

bool load(RelayTopology::RelayTopologyConfig& topology, uint8_t nbZones) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) return false;  // namespace jamais ecrit

    const size_t len = prefs.getBytesLength(NVS_KEY);
    if (len == 0U) { prefs.end(); return false; }
    // Garde de longueur ELARGIE : accepter aussi la taille du schema
    // precedent, sinon la migration n'a jamais lieu et la configuration
    // disparait en silence au premier changement de modele.
    const bool isV1 = (len == sizeof(PersistedV1));
    if (len != sizeof(Persisted) && !isV1) {
        prefs.end();
        EventLog::log(LOG_WARN, "Topo: taille NVS %u inattendue (%u ou %u), ignoree",
                      (unsigned)len, (unsigned)sizeof(Persisted),
                      (unsigned)sizeof(PersistedV1));
        return false;
    }

    if (isV1) {
        PersistedV1 old;
        const size_t readOld = prefs.getBytes(NVS_KEY, &old, sizeof(old));
        prefs.end();
        if (readOld != sizeof(old) || old.magic != NVS_MAGIC || old.schema != 1U) {
            EventLog::log(LOG_WARN, "Topo: entete NVS schema 1 invalide, ignoree");
            return false;
        }
        const uint32_t expectedOld = crc32Bytes(
            reinterpret_cast<const uint8_t*>(&old), offsetof(PersistedV1, crc32));
        if (expectedOld != old.crc32) {
            EventLog::log(LOG_WARN, "Topo: CRC NVS schema 1 invalide, ignoree");
            return false;
        }
        RelayTopology::RelayTopologyConfig migrated;
        migrateV1(old.topology, migrated);
        if (!coherent(migrated, nbZones)) {
            EventLog::log(LOG_WARN, "Topo: schema 1 incoherent apres migration, ignoree");
            return false;
        }
        topology = migrated;
        EventLog::log(LOG_INFO, "Topo: migree du schema 1 vers 2 (transport = I2C local)");
        return true;
    }

    Persisted blob;
    const size_t read = prefs.getBytes(NVS_KEY, &blob, sizeof(blob));
    prefs.end();

    if (read != sizeof(blob) || blob.magic != NVS_MAGIC ||
        blob.schema != SCHEMA || blob.size != sizeof(Persisted)) {
        EventLog::log(LOG_WARN, "Topo: entete NVS invalide, ignoree");
        return false;
    }

    const uint32_t expected = crc32Bytes(
        reinterpret_cast<const uint8_t*>(&blob), offsetof(Persisted, crc32));
    if (expected != blob.crc32) {
        EventLog::log(LOG_WARN, "Topo: CRC NVS invalide, ignoree");
        return false;
    }

    if (!coherent(blob.topology, nbZones)) {
        EventLog::log(LOG_WARN, "Topo: contenu NVS incoherent, ignoree");
        return false;
    }

    topology = blob.topology;
    return true;
}

bool save(const RelayTopology::RelayTopologyConfig& topology, uint8_t nbZones) {
    if (!coherent(topology, nbZones)) {
        EventLog::log(LOG_ERROR, "Topo: refus d'enregistrer, topologie incoherente");
        return false;
    }

    Persisted blob;
    memset(&blob, 0, sizeof(blob));  // padding deterministe pour le CRC
    blob.magic    = NVS_MAGIC;
    blob.schema   = SCHEMA;
    blob.size     = static_cast<uint16_t>(sizeof(Persisted));
    blob.topology = topology;
    blob.crc32    = crc32Bytes(
        reinterpret_cast<const uint8_t*>(&blob), offsetof(Persisted, crc32));

    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) {
        EventLog::log(LOG_ERROR, "Topo: ouverture NVS impossible");
        return false;
    }
    const size_t written = prefs.putBytes(NVS_KEY, &blob, sizeof(blob));
    prefs.end();

    const bool ok = (written == sizeof(blob));
    EventLog::log(ok ? LOG_INFO : LOG_ERROR,
                  ok ? "Topo: topologie enregistree en NVS"
                     : "Topo: echec d'ecriture NVS");
    return ok;
}

bool clear() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    const bool ok = prefs.remove(NVS_KEY);
    prefs.end();
    EventLog::log(LOG_INFO, "Topo: topologie persistee effacee, retour legacy");
    return ok;
}

bool exists() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) return false;
    const size_t len = prefs.getBytesLength(NVS_KEY);
    prefs.end();
    return len == sizeof(Persisted);
}

}  // namespace RelayTopologyStore
