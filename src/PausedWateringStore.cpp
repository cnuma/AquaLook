#include "PausedWateringStore.h"

#include <Preferences.h>

#include "EventLog.h"

namespace PausedWateringStore {
namespace {

constexpr const char* NVS_NAMESPACE = "aqualook";
constexpr const char* NVS_KEY = "aq_pause";
constexpr uint32_t NVS_MAGIC = 0x50415553UL;  // "PAUS"
constexpr uint16_t SCHEMA = 1U;

struct Persisted {
    uint32_t magic;
    uint16_t schema;
    uint16_t size;
    uint8_t count;
    uint8_t reserved[3];
    Entry entries[MAX_ZONES];
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

} // namespace

bool save(const Entry* entries, uint8_t count) {
    if (count > MAX_ZONES) count = MAX_ZONES;

    Persisted blob;
    memset(&blob, 0, sizeof(blob));
    blob.magic = NVS_MAGIC;
    blob.schema = SCHEMA;
    blob.size = sizeof(Persisted);
    blob.count = count;
    for (uint8_t i = 0U; i < count && entries; ++i) blob.entries[i] = entries[i];
    blob.crc32 = crc32Bytes(reinterpret_cast<const uint8_t*>(&blob),
                            offsetof(Persisted, crc32));

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        EventLog::log(LOG_ERROR, "Pause: ouverture NVS impossible");
        return false;
    }
    const size_t written = prefs.putBytes(NVS_KEY, &blob, sizeof(blob));
    prefs.end();

    if (written != sizeof(blob)) {
        EventLog::log(LOG_ERROR, "Pause: ecriture NVS incomplete");
        return false;
    }
    return true;
}

uint8_t load(Entry* entries, uint8_t capacity) {
    if (!entries || capacity == 0U) return 0U;

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return 0U;
    const size_t len = prefs.getBytesLength(NVS_KEY);
    if (len != sizeof(Persisted)) {
        prefs.end();
        // Rien d'enregistre est le cas NORMAL : ne pas en faire un incident.
        if (len != 0U) {
            EventLog::log(LOG_WARN, "Pause: taille NVS %u inattendue, ignoree",
                          (unsigned)len);
        }
        return 0U;
    }

    Persisted blob;
    const size_t read = prefs.getBytes(NVS_KEY, &blob, sizeof(blob));
    prefs.end();

    if (read != sizeof(blob) || blob.magic != NVS_MAGIC ||
        blob.schema != SCHEMA || blob.size != sizeof(Persisted)) {
        EventLog::log(LOG_WARN, "Pause: entete NVS invalide, ignoree");
        return 0U;
    }
    if (crc32Bytes(reinterpret_cast<const uint8_t*>(&blob),
                   offsetof(Persisted, crc32)) != blob.crc32) {
        EventLog::log(LOG_WARN, "Pause: CRC NVS invalide, ignoree");
        return 0U;
    }

    const uint8_t n = blob.count < capacity ? blob.count : capacity;
    for (uint8_t i = 0U; i < n; ++i) entries[i] = blob.entries[i];
    return n;
}

void clear() { save(nullptr, 0U); }

} // namespace PausedWateringStore
