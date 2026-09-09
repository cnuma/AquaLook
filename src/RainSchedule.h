#pragma once

#include <stdint.h>

#include "ScheduleManager.h"
#include "WeatherManager.h"

// Predicats "la pluie bloque-t-elle l'arrosage" et "ce jour est-il prevu en
// mode intervalle", PARTAGES entre le LCD (DisplayManager) et l'API Web
// (WebManager).
//
// Ces deux regles vivaient auparavant dupliquees en `static` dans
// DisplayManager.cpp -- exactement le travers que ce fichier lui-meme
// denoncait deja pour un autre cas ("une regle metier recopiee finit
// toujours par diverger d'un site a l'autre"). Le voyant WS2812, le LCD et
// l'API Web doivent dire tous les trois la MEME chose sur un meme jour et
// une meme zone ; ecrire la regle une fois est ce qui le garantit, pas une
// discipline de copier-coller.

namespace RainSchedule {

// Arrosage prevu ce jour-la (au moins un creneau active), mais suspendu
// parce que la pluie annoncee atteint le seuil de la zone. `col` est le
// decalage en jours par rapport a aujourd'hui (0 = aujourd'hui), dans la
// limite de ce que WeatherManager sait prevoir (5 jours).
bool rainBlocksDay(
    const ZoneSchedule& zs,
    const DaySchedule& ds,
    uint8_t col,
    const WeatherManager* weather
);

// En mode intervalle, indique si le jour todayEpochDay+daysAhead tombe sur
// un jour d'arrosage prevu (ancre + multiple de l'intervalle).
bool intervalDayIsPlanned(
    const ZoneSchedule& zs,
    uint32_t todayEpochDay,
    uint8_t daysAhead
);

} // namespace RainSchedule
