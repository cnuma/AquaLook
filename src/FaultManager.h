#pragma once
#include <Arduino.h>

enum class FaultId : uint8_t {
    RELAY_I2C = 0,
    WIFI = 1,
    FILESYSTEM = 2,
    SOFTWARE = 3,
    STORAGE_SD = 4,
    // Sauvegarde de la configuration en echec : un reglage modifie par
    // l'utilisateur n'a PAS ete persiste et sera perdu au prochain
    // redemarrage. Ajoute le 16 aout 2026 apres l'incident de saturation NVS,
    // ou l'echec n'apparaissait que dans le journal : l'interface confirmait
    // l'enregistrement, le voyant restait au vert, et la perte n'etait
    // decouverte qu'au redemarrage suivant. Un reglage perdu en silence coute
    // plus cher a la confiance qu'une panne franche, qui elle se voit.
    CONFIG_PERSIST = 5
};

class FaultManager {
public:
    static void begin();
    static void update();

    static void setActive(FaultId id, bool active);
    static void notifyError();
    static void acknowledge();

    static bool hasActiveFaults();
    static bool hasUnacknowledgedErrors();
    static bool isAcknowledged();
    static uint32_t activeMask();

    static void resolveColor(uint8_t normalRed,
                             uint8_t normalGreen,
                             uint8_t normalBlue,
                             uint8_t& outRed,
                             uint8_t& outGreen,
                             uint8_t& outBlue);

private:
    static uint32_t _activeMask;
    static bool _unacknowledged;
    static bool _started;
};
