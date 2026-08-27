// Test isole n°8 du portage ESP32-S3 (docs/architecture/HW_JC4827W543_PORT_IMPACT.md, §8)
// Carte cible : Guition JC4827W543C_I - lecteur microSD.
//
// Question a laquelle ce test repond : presence, brochage, montage,
// lecture et ecriture ? Critere de reussite : fichier ecrit puis relu a
// l'identique. Tout le service des pages Web et le canal de mise a jour
// en dependent.
//
// Brochage NON CONFIRME avant ce test (repris d'une source communautaire,
// voir platformio.ini [jc4827w543c_i]) : MISO=13, MOSI=11, SCLK=12,
// CS=10. Bus SPI DEDIE (HSPI), distinct du QSPI de l'ecran - la
// discussion Arduino_GFX #557 citee dans le document d'impact signale
// que gfx->flush() peut casser des acces SD partageant le meme
// peripherique SPI que l'ecran ; ce risque ne s'applique pas ici car les
// broches SD ne recoupent pas celles du LCD.
//
// L'utilisateur signale que cette carte SD est deja montee et devrait
// contenir les fichiers du projet (data/ - index.html, style-base.css,
// etc.) : ce test cherche explicitement index.html en plus du
// aller-retour ecriture/lecture generique, pour confirmer qu'il s'agit
// bien de la carte du projet et pas d'une carte vierge quelconque.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include <SD.h>

#define LCD_BL AQ_S3_LCD_BL
#define LCD_BL_CHANNEL AQ_S3_LCD_BL_CHANNEL

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    AQ_S3_LCD_CS, AQ_S3_LCD_SCK, AQ_S3_LCD_D0, AQ_S3_LCD_D1, AQ_S3_LCD_D2, AQ_S3_LCD_D3);
Arduino_NV3041A *panel = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, AQ_S3_LCD_IPS);
Arduino_GFX *gfx = new Arduino_Canvas(AQ_S3_SCREEN_WIDTH, AQ_S3_SCREEN_HEIGHT, panel);

static SPIClass spiSD(HSPI);

static const char *kTestPath = "/aqualook_test_s3.txt";
static const char *kTestContent = "AquaLook - test SD ESP32-S3 JC4827W543C_I - 27 aout 2026";

struct Line {
    char text[64];
};
static Line g_lines[10];
static uint8_t g_lineCount = 0;

static void addLine(const char *fmt, ...) {
    if (g_lineCount >= 10) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_lines[g_lineCount].text, sizeof(g_lines[g_lineCount].text), fmt, args);
    va_end(args);
    Serial.println(g_lines[g_lineCount].text);
    g_lineCount++;
}

static void drawResults() {
    gfx->fillScreen(0x0000);
    gfx->setTextColor(0xFFFF);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->print(F("AquaLook - test 8/9 : carte SD"));
    gfx->setTextSize(1);
    for (uint8_t i = 0; i < g_lineCount; i++) {
        gfx->setCursor(10, 45 + i * 20);
        gfx->print(g_lines[i].text);
    }
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
    Serial.println(F("=== AquaLook - bring-up ESP32-S3 JC4827W543C_I - test 8/9 ==="));
    Serial.printf("SD (bus SPI dedie) : MISO=%d MOSI=%d SCLK=%d CS=%d\n",
                  AQ_S3_SD_MISO, AQ_S3_SD_MOSI, AQ_S3_SD_SCLK, AQ_S3_SD_CS);

    ledcSetup(LCD_BL_CHANNEL, 5000, 12);
    ledcAttachPin(LCD_BL, LCD_BL_CHANNEL);
    ledcWrite(LCD_BL_CHANNEL, 4095);
    gfx->begin();
    gfx->fillScreen(0x0000);
    gfx->flush();

    spiSD.begin(AQ_S3_SD_SCLK, AQ_S3_SD_MISO, AQ_S3_SD_MOSI, AQ_S3_SD_CS);

    if (!SD.begin(AQ_S3_SD_CS, spiSD)) {
        addLine("ECHEC SD.begin() - carte absente ou brochage faux");
        addLine("Broches essayees : MISO=%d MOSI=%d SCLK=%d CS=%d",
                AQ_S3_SD_MISO, AQ_S3_SD_MOSI, AQ_S3_SD_SCLK, AQ_S3_SD_CS);
        drawResults();
        return;
    }

    uint8_t cardType = SD.cardType();
    const char *typeName = cardType == CARD_MMC   ? "MMC"
                            : cardType == CARD_SD  ? "SDSC"
                            : cardType == CARD_SDHC ? "SDHC"
                                                    : "INCONNU/ABSENT";
    uint64_t sizeMB = SD.cardSize() / (1024ULL * 1024ULL);
    addLine("Carte montee : type=%s taille=%lu Mo", typeName, (unsigned long)sizeMB);

    // Racine : contenu present, et recherche explicite de index.html pour
    // confirmer qu'il s'agit de la carte du projet (data/) et non d'une
    // carte vierge.
    File root = SD.open("/");
    int fileCount = 0;
    bool foundIndex = false;
    if (root) {
        File entry = root.openNextFile();
        while (entry) {
            fileCount++;
            if (!entry.isDirectory() && strcmp(entry.name(), "index.html") == 0) {
                foundIndex = true;
                addLine("Trouve index.html : %lu octets", (unsigned long)entry.size());
            }
            entry.close();
            entry = root.openNextFile();
        }
        root.close();
    }
    addLine("Racine : %d entrees", fileCount);
    if (!foundIndex) {
        addLine("index.html NON trouve a la racine (carte du projet ?)");
    }

    // Aller-retour ecriture/lecture - critere de reussite du test 8.
    File w = SD.open(kTestPath, FILE_WRITE);
    bool writeOk = false;
    if (w) {
        writeOk = (w.print(kTestContent) == (int)strlen(kTestContent));
        w.close();
    }
    if (!writeOk) {
        addLine("ECHEC ecriture %s", kTestPath);
        drawResults();
        return;
    }

    File r = SD.open(kTestPath, FILE_READ);
    bool readOk = false;
    if (r) {
        char buf[128] = {0};
        size_t n = r.readBytes(buf, sizeof(buf) - 1);
        r.close();
        readOk = (n == strlen(kTestContent)) && (memcmp(buf, kTestContent, n) == 0);
    }
    SD.remove(kTestPath);

    if (readOk) {
        addLine(">>> RESULTAT test 8 : OK - ecrit puis relu a l'identique");
    } else {
        addLine(">>> RESULTAT test 8 : ECHEC - relecture differente de l'ecriture");
    }

    drawResults();
}

void loop() {
    delay(1000);
}
