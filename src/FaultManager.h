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
    CONFIG_PERSIST = 5,
    // Allocation d'un tampon d'affichage impossible au reveil de l'ecran.
    // Sans ce defaut, l'echec serait muet : TFT_eSprite teste _created et
    // sort sans rien dessiner, donc l'ecran resterait fige sur son dernier
    // contenu, sans message ni trace. Ajoute le 17 aout 2026.
    DISPLAY_ALLOC = 6,
    // Memoire libre passee sous le seuil d'alerte. Signale AVANT la panne :
    // les trois defaillances du 16-17 aout 2026 (page non chargee, poignee de
    // main TLS en echec, plantages au demarrage) etaient toutes des epuisements
    // memoire, et toutes ont ete decouvertes par la panne alors que la
    // degradation etait mesurable en amont. Ajoute le 17 aout 2026.
    MEMORY_LOW = 7,
    // Heure inconnue depuis assez longtemps pour que ce ne soit plus le simple
    // delai de demarrage. Consequence directe et invisible autrement :
    // main.cpp n'appelle ScheduleManager::update() QUE si NTPManager est
    // synchronise, donc sans heure le module n'arrose plus du tout — sans rien
    // dire. L'horloge interne survit a un redemarrage logiciel (verifie le
    // 17 aout 2026 : apres esp_restart(), la premiere ligne de journal est deja
    // horodatee), mais pas a une coupure d'alimentation. Le scenario reel est
    // donc : coupure de courant, retour du courant sans reseau, et un arrosage
    // qui ne repart jamais. Ajoute le 17 aout 2026.
    TIME_UNSYNCED = 8,
    // Le module a redemarre plusieurs fois sans jamais tenir une periode de
    // fonctionnement stable, et s'est mis en mode degrade. Ajoute le 17 aout
    // 2026, apres deux boucles de redemarrages que rien dans le module n'a
    // arretees : le seul rempart avait ete un utilisateur debranchant la prise.
    BOOT_LOOP = 9
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
