#pragma once
#include <Arduino.h>
#include "ConfigManager.h"

#define PIN_TFT_BL    21

// Les broches et canaux du voyant RGB embarque ont demenage dans
// src/StatusLed.cpp : ce sont des details du materiel du voyant, et la
// carte S3 n'en a aucun (ruban WS2812 externe a la place).

class ScreenManager {
public:
    void begin(ConfigManager* config = nullptr);
    // wifiSearching : WiFi ni connecte ni en portail captif (connexion en
    // cours ou reconnexion apres detection zombie) — priorite d'affichage
    // juste sous l'arrosage actif, au-dessus du mode LED normal.
    //
    // activeZoneMask  : bit z leve = zone z en cours d'arrosage.
    // rainBlockedMask : bit z leve = arrosage prevu aujourd'hui pour la
    //                   zone z, mais suspendu car la pluie annoncee
    //                   atteint son seuil.
    //
    // Les deux servent au ruban WS2812 de la carte S3, qui a une LED par
    // zone (voir StatusLed.h). Sans effet sur la carte historique, dont le
    // voyant unique ne peut montrer qu'un etat global — d'ou les valeurs
    // par defaut, qui laissent les appels existants inchanges.
    void update(bool anyRelayActive, bool wifiSearching,
                uint16_t activeZoneMask = 0U, uint8_t nbZones = 0U,
                uint16_t rainBlockedMask = 0U);
    void wakeUp();

    // Fait clignoter en BLANC la LED d'une zone, pour l'identifier
    // physiquement pendant le raccordement. Le blanc est la seule couleur
    // qu'aucun etat n'utilise : bleu = arrosage, orange = pluie, vert /
    // ambre / violet / rouge = etats du module. Aucune confusion possible.
    //
    // S'arrete tout seul au bout de durationMs : une identification oubliee
    // ne doit pas masquer indefiniment l'etat reel de la zone.
    // zone >= MAX_ZONES annule l'identification en cours.
    void identifyZone(uint8_t zone, uint32_t durationMs);

    bool isAsleep() const { return _sleeping; }

private:
    ConfigManager* _config = nullptr;
    bool _sleeping = false;
    uint32_t _lastActivity = 0;

    uint32_t _ledTimer = 0;
    uint8_t _ledPhase = 0;
    bool _relayWasActive = false;

#if AQUALOOK_BOARD_S3
    // Identification physique d'une zone (voir identifyZone).
    // 255 = aucune identification en cours.
    //
    // Sous garde : la carte historique n'a pas de ruban, elle ne peut rien
    // identifier et n'a donc pas a porter ces octets.
    uint8_t  _identifyZone  = 255;
    uint32_t _identifyUntil = 0;
#endif

    uint8_t _normalLedRed = 0;
    uint8_t _normalLedGreen = 0;
    uint8_t _normalLedBlue = 0;

    // Mise a jour en attente (firmware ou ressources Web) : chargee une
    // seule fois, jamais rafraichie en cours de fonctionnement. Comme pour
    // DisplayPlanningDecor::loadUpdateState(), c'est suffisant : toute
    // verification de mise a jour redemarre le module (UpdateCheckScheduler
    // via BootLoopGuard::restartDeliberately()), donc l'etat ne peut
    // changer qu'au demarrage suivant.
    bool _updateStateLoaded = false;
    bool _updatePending = false;
    void loadUpdateState();

    void screenOn();
    void screenOff();
    void updateLed(bool relayActive, bool wifiSearching);
    void renderLed();
    // Une LED par zone : bleu si elle arrose, orange si son arrosage du
    // jour est suspendu par la pluie, eteinte sinon.
    // Ne fait rien sur la carte historique (voyant unique).
    void renderZones(uint16_t activeZoneMask, uint16_t rainBlockedMask,
                     uint8_t nbZones);
    void ledOff();
    void ledSet(bool r, bool g, bool b);
    void ledSetBrightness(uint8_t r, uint8_t g, uint8_t b);
};
