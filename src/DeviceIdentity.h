#pragma once

#include <Arduino.h>
#include <stdio.h>

// Identifiant materiel du module (decision D016, lot C).
//
// "aql-" + adresse MAC de base gravee en eFuse, en hexadecimal minuscule
// (12 caracteres) : stable a travers un reflash, un effacement de la NVS
// ou un changement de nom, et identique a l'adresse Wi-Fi station que
// /api/diagnostics affiche deja (wifi.mac) -- l'utilisateur peut faire le
// rapprochement sans outil.
//
// Le serveur s'en sert pour garantir qu'un module n'existe qu'une fois
// (colonne module.hw_id unique) ; l'enrolement par code court (lot D)
// s'appuiera dessus. Ce n'est PAS un secret : il circule en clair dans le
// rapport et s'affiche, l'authentification reste le jeton du module.
//
// En-tete seul : EventLog.h l'utilise pour la banniere de demarrage et est
// inclus par des bancs test_* dont les filtres de sources n'embarquent pas
// forcement un .cpp de plus.
namespace DeviceIdentity {

constexpr size_t HW_ID_LEN = 17;  // "aql-" + 12 hex + '\0'

inline const char* hwId() {
    static char id[HW_ID_LEN] = {0};
    if (id[0] == '\0') {
        // getEfuseMac() range l'octet 0 de l'adresse dans les 8 bits de
        // poids faible : on le relit dans cet ordre pour retrouver
        // l'ecriture habituelle (celle de WiFi.macAddress()).
        const uint64_t mac = ESP.getEfuseMac();
        snprintf(id, sizeof(id), "aql-%02x%02x%02x%02x%02x%02x",
                 static_cast<unsigned>(mac & 0xFF),
                 static_cast<unsigned>((mac >> 8) & 0xFF),
                 static_cast<unsigned>((mac >> 16) & 0xFF),
                 static_cast<unsigned>((mac >> 24) & 0xFF),
                 static_cast<unsigned>((mac >> 32) & 0xFF),
                 static_cast<unsigned>((mac >> 40) & 0xFF));
    }
    return id;
}

}  // namespace DeviceIdentity
