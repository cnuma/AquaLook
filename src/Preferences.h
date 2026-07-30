#pragma once

// Adaptateur local AquaLook autour de la classe Preferences du core ESP32.
// Le fichier porte volontairement le meme nom que l'en-tete du framework :
// les sources du projet incluent d'abord cet adaptateur, qui charge ensuite
// l'en-tete reel avec include_next.
#include_next <Preferences.h>
#include <Arduino.h>

class AquaLookPreferences : public ::Preferences {
public:
    size_t putBytes(const char* key, const void* value, size_t len) {
        const size_t written = ::Preferences::putBytes(key, value, len);
        if (written == len || !key || len < LARGE_BLOB_THRESHOLD) {
            return written;
        }

        // NVS conserve normalement l'ancienne valeur pendant la nouvelle
        // ecriture. Sur la partition AquaLook de 20 Kio, un blob de 4,8 Kio
        // peut manquer d'espace temporaire malgre la presence d'une valeur
        // valide. Le blob complet reste en RAM chez l'appelant : supprimer
        // uniquement cette cle puis retenter libere les pages occupees, sans
        // effacer le namespace ni les autres donnees persistantes.
        log_w(
            "NVS: blob volumineux non ecrit key=%s bytes=%u; nettoyage cible et nouvelle tentative",
            key,
            static_cast<unsigned>(len)
        );

        if (!::Preferences::remove(key)) {
            log_e("NVS: suppression de la cle %s impossible", key);
            return written;
        }

        const size_t retried = ::Preferences::putBytes(key, value, len);
        if (retried != len || ::Preferences::getBytesLength(key) != len) {
            log_e(
                "NVS: nouvelle tentative incomplete key=%s written=%u/%u",
                key,
                static_cast<unsigned>(retried),
                static_cast<unsigned>(len)
            );
            return retried;
        }

        log_w(
            "NVS: blob restaure apres nettoyage cible key=%s bytes=%u",
            key,
            static_cast<unsigned>(retried)
        );
        return retried;
    }

private:
    static constexpr size_t LARGE_BLOB_THRESHOLD = 4096U;
};

// Les declarations Preferences des sources AquaLook deviennent l'adaptateur.
// Le framework ESP32 lui-meme reste compile avec sa classe d'origine.
#define Preferences AquaLookPreferences
