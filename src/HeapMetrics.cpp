#include "HeapMetrics.h"

namespace AquaLook {
namespace Heap {

namespace {
// 0 = pas encore mesure. Ces deux valeurs sont des constantes materielles
// (taille totale du tas interne et de la PSRAM) : les relire a chaque
// appel ne changerait rien au resultat mais refarait le parcours complet
// du tas, qui est precisement ce que cet en-tete existe pour eviter.
uint32_t s_totalHeap  = 0;
uint32_t s_totalPsram = 0;

uint32_t measureTotal(uint32_t caps) {
    multi_heap_info_t info{};
    heap_caps_get_info(&info, caps);
    return static_cast<uint32_t>(info.total_free_bytes + info.total_allocated_bytes);
}
}  // namespace

uint32_t totalHeapBytes() {
    if (s_totalHeap == 0) s_totalHeap = measureTotal(MALLOC_CAP_INTERNAL);
    return s_totalHeap;
}

uint32_t totalPsramBytes() {
    // Pas de garde "deja mesure" possible via la valeur elle-meme sur une
    // carte sans PSRAM, ou le total vaut legitimement 0 : on mesurerait
    // alors a chaque appel. D'ou l'indicateur separe.
    static bool measured = false;
    if (!measured) {
        s_totalPsram = measureTotal(MALLOC_CAP_SPIRAM);
        measured = true;
    }
    return s_totalPsram;
}

void warmUp() {
    totalHeapBytes();
    totalPsramBytes();
}

}  // namespace Heap
}  // namespace AquaLook
