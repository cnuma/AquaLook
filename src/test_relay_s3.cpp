// Test isole n°6 du portage ESP32-S3 (docs/architecture/HW_JC4827W543_PORT_IMPACT.md, §8)
// Carte cible : Guition JC4827W543C_I - bloc relais XL9535 cable en externe
// sur un bus I2C dedie, distinct de celui du tactile GT911 (SCL=4/SDA=8) :
// SCL=GPIO17, SDA=GPIO18.
//
// Question a laquelle ce test repond : quels GPIO sont libres pour le bus
// relais, le XL9535 repond-il en 0x20 ? Critere de reussite : commutation
// reelle d'une voie, identifiee sans ambiguite. Decisif - fonction coeur
// du produit.
//
// Reecrit le 26 aout 2026 (v2) apres deux problemes constates sur le
// premier jet (voir historique git) :
//   1. Ce port USB natif (USB Serial/JTAG) reinitialise la carte a chaque
//      connexion serie - une sequence automatique au demarrage se
//      redeclenche donc a chaque reconnexion, faisant "bouger" les relais
//      de facon imprevisible pour qui regarde le bloc physique.
//   2. Un dump registre ne suffit pas a relier "ce qui a ete commande" a
//      "ce qui est observe sur les LED du bloc relais" sans ambiguite.
// v2 corrige les deux : demarrage TOUJOURS a l'etat OFF (aucune bascule
// automatique), et affichage en continu sur l'ecran QSPI (deja valide par
// test_screen_s3) de l'etat des 16 canaux, mis a jour a chaque commande.
//
// Logique appliquee : DIRECTE (bit=1 -> ON) uniquement, celle deja
// etablie en production pour la carte actuelle (main.cpp:245,
// RelayTopology::LOGIC_DIRECT). Canaux 0-7 = OUTPUT_P0 bit0-7,
// canaux 8-15 = OUTPUT_P1 bit0-7 (meme convention que RelaisManager.cpp).
//
// Autonome, ne depend pas de include/config.h (broches/adresse propres a
// la carte actuelle) : tout est redefini ici.

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

// Brochage/parametres carte : voir la section [jc4827w543c_i] de
// platformio.ini - source de verite unique, partagee par tous les
// env test_*_s3, plutot que des #define disperses et divergents.
#define REL_SCL_PIN   AQ_S3_RELAY_SCL
#define REL_SDA_PIN   AQ_S3_RELAY_SDA
#define XL9535_ADDR   AQ_S3_RELAY_ADDR

#define XL9535_REG_OUTPUT_P0  0x02
#define XL9535_REG_OUTPUT_P1  0x03
#define XL9535_REG_CONFIG_P0  0x06
#define XL9535_REG_CONFIG_P1  0x07

#define LCD_BL AQ_S3_LCD_BL
#define LCD_BL_CHANNEL AQ_S3_LCD_BL_CHANNEL

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    AQ_S3_LCD_CS, AQ_S3_LCD_SCK, AQ_S3_LCD_D0, AQ_S3_LCD_D1, AQ_S3_LCD_D2, AQ_S3_LCD_D3);
Arduino_NV3041A *panel = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, AQ_S3_LCD_IPS /* confirme le 26 aout */);
Arduino_GFX *gfx = new Arduino_Canvas(AQ_S3_SCREEN_WIDTH, AQ_S3_SCREEN_HEIGHT, panel);

static uint16_t g_outP0 = 0x00;
static uint16_t g_outP1 = 0x00;
static bool g_xl9535Ok = false;

static uint8_t writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(XL9535_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission();
}

static uint8_t readReg(uint8_t reg) {
    Wire.beginTransmission(XL9535_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0xFF;
    Wire.requestFrom((uint8_t)XL9535_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

// Ecriture OUTPUT avant CONFIG pour eviter un glitch a la mise en sortie
// (meme precaution que RelaisManager.cpp sur la carte actuelle).
static void initXL9535AllOff() {
    writeReg(XL9535_REG_OUTPUT_P0, 0x00);
    writeReg(XL9535_REG_OUTPUT_P1, 0x00);
    delay(5);
    writeReg(XL9535_REG_CONFIG_P0, 0x00);
    writeReg(XL9535_REG_CONFIG_P1, 0x00);
    delay(5);
    writeReg(XL9535_REG_OUTPUT_P0, 0x00);
    writeReg(XL9535_REG_OUTPUT_P1, 0x00);
    g_outP0 = 0x00;
    g_outP1 = 0x00;
}

static void setChannel(uint8_t channel, bool on) {
    uint8_t reg = channel < 8 ? XL9535_REG_OUTPUT_P0 : XL9535_REG_OUTPUT_P1;
    uint8_t bit = channel < 8 ? channel : channel - 8;
    uint16_t &shadow = channel < 8 ? g_outP0 : g_outP1;
    if (on) shadow |= (1U << bit);
    else shadow &= ~(1U << bit);
    writeReg(reg, (uint8_t)shadow);
}

// ── Affichage ecran : etat des 16 canaux + dernier registre lu ──────────

static void drawStatus(const char *lastAction) {
    gfx->fillScreen(0x0000);
    gfx->setTextColor(0xFFFF);
    gfx->setTextSize(2);

    gfx->setCursor(10, 10);
    gfx->print(F("AquaLook - test relais XL9535 (S3)"));

    gfx->setCursor(10, 35);
    gfx->printf("I2C SCL=17 SDA=18 addr=0x20 : %s", g_xl9535Ok ? "OK" : "ABSENT");

    gfx->setCursor(10, 60);
    gfx->printf("Logique DIRECTE (bit=1=ON)  P0=0x%02X  P1=0x%02X", g_outP0, g_outP1);

    // Grille 16 canaux, 2 lignes de 8, vert=ON / gris fonce=OFF
    for (uint8_t ch = 0; ch < 16; ch++) {
        bool on = ch < 8 ? (g_outP0 & (1U << ch)) : (g_outP1 & (1U << (ch - 8)));
        int col = ch % 8;
        int row = ch / 8;
        int x = 10 + col * 58;
        int y = 95 + row * 55;
        gfx->fillRoundRect(x, y, 50, 42, 6, on ? 0x07E0 : 0x39C7);
        gfx->drawRoundRect(x, y, 50, 42, 6, 0xFFFF);
        gfx->setTextColor(on ? 0x0000 : 0xFFFF);
        gfx->setTextSize(1);
        gfx->setCursor(x + 14, y + 8);
        gfx->printf("Z%d", ch + 1);
        gfx->setCursor(x + 6, y + 24);
        gfx->print(on ? F("ON") : F("off"));
    }

    gfx->setTextColor(0xFFE0);
    gfx->setTextSize(1);
    gfx->setCursor(10, 215);
    gfx->print(F("Derniere commande :"));
    gfx->setCursor(10, 230);
    gfx->print(lastAction);

    gfx->setTextColor(0x8410);
    gfx->setCursor(10, 255);
    gfx->print(F("Zx=ON/OFF direct (x=1..9,puis a,b,c,d,e,f pour Z10-16), o=tout OFF"));

    gfx->flush();
}

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(300);

    Serial.println();
    Serial.println(F("=== AquaLook - bring-up ESP32-S3 JC4827W543C_I - test 6/9 (v2) ==="));
    Serial.printf("Bus I2C relais : SCL=GPIO%d SDA=GPIO%d addr=0x%02X\n",
                  REL_SCL_PIN, REL_SDA_PIN, XL9535_ADDR);

    // Ecran d'abord : l'etat affiche doit etre correct des la premiere image,
    // y compris si le XL9535 est absent.
    ledcSetup(LCD_BL_CHANNEL, 5000, 12);
    ledcAttachPin(LCD_BL, LCD_BL_CHANNEL);
    ledcWrite(LCD_BL_CHANNEL, 4095);
    if (!gfx->begin()) {
        Serial.println(F("ECHEC gfx->begin() - suite du test en serie seul"));
    }

    Wire.begin(REL_SDA_PIN, REL_SCL_PIN);
    Wire.setClock(100000);
    delay(50);

    Wire.beginTransmission(XL9535_ADDR);
    g_xl9535Ok = (Wire.endTransmission() == 0);

    if (!g_xl9535Ok) {
        Serial.printf(">>> RESULTAT : ECHEC - XL9535 absent @ 0x%02X\n", XL9535_ADDR);
        Serial.println(F("    Verifier le cablage SCL/SDA et l'alimentation du bloc relais."));
    } else {
        Serial.printf(">>> XL9535 trouve @ 0x%02X\n", XL9535_ADDR);
        // Etat sur au demarrage : tout OFF, AUCUNE bascule automatique.
        // (v1 lancait ici un cycle inverse+directe a chaque boot - source
        // du comportement erratique constate le 26 aout avec un port qui
        // reset a chaque connexion serie.)
        initXL9535AllOff();
    }

    drawStatus("(demarrage, tout OFF)");

    Serial.println(F("\nCommandes (Serial Monitor) :"));
    Serial.println(F("  '1'-'9' -> Z1-Z9 ON (bascule)   'a'-'g' -> Z10-Z16 ON (bascule)"));
    Serial.println(F("  'o' -> tout OFF"));
    Serial.println(F("  'd' -> dump registres serie"));
    Serial.println(F("Rappel : ce port USB reinitialise la carte a la reconnexion -"));
    Serial.println(F("garder UNE seule session serie ouverte pour ne pas perdre l'etat.\n"));
}

static void dumpSerial() {
    Serial.printf("P0=0x%02X (lu=0x%02X)  P1=0x%02X (lu=0x%02X)\n",
                  g_outP0, readReg(XL9535_REG_OUTPUT_P0),
                  g_outP1, readReg(XL9535_REG_OUTPUT_P1));
}

void loop() {
    if (!Serial.available()) return;
    char c = Serial.read();
    if (c < 32) return;  // ignore CR/LF

    char msg[48] = {0};

    if (!g_xl9535Ok) {
        snprintf(msg, sizeof(msg), "'%c' ignoree - XL9535 absent", c);
        drawStatus(msg);
        Serial.println(msg);
        return;
    }

    if (c == 'o') {
        initXL9535AllOff();
        snprintf(msg, sizeof(msg), "'o' -> tout OFF");
    } else if (c == 'd') {
        dumpSerial();
        return;  // pas de changement d'etat, pas de redessin necessaire
    } else {
        int channel = -1;
        if (c >= '1' && c <= '9') channel = c - '1';            // Z1..Z9
        else if (c >= 'a' && c <= 'g') channel = 9 + (c - 'a'); // Z10..Z16
        if (channel < 0 || channel > 15) return;

        bool wasOn = channel < 8 ? (g_outP0 & (1U << channel))
                                 : (g_outP1 & (1U << (channel - 8)));
        setChannel((uint8_t)channel, !wasOn);
        snprintf(msg, sizeof(msg), "Z%d -> %s", channel + 1, wasOn ? "OFF" : "ON");
    }

    Serial.println(msg);
    dumpSerial();
    drawStatus(msg);
}
