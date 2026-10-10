#include "WebSession.h"

#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <string.h>

#include "ApiAuth.h"
#include "EventLog.h"

namespace WebSession {
namespace {

struct Slot {
    bool used = false;
    char token[TOKEN_HEX_LEN + 1] = {};
    uint32_t lastUseMs = 0U;
};

struct Challenge {
    bool used = false;
    char value[TOKEN_HEX_LEN + 1] = {};
    uint32_t issuedMs = 0U;
};

// Le filtre tourne dans la tache async_tcp, l'oubli du secret dans la boucle
// principale (ecran) : une section critique courte protege les deux tables.
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
Slot g_sessions[MAX_SESSIONS];
Challenge g_challenges[MAX_CHALLENGES];
uint8_t g_failures = 0U;
uint32_t g_lockUntilMs = 0U;
bool g_locked = false;

void randomHex(char* out) {
    uint8_t raw[TOKEN_HEX_LEN / 2U];
    esp_fill_random(raw, sizeof(raw));
    static const char DIGITS[] = "0123456789abcdef";
    for (uint8_t i = 0U; i < sizeof(raw); ++i) {
        out[i * 2U] = DIGITS[(raw[i] >> 4) & 0x0F];
        out[i * 2U + 1U] = DIGITS[raw[i] & 0x0F];
    }
    out[TOKEN_HEX_LEN] = '\0';
}

bool equalsConstantTime(const char* a, const char* b, size_t len) {
    uint8_t diff = 0U;
    for (size_t i = 0; i < len; ++i) diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    return diff == 0U;
}

// Extrait la valeur du cookie aqls= d'un en-tete "a=1; aqls=...; b=2".
bool tokenFromCookie(const char* header, char* out) {
    if (!header) return false;
    const size_t nameLen = strlen(COOKIE_NAME);
    const char* p = header;
    while (*p) {
        while (*p == ' ' || *p == ';') ++p;
        if (strncmp(p, COOKIE_NAME, nameLen) == 0 && p[nameLen] == '=') {
            const char* v = p + nameLen + 1U;
            size_t n = 0U;
            while (v[n] && v[n] != ';' && v[n] != ' ') ++n;
            if (n != TOKEN_HEX_LEN) return false;
            memcpy(out, v, TOKEN_HEX_LEN);
            out[TOKEN_HEX_LEN] = '\0';
            return true;
        }
        while (*p && *p != ';') ++p;
    }
    return false;
}

// Appele sous section critique.
int findSession(const char* token, uint32_t now) {
    for (uint8_t i = 0U; i < MAX_SESSIONS; ++i) {
        Slot& s = g_sessions[i];
        if (!s.used) continue;
        if (now - s.lastUseMs > IDLE_TIMEOUT_MS) { s.used = false; continue; }
        if (equalsConstantTime(s.token, token, TOKEN_HEX_LEN)) return i;
    }
    return -1;
}

bool storeSession(char* outToken) {
    char token[TOKEN_HEX_LEN + 1];
    randomHex(token);
    const uint32_t now = millis();
    portENTER_CRITICAL(&g_mux);
    // Place libre, sinon la session la moins recemment utilisee : la
    // cinquieme connexion deconnecte la plus ancienne plutot que d'etre
    // refusee (un onglet oublie ne doit pas bloquer le proprietaire).
    int target = -1;
    uint32_t oldestAge = 0U;
    for (uint8_t i = 0U; i < MAX_SESSIONS; ++i) {
        Slot& s = g_sessions[i];
        if (!s.used || now - s.lastUseMs > IDLE_TIMEOUT_MS) { target = i; break; }
        if (now - s.lastUseMs >= oldestAge) { oldestAge = now - s.lastUseMs; target = i; }
    }
    Slot& s = g_sessions[target];
    s.used = true;
    memcpy(s.token, token, sizeof(token));
    s.lastUseMs = now;
    portEXIT_CRITICAL(&g_mux);
    memcpy(outToken, token, sizeof(token));
    return true;
}

}  // namespace

bool newChallenge(char out[TOKEN_HEX_LEN + 1]) {
    char value[TOKEN_HEX_LEN + 1];
    randomHex(value);
    const uint32_t now = millis();
    portENTER_CRITICAL(&g_mux);
    int target = 0;
    uint32_t oldestAge = 0U;
    for (uint8_t i = 0U; i < MAX_CHALLENGES; ++i) {
        Challenge& c = g_challenges[i];
        if (!c.used) { target = i; break; }
        if (now - c.issuedMs >= oldestAge) { oldestAge = now - c.issuedMs; target = i; }
    }
    Challenge& c = g_challenges[target];
    c.used = true;
    memcpy(c.value, value, sizeof(value));
    c.issuedMs = now;
    portEXIT_CRITICAL(&g_mux);
    memcpy(out, value, sizeof(value));
    return true;
}

LoginResult login(const char* challengeHex, const char* signatureHex,
                  char outToken[TOKEN_HEX_LEN + 1], uint32_t& retryInSec) {
    retryInSec = 0U;
    if (!ApiAuth::hasSecret()) return LoginResult::NO_SECRET;

    const uint32_t now = millis();
    bool challengeOk = false;
    bool locked = false;
    portENTER_CRITICAL(&g_mux);
    if (g_locked && (int32_t)(g_lockUntilMs - now) > 0) {
        locked = true;
        retryInSec = (g_lockUntilMs - now + 999U) / 1000U;
    }
    // Le defi est consomme meme en cas de verrou : une reponse ne se rejoue
    // jamais.
    if (challengeHex && strlen(challengeHex) == TOKEN_HEX_LEN) {
        for (uint8_t i = 0U; i < MAX_CHALLENGES; ++i) {
            Challenge& c = g_challenges[i];
            if (!c.used || memcmp(c.value, challengeHex, TOKEN_HEX_LEN) != 0) continue;
            c.used = false;
            challengeOk = now - c.issuedMs <= CHALLENGE_TTL_MS;
            break;
        }
    }
    portEXIT_CRITICAL(&g_mux);

    if (locked) return LoginResult::LOCKED;
    if (!challengeOk) return LoginResult::NO_CHALLENGE;

    char message[8 + TOKEN_HEX_LEN + 1];
    snprintf(message, sizeof(message), "session|%s", challengeHex);
    if (!signatureHex || !ApiAuth::verifyMessage(message, signatureHex)) {
        portENTER_CRITICAL(&g_mux);
        if (g_failures < 255U) ++g_failures;
        uint32_t lockSec = 0U;
        if (g_failures >= FREE_FAILURES) {
            lockSec = FIRST_LOCK_SEC;
            for (uint8_t k = FREE_FAILURES; k < g_failures && lockSec < MAX_LOCK_SEC; ++k) {
                lockSec *= 2U;
            }
            if (lockSec > MAX_LOCK_SEC) lockSec = MAX_LOCK_SEC;
            g_locked = true;
            g_lockUntilMs = now + lockSec * 1000UL;
        }
        const uint8_t failures = g_failures;
        portEXIT_CRITICAL(&g_mux);
        retryInSec = lockSec;
        EventLog::log(LOG_WARN, "[WEB-SESSION] mot de passe refuse (%u echec(s), attente %lu s)",
                      static_cast<unsigned>(failures), static_cast<unsigned long>(lockSec));
        return LoginResult::REFUSED;
    }

    portENTER_CRITICAL(&g_mux);
    g_failures = 0U;
    g_locked = false;
    portEXIT_CRITICAL(&g_mux);
    storeSession(outToken);
    EventLog::log(LOG_INFO, "[WEB-SESSION] session ouverte (%u active(s))",
                  static_cast<unsigned>(activeCount()));
    return LoginResult::OK;
}

bool openTrusted(char outToken[TOKEN_HEX_LEN + 1]) {
    return storeSession(outToken);
}

bool isValid(const char* cookieHeader) {
    char token[TOKEN_HEX_LEN + 1];
    if (!tokenFromCookie(cookieHeader, token)) return false;
    const uint32_t now = millis();
    portENTER_CRITICAL(&g_mux);
    const int i = findSession(token, now);
    if (i >= 0) g_sessions[i].lastUseMs = now;
    portEXIT_CRITICAL(&g_mux);
    return i >= 0;
}

uint32_t remainingSec(const char* cookieHeader) {
    char token[TOKEN_HEX_LEN + 1];
    if (!tokenFromCookie(cookieHeader, token)) return 0U;
    const uint32_t now = millis();
    uint32_t remaining = 0U;
    portENTER_CRITICAL(&g_mux);
    const int i = findSession(token, now);
    if (i >= 0) remaining = (IDLE_TIMEOUT_MS - (now - g_sessions[i].lastUseMs)) / 1000U;
    portEXIT_CRITICAL(&g_mux);
    return remaining;
}

void close(const char* cookieHeader) {
    char token[TOKEN_HEX_LEN + 1];
    if (!tokenFromCookie(cookieHeader, token)) return;
    portENTER_CRITICAL(&g_mux);
    const int i = findSession(token, millis());
    if (i >= 0) g_sessions[i].used = false;
    portEXIT_CRITICAL(&g_mux);
}

void closeAll() {
    portENTER_CRITICAL(&g_mux);
    for (uint8_t i = 0U; i < MAX_SESSIONS; ++i) g_sessions[i].used = false;
    for (uint8_t i = 0U; i < MAX_CHALLENGES; ++i) g_challenges[i].used = false;
    portEXIT_CRITICAL(&g_mux);
}

uint8_t activeCount() {
    const uint32_t now = millis();
    uint8_t n = 0U;
    portENTER_CRITICAL(&g_mux);
    for (uint8_t i = 0U; i < MAX_SESSIONS; ++i) {
        if (g_sessions[i].used && now - g_sessions[i].lastUseMs <= IDLE_TIMEOUT_MS) ++n;
    }
    portEXIT_CRITICAL(&g_mux);
    return n;
}

}  // namespace WebSession
