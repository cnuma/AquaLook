#pragma once

#include <Arduino.h>

class WiFiManager;
class RelaisManager;
class ConfigManager;

// ═══════════════════════════════════════════════════════════════
//  UpdateCheckScheduler — vérification périodique des mises à jour
//
//  Une vérification de mise à jour exige une poignée de main TLS, donc
//  beaucoup de mémoire contiguë. En fonctionnement normal le module ne
//  dispose que d'environ 32 Ko de tas et de 17 Ko de plus gros bloc quand
//  l'écran est allumé : les deux tentatives de vérification dans ce contexte
//  ont échoué (poignée de main impossible, puis débordement de pile de
//  loopTask). Le mode maintenance, lui, démarre sur un contexte minimal avec
//  environ 242 Ko de tas libre.
//
//  Cette classe ne fait donc pas la vérification : elle décide **quand** il
//  est sûr de redémarrer en mode maintenance pour la faire.
//
//  Conditions de déclenchement, toutes obligatoires :
//    1. la vérification périodique est activée ;
//    2. l'heure est connue — condition posée par l'utilisateur : main.cpp
//       n'appelle ScheduleManager::update() que si le NTP est synchronisé,
//       donc redémarrer sans heure valide reviendrait à suspendre l'arrosage
//       programmé pour une simple vérification de mise à jour ;
//    3. le WiFi est connecté depuis assez longtemps pour être qualifié de
//       stable — redémarrer pour une vérification qui échouera faute de
//       réseau n'apporte rien et coûte un redémarrage ;
//    4. aucun arrosage n'est en cours — même invariant que les autres
//       commandes de maintenance : on n'interrompt jamais une vanne ouverte ;
//    5. l'échéance est atteinte (heure du jour + intervalle en jours).
//
//  Anti-boucle : le jour de la vérification est enregistré **avant** de
//  demander le redémarrage, jamais après. Si la vérification échoue, ou si le
//  module redémarre en boucle pour une autre raison, il ne peut pas réessayer
//  avant l'échéance suivante. Une boucle de redémarrages serait une panne
//  bien plus grave que la vérification manquée qu'elle chercherait à rattraper.
// ═══════════════════════════════════════════════════════════════

struct UpdateCheckConfig {
    bool     enabled      = true;
    uint8_t  hour         = 3U;    // 0-23, heure locale
    uint8_t  minute       = 30U;   // 0-59
    uint8_t  intervalDays = 1U;    // 1 = tous les jours
};

class UpdateCheckScheduler {
public:
    void begin();

    // Appelée à chaque tour de boucle. Ne fait rien tant que toutes les
    // conditions ne sont pas réunies ; peut déclencher un redémarrage.
    void update(bool ntpSynced,
                int hour,
                int minute,
                uint32_t epochDay,
                const WiFiManager* wifi,
                const RelaisManager* relais,
                const ConfigManager* config);

    const UpdateCheckConfig& config() const { return _cfg; }
    bool set(bool enabled, uint8_t hour, uint8_t minute, uint8_t intervalDays);

    uint32_t lastCheckEpochDay() const { return _lastCheckEpochDay; }

    // Durée de connexion WiFi ininterrompue exigée avant de qualifier le
    // réseau de stable. Une association qui vient de s'établir n'est pas
    // encore une preuve que la route vers Internet fonctionne.
    static constexpr uint32_t WIFI_STABLE_MS = 300000UL;   // 5 min

private:
    void load();
    void save();
    void saveLastCheckDay(uint32_t epochDay);
    // Journal des refus, limité : sans limite, une condition non remplie
    // produirait une ligne par tour de boucle.
    void logBlocked(const char* reason);

    UpdateCheckConfig _cfg;
    uint32_t _lastCheckEpochDay = 0U;
    uint32_t _wifiConnectedSinceMs = 0U;
    uint32_t _blockedLogAtMs = 0U;
    bool     _triggered = false;
    bool     _loaded = false;
};
