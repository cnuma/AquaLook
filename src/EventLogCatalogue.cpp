#include "EventLogCatalogue.h"

#include <cstring>

#include "StorageManager.h"

namespace EventLogCatalogue {
namespace {

StorageManager* g_storage = nullptr;

// Taille maximale du fichier : MAX_ENTRIES lignes, chacune au plus
// MAX_CODE + TAB + MAX_PHRASE + "\n". Marge pour le terminateur.
constexpr size_t FILE_CAP =
    static_cast<size_t>(MAX_ENTRIES) * (MAX_CODE + 1U + MAX_PHRASE + 1U) + 16U;

// Ouvre /logs/messages.tsv et le lit EN ENTIER dans un tampon de tas (le
// fichier est borne), plutot que de reprendre le mutex SD a chaque octet.
// Rend le tampon termine par '\0' -- a liberer par l'appelant. Rend
// nullptr si SD indisponible, fichier absent ou allocation impossible.
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
// cette forme (pas de TAB, code vide ou trop long, phrase vide). Sinon
// fait pointer *codeOut/*texte sur le code et la phrase, terminee a \r/\n.
bool parseLine(char* line, char** codeOut, char** texte) {
    char* tab = strchr(line, '\t');
    if (!tab) return false;
    *tab = '\0';

    if (line[0] == '\0' || strlen(line) >= MAX_CODE) return false;

    char* txt = tab + 1;
    txt[strcspn(txt, "\r\n")] = '\0';
    if (txt[0] == '\0') return false;

    *codeOut = line;
    *texte = txt;
    return true;
}

}  // namespace

void begin(StorageManager* storage) {
    g_storage = storage;
}

bool phrase(const char* code, char* out, size_t n) {
    if (!g_storage || !code || !out || n == 0U) return false;
    if (code[0] == '\0') return false;

    char* buf = slurp();
    if (!buf) return false;

    bool found = false;
    char* save = nullptr;
    for (char* line = strtok_r(buf, "\n", &save);
         line != nullptr && !found;
         line = strtok_r(nullptr, "\n", &save)) {
        char* c = nullptr;
        char* txt = nullptr;
        if (parseLine(line, &c, &txt) && strcmp(c, code) == 0) {
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
    if (!g_storage->isSdAvailable()) return false;

    FsFile probe;
    if (!g_storage->openRead(PATH, probe)) {
        // Fichier pas encore cree : ce n'est pas une erreur, juste rien a
        // afficher en detail pour l'instant.
        return true;
    }
    g_storage->closeFile(probe);

    char* buf = slurp();
    if (!buf) return false;

    char* save = nullptr;
    for (char* line = strtok_r(buf, "\n", &save);
         line != nullptr; line = strtok_r(nullptr, "\n", &save)) {
        char* c = nullptr;
        char* txt = nullptr;
        if (!parseLine(line, &c, &txt)) continue;
        JsonObject o = arr.add<JsonObject>();
        o["code"] = String(c);
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

}  // namespace EventLogCatalogue
