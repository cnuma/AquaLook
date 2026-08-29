#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>

// ═══════════════════════════════════════════════════════════════
//  Mesures memoire sures - point de passage OBLIGATOIRE
//
//  Certaines fonctions de l'API tas parcourent le tas bloc par bloc
//  (heap_caps_get_largest_free_block et heap_caps_get_info descendent
//  toutes deux dans tlsf_walk_pool). Ce parcours est proportionnel au
//  nombre de blocs, donc a la taille ET a la fragmentation du tas.
//
//  Sur la carte ESP32-S3 JC4827W543C_I (8 Mo de PSRAM) il devient assez
//  long pour depasser le delai du chien de garde d'INTERRUPTION, ce qui
//  provoque une panique "Interrupt wdt timeout on CPU1" - pas un simple
//  ralentissement, un redemarrage.
//
//  Ce piege a mordu TROIS fois, a trois endroits differents, avant que
//  cet en-tete existe :
//    - 27 aout 2026 : SystemDiagnostics::sampleMemory(), depuis loop()
//    - 29 aout 2026 : SystemDiagnostics::fillJson(), depuis la tache
//      AsyncTCP - pire cas, car une page de diagnostic ouverte dans un
//      navigateur relance la requete a chaque redemarrage : le module
//      est parti en boucle de 45 cycles
//    - 29 aout 2026 : ESP.getHeapSize() dans la meme fonction, corrige
//      juste apres le precedent, sans avoir cherche les autres appels
//
//  D'ou ce point de passage unique : toute mesure memoire du firmware
//  passe par ici, pour qu'une correction n'ait plus jamais a etre
//  refaite site par site. Ne pas rappeler directement les fonctions de
//  l'IDF ailleurs dans le code applicatif.
// ═══════════════════════════════════════════════════════════════

namespace AquaLook {
namespace Heap {

// RAM INTERNE, et non MALLOC_CAP_8BIT seul.
//
// Sur la carte JC4827W543C_I, MALLOC_CAP_8BIT englobe les 8 Mo de PSRAM :
// toute mesure fondee dessus repond ~8,4 Mo en permanence et devient
// aveugle. Constate le 29 aout 2026 - le "minimum de tas libre observe"
// remonte valait 8 391 087 octets, soit plus que le tas lui-meme.
//
// Consequences reelles, pas theoriques : le seuil de memoire basse ne
// pouvait plus se declencher, et la garde qui protege la poignee de main
// TLS de CloudSync laissait demarrer une synchronisation sans RAM interne
// disponible - alors que c'est precisement ce qu'elle existe pour empecher.
//
// La RAM interne est la vraie ressource rare : piles de taches, tampons
// DMA et mbedTLS n'y echappent pas (CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=1).
// La PSRAM ne les remplace pas, elle n'accueille que les gros tampons
// applicatifs comme les sprites.
//
// Sur la carte historique, sans PSRAM, ce masque donne exactement les
// memes valeurs qu'avant : aucun changement de comportement.
constexpr uint32_t INTERNAL_CAPS = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

// Interrogent des compteurs tenus a jour par l'allocateur : pas de
// parcours, cout constant. Sans danger dans n'importe quel contexte.
inline uint32_t freeBytes() {
    return static_cast<uint32_t>(heap_caps_get_free_size(INTERNAL_CAPS));
}

inline uint32_t minFreeBytes() {
    return static_cast<uint32_t>(heap_caps_get_minimum_free_size(INTERNAL_CAPS));
}

inline uint32_t freePsramBytes() {
    return static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// Plus gros bloc libre contigu. C'est le meilleur predicteur d'echec
// d'allocation (une allocation echoue faute de bloc assez grand, pas
// faute de total), mais sa mesure exacte impose le parcours dangereux.
//
// Sur S3 on rend donc la taille libre TOTALE : valeur optimiste, jamais
// inferieure a la vraie. Un consommateur qui compare a un seuil reste
// donc du bon cote pour detecter une penurie franche, mais ne verra pas
// une fragmentation pure. Compromis assume tant qu'aucune variante
// bornee en temps n'existe.
inline uint32_t largestFreeBlock() {
#if AQUALOOK_BOARD_S3
    return freeBytes();
#else
    return static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
}

// Tailles TOTALES du tas interne et de la PSRAM. Ce sont des constantes
// materielles, mais l'IDF ne les expose qu'au travers d'un parcours
// complet (heap_caps_get_info). Elles sont donc mesurees une seule fois
// et memorisees - voir warmUp(), appelee tot au demarrage, avant que le
// WiFi ne soit actif et que le chien de garde ne devienne sensible.
uint32_t totalHeapBytes();
uint32_t totalPsramBytes();

// A appeler une fois au demarrage, avant l'activation du WiFi, pour que
// les mesures couteuses ci-dessus soient faites au moment le plus sur.
void warmUp();

}  // namespace Heap
}  // namespace AquaLook
