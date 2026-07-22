#include <Arduino.h>

// IMPORTANT:
// Ne jamais acceder a Preferences/NVS depuis initVariant() sur cette cible.
// Avec Arduino-ESP32 / PlatformIO espressif32 6.13.0, ce hook intervient trop
// tot dans l'initialisation du runtime et corrompt l'etat du tas. Cela provoque
// ensuite un LoadProhibited dans heap_caps_get_largest_free_block() au debut de
// SystemDiagnostics::begin().
//
// L'entree maintenance sera executee depuis le debut de setup(), apres
// l'initialisation Arduino, mais avant TFT, Web, meteo et runtime complet.
void initVariant() {
    // Intentionnellement vide.
}
