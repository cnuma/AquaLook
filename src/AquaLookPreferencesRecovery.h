#pragma once

#include <Preferences.h>
#include <Arduino.h>

// Adaptateur limite aux sources AquaLook qui l'activent explicitement.
// Le core ESP32 et ses bibliotheques continuent d'utiliser Preferences natif.
class AquaLookPreferences : public Preferences {
public:
    size_t putBytes(const char* key, const void* value, size_t len) {
        const size_t written = Preferences::putBytes(key, value, len);
        if (written == len || !key || !value || len < LARGE_BLOB_THRESHOLD) {
            return written;
        }

        // NVS conserve l'ancienne valeur pendant le remplacement. Sur la
        // partition AquaLook de 20 Kio, le blob de configuration de 4,8 Kio
        // peut manquer d'espace temporaire. Le blob complet est encore en RAM :
        // supprimer uniquement sa cle, puis retenter, sans vider le namespace.
        log_w(
            "NVS: echec blob volumineux key=%s bytes=%u; nettoyage cible",
            key,
            static_cast<unsigned>(len)
        );

        if (!Preferences::remove(key)) {
            log_e("NVS: suppression cle impossible key=%s", key);
            return written;
        }

        const size_t retried = Preferences::putBytes(key, value, len);
        const size_t stored = Preferences::getBytesLength(key);
        if (retried != len || stored != len) {
            log_e(
                "NVS: reprise blob incomplete key=%s written=%u stored=%u expected=%u",
                key,
                static_cast<unsigned>(retried),
                static_cast<unsigned>(stored),
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

// Ce renommage n'est injecte que pour ConfigManager.cpp par le middleware
// PlatformIO defini dans tools/version_build.py.
#define Preferences AquaLookPreferences
