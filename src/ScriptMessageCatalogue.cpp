#include "ScriptMessageCatalogue.h"

#include <cstdlib>
#include <cstring>

#include "EventLog.h"
#include "StorageManager.h"

namespace ScriptMessageCatalogue {
namespace {

StorageManager* g_storage = nullptr;

// Taille maximale du fichier : MAX_ENTRIES lignes, chacune au plus
// "65535" + TAB + MAX_PHRASE + "\n". Marge pour le terminateur.
constexpr size_t FILE_CAP =
    static_cast<size_t>(MAX_ENTRIES) * (5U + 1U + MAX_PHRASE + 1U) + 16U;

// Ouvre /scripts/messages.tsv et le lit EN ENTIER dans un tampon de tas
// (le fichier est borne), plutot que de reprendre le mutex SD a chaque
// octet. Rend le tampon termine par '\0' -- a liberer par l'appelant.
// Rend nullptr si SD indisponible, fichier absent ou allocation impossible.
char* slurp() {
    if (!g_storage || !g_storage->isSdAvailable()) return nullptr;

    FsFile f;
    if (!g_storage->openRead(PATH, f)) return nullptr;  // absent => nullptr

    char* buf = static_cast<char*>(malloc(FILE_CAP));
    if (!buf) { g_storage->closeFile(f); return nullptr; }

    size_t total = 0U;
    while (total + 1U < FILE_CAP) {
        const int32_t got = g_storage->readChunk(
            f, reinterpret_cast<uint8_t*>(buf + total), FILE_CAP - 1U - total);
        if (got <= 0) break;
        total += static_cast<size_t>(got);
    }
    g_storage->closeFile(f);
    buf[total] = '\0';
    return buf;
}

// Decoupe une ligne "<code><TAB><phrase>". Rend false si la ligne n'a pas
// cette forme (pas de TAB, code hors 1..65535, phrase vide). Sinon pose
// *code et fait pointer *texte sur la phrase, terminee a \r/\n.
bool parseLine(char* line, uint16_t* code, char** texte) {
    char* tab = strchr(line, '\t');
    if (!tab) return false;
    *tab = '\0';

    char* end = nullptr;
    const long v = strtol(line, &end, 10);
    if (end == line || *end != '\0' || v < 1 || v > 65535) return false;

    char* txt = tab + 1;
    txt[strcspn(txt, "\r\n")] = '\0';
    if (txt[0] == '\0') return false;

    *code = static_cast<uint16_t>(v);
    *texte = txt;
    return true;
}

}  // namespace

void begin(StorageManager* storage) {
    g_storage = storage;
}

bool phrase(uint16_t code, char* out, size_t n) {
    if (!g_storage || !out || n == 0U) return false;
    // 0 = « pas de phrase » : inutile d'aller jusqu'a la carte.
    if (code == 0U) return false;

    char* buf = slurp();
    if (!buf) return false;

    bool found = false;
    char* save = nullptr;
    for (char* line = strtok_r(buf, "\n", &save);
         line != nullptr && !found;
         line = strtok_r(nullptr, "\n", &save)) {
        uint16_t c = 0U;
        char* txt = nullptr;
        if (parseLine(line, &c, &txt) && c == code) {
            strlcpy(out, txt, n);
            found = true;
        }
    }
    free(buf);
    return found;
}

bool load(JsonDocument& out) {
    out["max"] = MAX_ENTRIES;
    out["lenMax"] = MAX_PHRASE;
    JsonArray arr = out["entries"].to<JsonArray>();

    if (!g_storage) return false;
    // Carte montee mais /www absent : openRead echoue tout de meme. On rend
    // false pour que la route reponde 503 plutot que « catalogue vide ».
    if (!g_storage->isSdAvailable()) return false;

    FsFile probe;
    if (!g_storage->openRead(PATH, probe)) {
        // Fichier pas encore cree : ce n'est pas une erreur, l'editeur part
        // d'une liste vide.
        return true;
    }
    g_storage->closeFile(probe);

    char* buf = slurp();
    if (!buf) return false;

    char* save = nullptr;
    for (char* line = strtok_r(buf, "\n", &save);
         line != nullptr; line = strtok_r(nullptr, "\n", &save)) {
        uint16_t c = 0U;
        char* txt = nullptr;
        if (!parseLine(line, &c, &txt)) continue;
        JsonObject o = arr.add<JsonObject>();
        o["code"] = c;
        // String() force la copie dans le pool du document : `txt` pointe
        // dans `buf`, libere avant la serialisation par l'appelant.
        o["texte"] = String(txt);
    }
    free(buf);
    return true;
}

bool store(const String& body) {
    if (!g_storage || !g_storage->isCardMounted()) return false;

    String tmp = String(PATH) + ".tmp";
    FsFile f;
    if (!g_storage->openWrite(tmp.c_str(), f)) return false;

    const size_t len = body.length();
    const bool ok = (len == 0U) ||
        g_storage->writeChunk(
            f, reinterpret_cast<const uint8_t*>(body.c_str()), len)
            == static_cast<int32_t>(len);
    g_storage->closeFile(f);

    if (!ok) {
        g_storage->deleteOnSd(tmp.c_str());
        return false;
    }
    return g_storage->renameOnSd(tmp.c_str(), PATH);
}

}  // namespace ScriptMessageCatalogue
