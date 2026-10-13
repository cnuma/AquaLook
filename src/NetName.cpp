#include "NetName.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <cstring>
#include <esp_mac.h>

#include "EventLog.h"

namespace NetName {

namespace {
constexpr char NVS_NS[]  = "aq_net";
constexpr char NVS_KEY[] = "nom";
constexpr size_t DNS_NAME_MAX = 32U;

char g_name[DNS_NAME_MAX + 1U] = "";
bool g_announced = false;
bool g_failed = false;   // une seule tentative : pas de boucle sur un echec

void defaultName(char* out, size_t outSize) {
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, outSize, "aqualook-%02x%02x", mac[4], mac[5]);
}

// Lettre accentuee UTF-8 (2 octets, 0xC3 xx ou oe 0xC5 0x93) -> ASCII ;
// nullptr si inconnue (devient un separateur).
const char* asciiFor(uint8_t b0, uint8_t b1) {
    if (b0 == 0xC5 && (b1 == 0x93 || b1 == 0x92)) return "oe";
    if (b0 != 0xC3) return nullptr;
    const uint8_t c = b1 >= 0xA0 ? static_cast<uint8_t>(b1 - 0x20) : b1;   // minuscule -> majuscule
    if (c >= 0x80 && c <= 0x85) return "a";
    if (c == 0x87) return "c";
    if (c >= 0x88 && c <= 0x8B) return "e";
    if (c >= 0x8C && c <= 0x8F) return "i";
    if (c == 0x91) return "n";
    if ((c >= 0x92 && c <= 0x96) || c == 0x98) return "o";
    if (c >= 0x99 && c <= 0x9C) return "u";
    if (c == 0x9D) return "y";
    return nullptr;
}

// Libelle libre -> nom DNS : minuscules, chiffres, tirets, 32 caracteres au
// plus, ni tiret en tete ou en fin (RFC 1123). Vide si rien d'utilisable.
void toDnsName(const char* label, char* out, size_t outSize) {
    size_t n = 0U;
    auto put = [&](char c) {
        if (n + 1U >= outSize || n >= DNS_NAME_MAX) return;
        if (c == '-' && (n == 0U || out[n - 1U] == '-')) return;
        out[n++] = c;
    };
    const uint8_t* s = reinterpret_cast<const uint8_t*>(label ? label : "");
    while (*s) {
        const uint8_t c = *s;
        if (c < 0x80) {
            if (c >= 'A' && c <= 'Z') put(static_cast<char>(c - 'A' + 'a'));
            else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) put(static_cast<char>(c));
            else put('-');
            ++s;
            continue;
        }
        size_t len = (c & 0xE0) == 0xC0 ? 2U : (c & 0xF0) == 0xE0 ? 3U : (c & 0xF8) == 0xF0 ? 4U : 1U;
        const char* a = (len == 2U && s[1]) ? asciiFor(c, s[1]) : nullptr;
        if (a) { while (*a) put(*a++); } else put('-');
        for (size_t k = 0U; k < len && *s; ++k) ++s;
    }
    while (n > 0U && out[n - 1U] == '-') --n;
    out[n] = '\0';
}

void announce() {
    if (!MDNS.begin(g_name)) {
        g_failed = true;
        EventLog::log(LOG_WARN, "[RESEAU] mDNS indisponible : utiliser l'adresse IP");
        return;
    }
    MDNS.addService("http", "tcp", 80);
    g_announced = true;
    EventLog::log(LOG_INFO, "[RESEAU] joignable par http://%s.local", g_name);
}
}  // namespace

void prepare() {
    Preferences prefs;
    if (prefs.begin(NVS_NS, true)) {
        prefs.getString(NVS_KEY, g_name, sizeof(g_name));
        prefs.end();
    }
    if (g_name[0] == '\0') defaultName(g_name, sizeof(g_name));
    WiFi.setHostname(g_name);
    EventLog::log(LOG_INFO, "[RESEAU] nom du module : %s", g_name);
}

void update(bool wifiConnected) {
    if (g_announced || g_failed || !wifiConnected || g_name[0] == '\0') return;
    announce();
}

void setLabel(const char* label) {
    char wanted[DNS_NAME_MAX + 1U] = "";
    toDnsName(label, wanted, sizeof(wanted));
    const bool byDefault = wanted[0] == '\0';
    if (byDefault) defaultName(wanted, sizeof(wanted));
    if (strcmp(wanted, g_name) == 0) return;

    Preferences prefs;
    if (prefs.begin(NVS_NS, false)) {
        if (byDefault) prefs.remove(NVS_KEY);
        else prefs.putString(NVS_KEY, wanted);
        prefs.end();
    }
    strlcpy(g_name, wanted, sizeof(g_name));
    WiFi.setHostname(g_name);   // nom DHCP : pris a la prochaine connexion
    EventLog::log(LOG_INFO, "[RESEAU] nouveau nom : %s%s", g_name, byDefault ? " (par defaut)" : "");
    if (g_announced) {          // mDNS : renomme tout de suite
        MDNS.end();
        g_announced = false;
        g_failed = false;
        announce();
    }
}

const char* name() { return g_name; }

bool announced() { return g_announced; }

}  // namespace NetName
