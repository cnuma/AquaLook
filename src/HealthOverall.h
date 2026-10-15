#pragma once

#include "BootLoopGuard.h"
#include "FaultManager.h"
#include "IncidentManager.h"

// Etat general du module, source unique pour /api/health ("overall") et la
// page Sante du LCD : les deux doivent dire la meme chose.
//
// Avant P3.18, "overall" ne refletait que la garde anti-boucle : la page
// affichait "Normal" en vert alors qu'une alarme attendait un acquittement
// (constate le 15 oct. 2026 pendant l'essai de rafale SD de P3.2).
//
// Priorite : degrade > essai > attention > normal. "attention" = une alarme
// non acquittee, un defaut actif (hors boucle de demarrage, deja porte par
// degrade/essai) ou un incident SD actif ou retabli a acquitter.
enum class HealthOverall : uint8_t { NORMAL, ATTENTION, ESSAI, DEGRADE };

inline HealthOverall healthOverall() {
    if (BootLoopGuard::isDegraded()) return HealthOverall::DEGRADE;
    if (BootLoopGuard::isOnProbation()) return HealthOverall::ESSAI;

    const uint32_t faultMask = FaultManager::activeMask() &
                               ~(1UL << static_cast<uint8_t>(FaultId::BOOT_LOOP));
    const IncidentState sd = IncidentManager::storageSd().state;
    if (FaultManager::hasUnacknowledgedErrors() || faultMask != 0U ||
        sd == IncidentState::ACTIVE ||
        sd == IncidentState::RECOVERED_UNACKNOWLEDGED) {
        return HealthOverall::ATTENTION;
    }
    return HealthOverall::NORMAL;
}

// Valeurs de l'API : "degrade", "essai" et "normal" inchangees (lues par
// sante.js), "attention" ajoutee.
inline const char* healthOverallCode(HealthOverall o) {
    switch (o) {
        case HealthOverall::DEGRADE:   return "degrade";
        case HealthOverall::ESSAI:     return "essai";
        case HealthOverall::ATTENTION: return "attention";
        default:                       return "normal";
    }
}
