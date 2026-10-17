#include "PanelSetup.h"

#include <Preferences.h>

#include "EventLog.h"

namespace PanelSetup {
namespace {

constexpr const char* NVS_NAMESPACE = "aqlhw";
constexpr const char* KEY_PANEL = "panel";
constexpr uint32_t MAGIC = 0x4C4E4150UL;  // "PANL"
constexpr uint8_t VERSION = 1;

// Bits de PanelBlob::flags.
constexpr uint8_t F_SCREEN_FLIP = 0x01;
constexpr uint8_t F_TOUCH_FLIP = 0x02;
constexpr uint8_t F_INVERT = 0x04;
constexpr uint8_t F_KNOWN = F_SCREEN_FLIP | F_TOUCH_FLIP | F_INVERT;

struct PanelBlob {
    uint32_t magic;
    uint8_t version;
    uint8_t flags;
    uint16_t check;  // ~(version, flags) : un bloc tronque ou ecrase se voit
};

uint8_t g_flags = 0;
bool g_needsWizard = false;

uint16_t checkOf(uint8_t version, uint8_t flags) {
    return static_cast<uint16_t>(~((static_cast<uint16_t>(version) << 8) | flags));
}

bool readBlob(uint8_t& flags) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return false;
    PanelBlob b = {};
    const size_t n = prefs.getBytesLength(KEY_PANEL) == sizeof(b)
                         ? prefs.getBytes(KEY_PANEL, &b, sizeof(b))
                         : 0;
    prefs.end();
    if (n != sizeof(b) || b.magic != MAGIC || b.version != VERSION ||
        b.check != checkOf(b.version, b.flags) || (b.flags & ~F_KNOWN) != 0) {
        return false;
    }
    flags = b.flags;
    return true;
}

void logState(const char* origin) {
    EventLog::log(LOG_INFO, "[ECRAN] dalle %s : image %s, tactile %s, couleurs %s", origin,
                  (g_flags & F_SCREEN_FLIP) ? "retournee" : "normale",
                  touchMirrored() ? "retourne" : "normal",
                  (g_flags & F_INVERT) ? "inversees" : "normales");
}

}  // namespace

void begin(bool configWasFresh) {
    uint8_t flags = 0;
    if (readBlob(flags)) {
        g_flags = flags;
        g_needsWizard = false;
        logState("enregistree");
        return;
    }
    g_flags = 0;
    if (configWasFresh) {
        g_needsWizard = true;
        EventLog::log(LOG_INFO, "[ECRAN] module neuf : assistant de reglage de l'ecran");
        return;
    }
    // Module deja en service, mis a jour vers une version qui connait ce
    // reglage : il fonctionnait avec les valeurs de compilation, on les fige.
    save();
}

bool needsWizard() { return g_needsWizard; }
bool screenFlipped() { return (g_flags & F_SCREEN_FLIP) != 0; }
bool colorsInverted() { return (g_flags & F_INVERT) != 0; }
bool touchFlipRaw() { return (g_flags & F_TOUCH_FLIP) != 0; }

bool touchMirrored() {
    // Retourner l'image retourne aussi le sens de lecture attendu du tactile ;
    // F_TOUCH_FLIP corrige l'ecart propre a la dalle.
    return screenFlipped() != touchFlipRaw();
}

void preview(bool screenFlip, bool touchFlip, bool invertColors) {
    g_flags = (screenFlip ? F_SCREEN_FLIP : 0) | (touchFlip ? F_TOUCH_FLIP : 0) |
              (invertColors ? F_INVERT : 0);
}

bool save() {
    PanelBlob b = {MAGIC, VERSION, g_flags, checkOf(VERSION, g_flags)};
    Preferences prefs;
    bool ok = prefs.begin(NVS_NAMESPACE, false) &&
              prefs.putBytes(KEY_PANEL, &b, sizeof(b)) == sizeof(b);
    prefs.end();
    if (!ok) {
        EventLog::log(LOG_ERROR, "[ECRAN] reglage de la dalle non enregistre (NVS)");
        return false;
    }
    g_needsWizard = false;
    logState("enregistree");
    return true;
}

}  // namespace PanelSetup
