#include "ScriptMessageCatalogue.h"

#include <cstdlib>
#include <cstring>

#include "EventLog.h"
#include "StorageManager.h"

namespace ScriptMessageCatalogue {
namespace {

StorageManager* g_storage = nullptr;

// Taille maximale du fichier : MAX_ENTRIES lignes, chacune au plus
// "65535" + TAB + MAX_PHRASE + "\n". Large de quelques octets pour le
// terminateur.
constexpr size_t FILE_CAP =
    static_cast<size_t>(MAX_ENTRIES) * (5U + 1U + MAX_PHRASE + 1U) + 16U;

}  // namespace

void begin(StorageManager* storage) {
    g_storage = storage;
}

bool phrase(uint16_t code, char* out, size_t n) {
    if (!g_storage || !out || n == 0U) return false;
    // 0 = « pas de phrase » : un script qui ecrit « notifier 0 » ne veut
    // rien resoudre. On ne va pas jusqu'a la carte pour ca.
    if (code == 0U) return false;
    if (!g_storage->isSdAvailable()) return false;

    FsFile f;
    if (!g_storage->openRead(PATH, f)) return false;  // absent => false

    // Le fichier est borne (MAX_ENTRIES lignes courtes) : on le lit en
    // entier dans un tampon de tas, plutot que de reprendre le mutex SD a
    // chaque octet. L'echec d'allocation retombe sur le code nu, sans faire
    // echouer le script.
    char* buf = static_cast<char*>(malloc(FILE_CAP));
    if (!buf) { g_storage->closeFile(f); return false; }

    size_t total = 0U;
    while (total + 1U < FILE_CAP) {
        const int32_t got = g_storage->readChunk(
            f, reinterpret_cast<uint8_t*>(buf + total), FILE_CAP - 1U - total);
        if (got <= 0) break;
        total += static_cast<size_t>(got);
    }
    g_storage->closeFile(f);
    buf[total] = '\0';

    bool found = false;
    char* save = nullptr;
    for (char* line = strtok_r(buf, "\n", &save);
         line != nullptr && !found;
         line = strtok_r(nullptr, "\n", &save)) {
        char* tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = '\0';

        char* end = nullptr;
        const long v = strtol(line, &end, 10);
        if (end == line || *end != '\0' || v < 1 || v > 65535) continue;
        if (static_cast<uint16_t>(v) != code) continue;

        char* txt = tab + 1;
        txt[strcspn(txt, "\r\n")] = '\0';
        strlcpy(out, txt, n);
        found = true;
    }

    free(buf);
    return found;
}

}  // namespace ScriptMessageCatalogue
