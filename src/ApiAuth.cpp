#include "ApiAuth.h"

#include <Preferences.h>
#include <nvs.h>
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
// Presence du secret, en cache : le filtre de session Web la consulte a
// chaque requete, et ouvrir la NVS a chaque fois couterait sur la tache
// async_tcp. -1 = pas encore lu ; tenu a jour par setSecret/forgetSecret.
volatile int8_t g_hasSecret = -1;

// Fenetre d'autorisation du premier secret (geste sur l'ecran).
volatile uint32_t g_firstAllowUntilMs = 0U;
volatile bool g_firstAllowOpen = false;

bool storeSecret(const char* newSecret) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;
    const size_t written = prefs.putString(KEY_SECRET, newSecret);
    prefs.end();
    if (written == 0U) return false;
    g_hasSecret = 1;
    g_firstAllowOpen = false;
    return true;
}

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
    if (g_hasSecret >= 0) return g_hasSecret == 1;
    // Lecture directe par l'API NVS pour distinguer « pas de secret » d'une
    // NVS illisible. Une erreur de lecture repond « secret present », sans
    // la garder en cache : le filtre de session reste ferme au lieu d'ouvrir
    // tout le module sur un incident passager (echec ferme).
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) { g_hasSecret = 0; return false; }
    if (err != ESP_OK) return true;
    size_t len = 0U;
    err = nvs_get_str(h, KEY_SECRET, nullptr, &len);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) { g_hasSecret = 0; return false; }
    if (err != ESP_OK) return true;
    g_hasSecret = len > 1U ? 1 : 0;
    return g_hasSecret == 1;
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
    } else if (firstSecretWindowLeftMs() == 0U) {
        EventLog::log(LOG_WARN, "API: premier secret refuse, pas autorise sur l'ecran");
        return false;
    }

    if (!storeSecret(newSecret)) return false;

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

void allowFirstSecret(uint32_t windowMs) {
    g_firstAllowUntilMs = millis() + windowMs;
    g_firstAllowOpen = true;
    EventLog::log(LOG_INFO, "API: pose du mot de passe Web autorisee depuis l'ecran (%lu min)",
                  static_cast<unsigned long>(windowMs / 60000UL));
}

uint32_t firstSecretWindowLeftMs() {
    if (!g_firstAllowOpen) return 0U;
    const int32_t left = static_cast<int32_t>(g_firstAllowUntilMs - millis());
    if (left <= 0) { g_firstAllowOpen = false; return 0U; }
    return static_cast<uint32_t>(left);
}

bool setSecretEncrypted(const char* challengeHex, const char* encHex, const char* macHex) {
    char secret[MAX_SECRET + 1] = {};
    if (!challengeHex || !encHex || !macHex || !readSecret(secret, sizeof(secret))) return false;
    const size_t encLen = strlen(encHex);
    if (encLen % 2U != 0U || encLen / 2U < 12U || encLen / 2U > MAX_SECRET) return false;

    // Authenticite d'abord : sans l'actuel, pas de MAC valable, rien n'est
    // dechiffre ni ecrit.
    String macMsg = "aql-chg-mac|";
    macMsg += challengeHex; macMsg += '|'; macMsg += encHex;
    char expected[65];
    if (!computeHmacHex(secret, macMsg, expected)) return false;
    if (strlen(macHex) != 64U || !equalsConstantTime(macHex, expected, 64U)) {
        EventLog::log(LOG_WARN, "API: changement de secret refuse (actuel incorrect)");
        return false;
    }

    char ks[129];
    String ksMsg = "aql-chg-ks|";
    ksMsg += challengeHex;
    if (!computeHmacHex(secret, ksMsg + "|0", ks) ||
        !computeHmacHex(secret, ksMsg + "|1", ks + 64)) return false;

    char next[MAX_SECRET + 1] = {};
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < encLen / 2U; ++i) {
        const int h = nib(encHex[2 * i]), l = nib(encHex[2 * i + 1]);
        const int kh = nib(ks[2 * i]), kl = nib(ks[2 * i + 1]);
        if (h < 0 || l < 0 || kh < 0 || kl < 0) return false;
        const uint8_t b = static_cast<uint8_t>(((h << 4) | l) ^ ((kh << 4) | kl));
        if (b == 0U) return false;          // un zero couperait la chaine
        next[i] = static_cast<char>(b);
    }
    if (!storeSecret(next)) return false;
    EventLog::log(LOG_INFO, "API: secret remplace (envoi chiffre)");
    return true;
}

bool verifyMessage(const char* message, const char* hexSignature) {
    char secret[MAX_SECRET + 1] = {};
    if (!message || !hexSignature || !readSecret(secret, sizeof(secret))) return false;
    char expected[65];
    if (!computeHmacHex(secret, String(message), expected)) return false;
    return strlen(hexSignature) == 64U &&
           equalsConstantTime(hexSignature, expected, 64U);
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
    g_hasSecret = 0;
    EventLog::log(LOG_WARN, "API: secret efface depuis l'ecran du module (secret oublie)");
}

} // namespace ApiAuth
