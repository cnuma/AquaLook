#pragma once

#include <Arduino.h>

// Garde de securite applicative pour l'activation OTA.
//
// Le framework Arduino/PlatformIO utilise ici n'expose pas de maniere fiable
// le rollback automatique du bootloader ESP-IDF
// (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE depend d'un sdkconfig non
// accessible simplement depuis ce projet). Cette garde remplace ce mecanisme
// par une validation cote application :
//
//   1. INSTALL_UPDATE arme la garde puis bascule esp_ota_set_boot_partition
//      vers la partition fraichement verifiee, avant de redemarrer.
//   2. Au premier boot sur la nouvelle partition, onBoot() incremente un
//      compteur de tentatives persiste en NVS. Si ce compteur depasse le
//      maximum autorise sans validation, la partition precedente est
//      restauree automatiquement et le module redemarre dessus.
//   3. update() valide la bascule une fois un delai de fonctionnement stable
//      ecoule, ce qui desarme la garde et efface le compteur.
class OtaBootGuard {
public:
    // Arme la garde avant bascule. previousPartitionLabel est la partition
    // actuellement en cours d'execution (cible du retour arriere),
    // targetPartitionLabel la partition qui va etre activee.
    static bool arm(const char* previousPartitionLabel, const char* targetPartitionLabel);

    // A appeler tres tot dans setup(), avant toute initialisation lourde.
    // Ne redemarre que si un retour arriere automatique est declenche.
    static void onBoot();

    // A appeler dans loop(). Valide la bascule une fois le delai de
    // stabilite atteint ; ne fait rien si aucune bascule n'est en attente
    // ou si elle est deja validee.
    static void update();

private:
    static bool _pendingValidation;
    static uint32_t _bootMs;
};
