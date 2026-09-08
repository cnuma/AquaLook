#pragma once

#include <stdint.h>

#include "config.h"

// Persistance des arrosages SUSPENDUS.
//
// POURQUOI CELA NE PEUT PAS RESTER EN MEMOIRE
//
// Une suspension attend un evenement exterieur -- une cuve qui se remplit --
// et peut durer des minutes. Le module, lui, redemarre chaque nuit pour sa
// verification de mise a jour, et peut planter. Une suspension gardee en RAM
// disparaitrait alors sans bruit : la zone ne reprendrait JAMAIS, et
// l'utilisateur constaterait un arrosage a moitie fait sans explication.
//
// LE TEMPS EST LE VRAI PROBLEME
//
// millis() repart de zero au demarrage : impossible de savoir, au retour,
// depuis combien de temps la suspension dure. On enregistre donc une date
// ABSOLUE (epoch), pas une duree.
//
// Et si l'horloge n'est pas credible au moment de restaurer, la suspension
// est ABANDONNEE plutot que reprise. C'est le choix prudent : rouvrir une
// vanne sans savoir si l'attente a dure dix minutes ou dix heures est
// exactement le geste qu'on ne veut pas faire sur un circuit d'eau.
//
// Meme discipline NVS que les autres magasins du projet : magic, version de
// schema, garde de longueur, CRC, et refus au moindre doute.

namespace PausedWateringStore {

struct Entry {
    uint16_t zoneId;        // identifiant STABLE, pas un index
    uint16_t remainingSec;
    uint32_t pausedAtEpoch; // date absolue, seule mesure qui traverse un reboot
};

// Enregistre l'etat courant des suspensions. count vaut 0 pour tout effacer.
bool save(const Entry* entries, uint8_t count);

// Relit les suspensions enregistrees. Retourne le nombre lu, 0 si rien
// d'enregistre ou si le bloc est refuse.
uint8_t load(Entry* entries, uint8_t capacity);

void clear();

} // namespace PausedWateringStore
