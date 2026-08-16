#pragma once

#include <Arduino.h>

// Etape 4 du plan "Mise a jour distante des ressources Web" (ROADMAP.md) :
// telecharger un fichier depuis une URL de release GitHub et verifier sa
// taille et son empreinte SHA-256, sans encore rien ecrire sur la carte SD
// (l'ecriture reelle, avec sa strategie de coherence en cas d'interruption,
// est l'etape 5, separee et non encore faite).
//
// Volontairement une classe distincte d'OtaDownloadTest, meme si la logique
// de telechargement HTTPS/redirection/SHA-256 se ressemble beaucoup : cette
// classe tourne en fonctionnement normal (WiFi, SD, Web deja actifs), pas
// dans la tache FreeRTOS isolee et minimale du mode maintenance OTA
// (MaintenanceSetupWrapper.cpp). C'est exactement la raison pour laquelle
// ROADMAP.md decrit un canal decouple : partager le code entre les deux
// recreerait un couplage que cette isolation existe justement pour eviter.
struct WebAssetVerifyResult {
    bool success = false;
    uint32_t downloadedSize = 0;
    uint32_t downloadDurationMs = 0;
    char calculatedSha256[65] = {0};
    char detail[40] = {0};
};

class WebAssetsUpdater {
public:
    // url doit etre une URL https:// vers un hote GitHub autorise (release
    // asset). expectedSha256Hex : 64 caracteres hexadecimaux minuscules.
    // Le corps telecharge n'est jamais conserve, seulement haché au vol.
    static WebAssetVerifyResult verifyOnly(
        const char* url,
        uint32_t expectedSize,
        const char* expectedSha256Hex
    );
};
