#pragma once

#include <ArduinoJson.h>

// Autotest de la machine a scripts, execute sur la cible et lisible par HTTP.
//
// Il n'existe pas de compilateur hote dans cet environnement : la machine ne
// peut donc pas etre eprouvee sur un poste avant d'arriver ici. Elle l'est
// la ou elle tournera, ce qui vaut mieux qu'une validation par relecture.
//
// Remplit doc avec un tableau "cas" et un verdict global. Retourne true si
// tous les cas passent.
bool runScriptVmSelfTest(JsonDocument& doc);
