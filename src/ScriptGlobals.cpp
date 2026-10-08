#include "ScriptGlobals.h"

#include <Preferences.h>

#include "EventLog.h"

namespace ScriptGlobals {
namespace {

constexpr const char* NVS_NAMESPACE = "aqlvars";
constexpr const char* KEY_VALUES = "v";
constexpr const char* KEY_NAMES = "n";
constexpr uint32_t MAGIC_VALUES = 0x52415647UL;   // "GVAR"
constexpr uint32_t MAGIC_NAMES = 0x4D414E47UL;    // "GNAM"
constexpr uint16_t VERSION = 1U;

struct ValuesBlob {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    int32_t  v[COUNT];
    uint32_t crc32;
};

struct NamesBlob {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    char     n[COUNT][NAME_LEN_MAX + 1U];
    uint32_t crc32;
};

int32_t g_values[COUNT] = {};
char    g_names[COUNT][NAME_LEN_MAX + 1U] = {};
bool     g_dirty = false;
uint32_t g_lastSaveMs = 0U;
bool     g_started = false;   // begin() passe : valeurs et noms relus
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

// Meme CRC32 que ConfigManager (polynome reflechi 0xEDB88320).
uint32_t crc32Bytes(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0U; b < 8U; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return ~crc;
}

template <typename Blob>
uint32_t blobCrc(const Blob& b) {
    return crc32Bytes(reinterpret_cast<const uint8_t*>(&b), offsetof(Blob, crc32));
}

bool saveValues() {
    ValuesBlob b = {};
    b.magic = MAGIC_VALUES;
    b.version = VERSION;
    b.count = COUNT;
    portENTER_CRITICAL(&g_mux);
    for (uint8_t i = 0U; i < COUNT; ++i) b.v[i] = g_values[i];
    g_dirty = false;
    portEXIT_CRITICAL(&g_mux);
    b.crc32 = blobCrc(b);

    Preferences prefs;
    bool ok = prefs.begin(NVS_NAMESPACE, false) &&
              prefs.putBytes(KEY_VALUES, &b, sizeof(b)) == sizeof(b);
    prefs.end();
    if (!ok) {
        // Retenter au prochain intervalle plutot que perdre la modification.
        portENTER_CRITICAL(&g_mux);
        g_dirty = true;
        portEXIT_CRITICAL(&g_mux);
        EventLog::log(LOG_WARN, "[GVAR] ecriture NVS des variables echouee, nouvel essai plus tard");
    }
    return ok;
}

}  // namespace

void begin() {
    g_started = true;
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        // Espace absent : premier demarrage avec cette fonction.
        EventLog::log(LOG_INFO, "[GVAR] aucune variable enregistree : valeurs a 0");
        return;
    }
    ValuesBlob vb = {};
    const size_t vn = prefs.getBytes(KEY_VALUES, &vb, sizeof(vb));
    NamesBlob nb = {};
    const size_t nn = prefs.getBytes(KEY_NAMES, &nb, sizeof(nb));
    prefs.end();

    const bool valuesOk = vn == sizeof(vb) && vb.magic == MAGIC_VALUES &&
                          vb.version == VERSION && vb.count == COUNT && vb.crc32 == blobCrc(vb);
    const bool namesOk = nn == sizeof(nb) && nb.magic == MAGIC_NAMES &&
                         nb.version == VERSION && nb.count == COUNT && nb.crc32 == blobCrc(nb);
    portENTER_CRITICAL(&g_mux);
    if (valuesOk) for (uint8_t i = 0U; i < COUNT; ++i) g_values[i] = vb.v[i];
    if (namesOk) {
        for (uint8_t i = 0U; i < COUNT; ++i) {
            memcpy(g_names[i], nb.n[i], NAME_LEN_MAX + 1U);
            g_names[i][NAME_LEN_MAX] = '\0';
        }
    }
    portEXIT_CRITICAL(&g_mux);

    if (vn != 0U && !valuesOk) {
        EventLog::log(LOG_WARN, "[GVAR] bloc des valeurs illisible (taille/version/CRC) : valeurs a 0");
    }
    if (nn != 0U && !namesOk) {
        EventLog::log(LOG_WARN, "[GVAR] bloc des noms illisible : noms vides");
    }
    EventLog::log(LOG_INFO, "[GVAR] variables relues : valeurs %s, noms %s",
                  valuesOk ? "ok" : "a 0", namesOk ? "ok" : "vides");
}

bool started() { return g_started; }

int32_t get(uint8_t i) {
    if (i >= COUNT) return 0;
    portENTER_CRITICAL(&g_mux);
    const int32_t v = g_values[i];
    portEXIT_CRITICAL(&g_mux);
    return v;
}

bool set(uint8_t i, int32_t value) {
    if (i >= COUNT) return false;
    portENTER_CRITICAL(&g_mux);
    if (g_values[i] != value) {
        g_values[i] = value;
        g_dirty = true;
    }
    portEXIT_CRITICAL(&g_mux);
    return true;
}

void name(uint8_t i, char* out, size_t n) {
    if (!out || n == 0U) return;
    out[0] = '\0';
    if (i >= COUNT) return;
    portENTER_CRITICAL(&g_mux);
    strlcpy(out, g_names[i], n);
    portEXIT_CRITICAL(&g_mux);
}

bool setNames(const char* const names[COUNT]) {
    NamesBlob b = {};
    b.magic = MAGIC_NAMES;
    b.version = VERSION;
    b.count = COUNT;
    for (uint8_t i = 0U; i < COUNT; ++i) {
        strlcpy(b.n[i], names[i] ? names[i] : "", sizeof(b.n[i]));
    }
    b.crc32 = blobCrc(b);

    Preferences prefs;
    const bool ok = prefs.begin(NVS_NAMESPACE, false) &&
                    prefs.putBytes(KEY_NAMES, &b, sizeof(b)) == sizeof(b);
    prefs.end();
    if (!ok) {
        EventLog::log(LOG_WARN, "[GVAR] ecriture NVS des noms echouee : noms inchanges");
        return false;
    }
    portENTER_CRITICAL(&g_mux);
    for (uint8_t i = 0U; i < COUNT; ++i) memcpy(g_names[i], b.n[i], NAME_LEN_MAX + 1U);
    portEXIT_CRITICAL(&g_mux);
    return true;
}

void update(uint32_t nowMs) {
    portENTER_CRITICAL(&g_mux);
    const bool dirty = g_dirty;
    portEXIT_CRITICAL(&g_mux);
    if (!dirty || static_cast<uint32_t>(nowMs - g_lastSaveMs) < SAVE_INTERVAL_MS) return;
    g_lastSaveMs = nowMs;
    saveValues();
}

void flush() {
    portENTER_CRITICAL(&g_mux);
    const bool dirty = g_dirty;
    portEXIT_CRITICAL(&g_mux);
    if (dirty) saveValues();
}

}  // namespace ScriptGlobals
