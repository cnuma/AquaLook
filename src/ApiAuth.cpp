#include "ApiAuth.h"

#include <Preferences.h>
#include <mbedtls/md.h>
#include <string.h>

#include "EventLog.h"

namespace ApiAuth {
namespace {

constexpr const char* NVS_NAMESPACE = "aqualook";
constexpr const char* KEY_SECRET = "aq_apikey";
constexpr const char* KEY_NONCE = "aq_apinonce";
constexpr uint8_t MAX_SECRET = 64;

uint32_t g_lastNonce = 0U;
bool g_nonceLoaded = false;

bool readSecret(char* out, size_t capacity) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return false;
    const size_t n = prefs.getString(KEY_SECRET, out, capacity);
    prefs.end();
    return n > 0U && out[0] != '\0';
}

void loadNonce() {
    if (g_nonceLoaded) return;
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, true)) {
        g_lastNonce = prefs.getUInt(KEY_NONCE, 0U);
        prefs.end();
    }
    g_nonceLoaded = true;
}

void storeNonce(uint32_t nonce) {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putUInt(KEY_NONCE, nonce);
        prefs.end();
    }
    g_lastNonce = nonce;
}

// Comparaison a temps constant. Comparer avec strcmp laisserait fuir, par le
// temps de reponse, le nombre d'octets corrects en tete -- de quoi
// reconstituer une signature octet par octet.
bool equalsConstantTime(const char* a, const char* b, size_t len) {
    uint8_t diff = 0U;
    for (size_t i = 0; i < len; ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0U;
}

bool computeHmacHex(const char* secret, const String& message, char* outHex) {
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info) return false;

    uint8_t digest[32];
    if (mbedtls_md_hmac(info,
                        reinterpret_cast<const uint8_t*>(secret), strlen(secret),
                        reinterpret_cast<const uint8_t*>(message.c_str()),
                        message.length(),
                        digest) != 0) {
        return false;
    }
    // HEX est un macro d Arduino (Print.h) : tout identifiant portant ce nom
    // se fait remplacer par 16 avant compilation.
    static const char HEXDIGITS[] = "0123456789abcdef";
    for (uint8_t i = 0U; i < 32U; ++i) {
        outHex[i * 2U] = HEXDIGITS[(digest[i] >> 4) & 0x0F];
        outHex[i * 2U + 1U] = HEXDIGITS[digest[i] & 0x0F];
    }
    outHex[64] = '\0';
    return true;
}

} // namespace

bool hasSecret() {
    char secret[MAX_SECRET + 1] = {};
    return readSecret(secret, sizeof(secret));
}

bool setSecret(const char* currentSecret, const char* newSecret) {
    if (!newSecret || strlen(newSecret) < 12U || strlen(newSecret) > MAX_SECRET) {
        // Douze caracteres au moins : un secret court se devine, et il n'y a
        // aucune limitation de debit sur un reseau local.
        return false;
    }

    char existing[MAX_SECRET + 1] = {};
    if (readSecret(existing, sizeof(existing))) {
        if (!currentSecret || strlen(currentSecret) != strlen(existing) ||
            !equalsConstantTime(currentSecret, existing, strlen(existing))) {
            EventLog::log(LOG_WARN, "API: remplacement du secret refuse");
            return false;
        }
    }

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;
    const size_t written = prefs.putString(KEY_SECRET, newSecret);
    prefs.end();
    if (written == 0U) return false;

    EventLog::log(LOG_INFO, "API: secret d'authentification enregistre");
    return true;
}

bool verify(const String& canonicalMessage, uint32_t nonce, const String& hexSignature) {
    char secret[MAX_SECRET + 1] = {};
    if (!readSecret(secret, sizeof(secret))) {
        EventLog::log(LOG_WARN, "API: aucun secret enregistre, ecriture refusee");
        return false;
    }

    loadNonce();
    // Strictement croissant : une requete capturee ne peut pas etre rejouee,
    // meme apres un redemarrage, puisque le compteur est persiste.
    if (nonce <= g_lastNonce) {
        EventLog::log(LOG_WARN, "API: nonce %lu deja vu (dernier %lu), refuse",
                      (unsigned long)nonce, (unsigned long)g_lastNonce);
        return false;
    }

    char expected[65];
    if (!computeHmacHex(secret, canonicalMessage, expected)) return false;

    if (hexSignature.length() != 64U ||
        !equalsConstantTime(hexSignature.c_str(), expected, 64U)) {
        EventLog::log(LOG_WARN, "API: signature invalide, ecriture refusee");
        return false;
    }

    storeNonce(nonce);
    return true;
}

uint32_t lastNonce() {
    loadNonce();
    return g_lastNonce;
}

void forgetSecret() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    prefs.remove(KEY_SECRET);
    prefs.end();
    EventLog::log(LOG_WARN, "API: secret efface depuis l'ecran du module (secret oublie)");
}

} // namespace ApiAuth
