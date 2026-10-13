#include "NetName.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_mac.h>

#include "EventLog.h"

namespace NetName {

namespace {
char g_name[33] = "";
bool g_announced = false;
bool g_failed = false;   // une seule tentative : pas de boucle sur un echec
}  // namespace

void prepare(const char* moduleId) {
    // Nom DNS : minuscules, chiffres et tirets, 32 caracteres au plus, ni
    // tiret en tete ou en fin (RFC 1123).
    size_t n = 0U;
    for (const char* s = moduleId ? moduleId : ""; *s && n < 32U; ++s) {
        char c = *s;
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (ok) g_name[n++] = c;
        else if (n > 0U && g_name[n - 1U] != '-') g_name[n++] = '-';
    }
    while (n > 0U && g_name[n - 1U] == '-') --n;
    g_name[n] = '\0';
    if (n == 0U) {
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(g_name, sizeof(g_name), "aqualook-%02x%02x", mac[4], mac[5]);
    }
    WiFi.setHostname(g_name);
    EventLog::log(LOG_INFO, "[RESEAU] nom du module : %s", g_name);
}

void update(bool wifiConnected) {
    if (g_announced || g_failed || !wifiConnected || g_name[0] == '\0') return;
    if (!MDNS.begin(g_name)) {
        g_failed = true;
        EventLog::log(LOG_WARN, "[RESEAU] mDNS indisponible : utiliser l'adresse IP");
        return;
    }
    MDNS.addService("http", "tcp", 80);
    g_announced = true;
    EventLog::log(LOG_INFO, "[RESEAU] joignable par http://%s.local", g_name);
}

const char* name() { return g_name; }

bool announced() { return g_announced; }

}  // namespace NetName
