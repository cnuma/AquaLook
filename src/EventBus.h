#pragma once

#include <stdint.h>

// ═══════════════════════════════════════════════════════════════
//  EventBus — canal de communication inter-modules
//
//  Principe : flags bool statiques, positionnés par le producteur,
//  consommés (remis à false) par le consommateur.
//  L'ESP32 est single-thread loop() — pas de mutex nécessaire.
//
//  Producteurs → Consommateurs :
//    WebManager      → DisplayManager  via displayDirty
//    RelaisManager   → DisplayManager  via displayDirty
//    WebManager      → tous managers   via configDirty
//    WebManager      → WiFiManager     via wifiDirty
//    DisplayManager  → WiFiManager     via captiveRequested  (bouton écran)
//    WebManager      → WiFiManager     via captiveRequested  (route /api/captive)
//    DisplayManager  → CloudSyncScheduler via cloudEnrollRequest (ADMIN > En ligne)
//
//  Invariant I18 : EventBus est le SEUL canal inter-modules
//  hors callbacks explicites (ex. onRelayRequest).
// ═══════════════════════════════════════════════════════════════

struct EventBus {
    // ── Display ───────────────────────────────────────────────
    /// Positionné quand l'état visible a changé (relais, config web...).
    /// DisplayManager le consomme au prochain tick update() et force un redraw.
    static bool displayDirty;

    // ── Configuration ─────────────────────────────────────────
    /// Positionné après une écriture ConfigManager depuis WebManager.
    /// Les managers qui ont des paramètres runtime (NTP, OWM...) le relisent.
    static bool configDirty;

    // ── WiFi ──────────────────────────────────────────────────
    /// Positionné quand les credentials WiFi ont changé.
    /// WiFiManager relance la connexion sans reboot si en mode CONNECTING.
    /// (Note : setWifi() fait déjà un reboot — ce flag sert si on veut
    ///  ajouter une reconnexion douce à l'avenir.)
    static bool wifiDirty;

    // ── Portail captif ────────────────────────────────────────
    /// Positionné par DisplayManager (bouton écran ADMIN page WiFi)
    /// ou par WebManager (route POST /api/captive).
    /// WiFiManager le consomme et bascule en mode AP + DNS redirect.
    static bool captiveRequested;

    // ── Mise a jour en cours ──────────────────────────────────
    /// Positionne des qu'une mise a jour est demandee (ressources Web ou
    /// firmware), jusqu'au redemarrage qui l'execute.
    ///
    /// Contrairement aux autres, ce drapeau n'est PAS consomme : il decrit
    /// un etat qui dure, pas un evenement. Il sert a prevenir l'utilisateur
    /// qu'une operation est engagee et qu'il ne faut pas solliciter le
    /// module - bandeau LCD et voyant passent au violet, la meme teinte que
    /// celle deja utilisee partout pour "mise a jour" (Theme::PURPLE,
    /// --purple #6633cc cote Web).
    static bool updateInProgress;

    // ── Rattachement a un compte en ligne (D016, lot D) ───────
    /// Positionne par DisplayManager seulement (ADMIN > En ligne, derriere
    /// le PIN) : aucune route Web ne rattache ni ne detache le module.
    /// CloudSyncScheduler::update() le consomme.
    enum : uint8_t { ENROLL_NONE = 0, ENROLL_START = 1, ENROLL_CANCEL = 2, ENROLL_FORGET = 3 };
    static uint8_t cloudEnrollRequest;

    // ── Helpers ───────────────────────────────────────────────
    /// Remet tous les flags à false — appelé uniquement en test unitaire.
    static void reset() {
        displayDirty     = false;
        configDirty      = false;
        wifiDirty        = false;
        captiveRequested = false;
        updateInProgress = false;
        cloudEnrollRequest = ENROLL_NONE;
    }
};

// Définitions dans EventBus.cpp
