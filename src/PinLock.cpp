#include "PinLock.h"

#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <stddef.h>
#include <string.h>

#include "EventLog.h"

namespace PinLock {
namespace {

constexpr const char* NVS_NAMESPACE = "aqlsec";
constexpr const char* KEY_PIN = "pin";
constexpr const char* KEY_FAILS = "fails";
constexpr uint32_t MAGIC = 0x4E495041UL;  // "APIN"
constexpr uint8_t VERSION = 1;
// Assez pour ralentir une recherche exhaustive sur un bloc NVS derobe sans
// rendre la saisie penible : mesure a la pose (ligne [SEC] ... ms). Range
// dans le bloc, donc modifiable plus tard sans casser les PIN deja poses.
constexpr uint32_t ITERATIONS = 4000;
constexpr uint8_t SALT_LEN = 16;
constexpr uint8_t HASH_LEN = 32;
constexpr uint8_t FREE_TRIES = 5;
constexpr uint32_t FIRST_LOCKOUT_S = 30;
constexpr uint32_t MAX_LOCKOUT_S = 15UL * 60UL;

struct PinBlob {
    uint32_t magic;
    uint8_t version;
    uint8_t digits;
    uint16_t reserved;
    uint32_t iterations;
    uint8_t salt[SALT_LEN];
    uint8_t hash[HASH_LEN];
    uint32_t crc32;
};

PinBlob g_blob = {};
bool g_hasPin = false;
uint8_t g_failures = 0;
uint32_t g_lastFailureMs = 0;
uint32_t g_unlockedAtMs = 0;
bool g_unlocked = false;

uint32_t crc32Bytes(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return ~crc;
}

uint32_t blobCrc(const PinBlob& b) {
    return crc32Bytes(reinterpret_cast<const uint8_t*>(&b), offsetof(PinBlob, crc32));
}

bool validDigits(const char* digits) {
    if (!digits) return false;
    const size_t n = strlen(digits);
    if (n < MIN_DIGITS || n > MAX_DIGITS) return false;
    for (size_t i = 0; i < n; ++i) {
        if (digits[i] < '0' || digits[i] > '9') return false;
    }
    return true;
}

bool derive(const char* digits, const uint8_t* salt, uint32_t iterations, uint8_t* out) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    bool ok = info && mbedtls_md_setup(&ctx, info, 1) == 0 &&
              mbedtls_pkcs5_pbkdf2_hmac(&ctx, reinterpret_cast<const unsigned char*>(digits),
                                        strlen(digits), salt, SALT_LEN, iterations,
                                        HASH_LEN, out) == 0;
    mbedtls_md_free(&ctx);
    return ok;
}

bool equalsConstantTime(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    return diff == 0;
}

void storeFailures() {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putUChar(KEY_FAILS, g_failures);
        prefs.end();
    }
}

uint32_t lockoutSecondsFor(uint8_t failures) {
    if (failures < FREE_TRIES) return 0;
    uint32_t s = FIRST_LOCKOUT_S;
    for (uint8_t i = FREE_TRIES; i < failures && s < MAX_LOCKOUT_S; ++i) s *= 2U;
    return s > MAX_LOCKOUT_S ? MAX_LOCKOUT_S : s;
}

}  // namespace

void begin() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        // Espace absent : jamais ecrit, donc aucun PIN. Cas nominal d'un
        // module mis a jour depuis un firmware anterieur au lot E.
        EventLog::log(LOG_INFO, "[SEC] PIN: aucun (espace aqlsec vide)");
        return;
    }
    PinBlob b = {};
    const size_t n = prefs.getBytes(KEY_PIN, &b, sizeof(b));
    g_failures = prefs.getUChar(KEY_FAILS, 0);
    prefs.end();

    if (n == 0) {
        EventLog::log(LOG_INFO, "[SEC] PIN: aucun");
    } else if (n == sizeof(b) && b.magic == MAGIC && b.version == VERSION &&
               b.digits >= MIN_DIGITS && b.digits <= MAX_DIGITS &&
               b.iterations > 0 && b.crc32 == blobCrc(b)) {
        g_blob = b;
        g_hasPin = true;
        EventLog::log(LOG_INFO, "[SEC] PIN: pose (%u chiffres), echecs=%u",
                      static_cast<unsigned>(b.digits), static_cast<unsigned>(g_failures));
    } else {
        EventLog::log(LOG_WARN, "[SEC] PIN: bloc illisible (%u octets), ignore -> pas de PIN",
                      static_cast<unsigned>(n));
    }
    // L'attente eventuelle repart du demarrage : un redemarrage ne la
    // raccourcit pas au-dela de ce qu'elle aurait dure.
    g_lastFailureMs = millis();
}

bool hasPin() { return g_hasPin; }

bool isUnlocked() {
    if (!g_hasPin) return true;
    if (!g_unlocked) return false;
    if (millis() - g_unlockedAtMs >= UNLOCK_WINDOW_MS) {
        g_unlocked = false;
        return false;
    }
    return true;
}

uint32_t lockoutRemainingSec() {
    const uint32_t lockS = lockoutSecondsFor(g_failures);
    if (lockS == 0) return 0;
    const uint32_t elapsedS = (millis() - g_lastFailureMs) / 1000UL;
    return elapsedS >= lockS ? 0 : lockS - elapsedS;
}

uint8_t failures() { return g_failures; }

Result verify(const char* digits) {
    if (!g_hasPin) return Result::NO_PIN;
    if (lockoutRemainingSec() > 0) return Result::LOCKED;

    uint8_t h[HASH_LEN];
    const bool ok = validDigits(digits) &&
                    strlen(digits) == g_blob.digits &&
                    derive(digits, g_blob.salt, g_blob.iterations, h) &&
                    equalsConstantTime(h, g_blob.hash, HASH_LEN);
    memset(h, 0, sizeof(h));
    if (ok) {
        if (g_failures != 0) {
            g_failures = 0;
            storeFailures();
        }
        g_unlocked = true;
        g_unlockedAtMs = millis();
        EventLog::log(LOG_INFO, "[SEC] PIN: deverrouille");
        return Result::OK;
    }
    if (g_failures < 255) ++g_failures;
    g_lastFailureMs = millis();
    storeFailures();
    EventLog::log(LOG_WARN, "[SEC] PIN: echec %u, attente %lu s",
                  static_cast<unsigned>(g_failures),
                  static_cast<unsigned long>(lockoutSecondsFor(g_failures)));
    return Result::WRONG;
}

bool set(const char* digits) {
    if (!validDigits(digits)) return false;
    if (g_hasPin && !isUnlocked()) {
        EventLog::log(LOG_WARN, "[SEC] PIN: changement refuse (verrouille)");
        return false;
    }
    PinBlob b = {};
    b.magic = MAGIC;
    b.version = VERSION;
    b.digits = static_cast<uint8_t>(strlen(digits));
    b.iterations = ITERATIONS;
    esp_fill_random(b.salt, SALT_LEN);
    const uint32_t t0 = millis();
    if (!derive(digits, b.salt, b.iterations, b.hash)) {
        EventLog::log(LOG_ERROR, "[SEC] PIN: derivation impossible");
        return false;
    }
    const uint32_t dt = millis() - t0;
    b.crc32 = blobCrc(b);

    Preferences prefs;
    bool ok = prefs.begin(NVS_NAMESPACE, false) &&
              prefs.putBytes(KEY_PIN, &b, sizeof(b)) == sizeof(b);
    if (ok) {
        prefs.putUChar(KEY_FAILS, 0);
    }
    prefs.end();
    if (!ok) {
        EventLog::log(LOG_ERROR, "[SEC] PIN: ecriture NVS impossible");
        return false;
    }
    g_blob = b;
    g_hasPin = true;
    g_failures = 0;
    g_unlocked = true;
    g_unlockedAtMs = millis();
    EventLog::log(LOG_INFO, "[SEC] PIN: pose (%u chiffres, derivation %lu ms)",
                  static_cast<unsigned>(b.digits), static_cast<unsigned long>(dt));
    return true;
}

bool remove() {
    if (!g_hasPin || !isUnlocked()) return false;
    clear();
    return true;
}

void clear() {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.remove(KEY_PIN);
        prefs.remove(KEY_FAILS);
        prefs.end();
    }
    g_blob = {};
    g_hasPin = false;
    g_failures = 0;
    g_unlocked = false;
    EventLog::log(LOG_WARN, "[SEC] PIN: efface");
}

void lock() { g_unlocked = false; }

}  // namespace PinLock
