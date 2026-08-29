// Test d'orientation du 28 aout 2026 - Guition JC4827W543C_I (NV3041A).
//
// Pourquoi ce test : test_screen_s3.cpp avait valide couleurs et absence
// d'inversion avec une mire d'aplats - un aplat de couleur ne peut pas
// reveler une erreur d'orientation. Des que du vrai contenu (texte, mise
// en page) a ete affiche, le rendu est apparu tourne de 90° par rapport
// au bandeau paysage du module. Deux constructions ont ete essayees sans
// succes (dims 480x272 rot=0, puis dims 272x480 rot=1, cette derniere
// ajoutant un enroulement : fenetre de 480 px de large ecrite alors que
// l'adressage n'en accepte que 272).
//
// Plutot que de deviner la bonne combinaison en enchainant les flashs,
// ce test balaie les 4 rotations, 5 s chacune, avec des reperes sans
// ambiguite (coins de couleur nommes + texte + dimensions affichees).
// Un seul flash suffit alors a identifier la bonne valeur a l'oeil.
//
// A supprimer une fois l'orientation figee dans lib/tft_espi_compat_s3.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#define LCD_BL AQ_S3_LCD_BL
#define LCD_BL_CHANNEL AQ_S3_LCD_BL_CHANNEL

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    AQ_S3_LCD_CS, AQ_S3_LCD_SCK, AQ_S3_LCD_D0, AQ_S3_LCD_D1, AQ_S3_LCD_D2, AQ_S3_LCD_D3);

// Dimensions natives par defaut de la bibliotheque (480x272), rotation
// appliquee ensuite explicitement par setRotation() dans la boucle.
Arduino_NV3041A *panel = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, AQ_S3_LCD_IPS);

static void drawFrame(uint8_t rot) {
    panel->setRotation(rot);

    const int16_t w = panel->width();
    const int16_t h = panel->height();

    panel->fillScreen(BLACK);

    // Coins reperes : chacun d'une couleur distincte, pour lever toute
    // ambiguite de miroir (un simple "haut/bas" ne distingue pas une
    // symetrie horizontale d'une rotation de 180°).
    const int16_t m = 40;
    panel->fillRect(0, 0, m, m, RED);                 // haut gauche
    panel->fillRect(w - m, 0, m, m, GREEN);           // haut droit
    panel->fillRect(0, h - m, m, m, BLUE);            // bas gauche
    panel->fillRect(w - m, h - m, m, m, YELLOW);      // bas droit

    // Bordure : si elle n'est pas entierement visible, la fenetre
    // d'adressage deborde de la dalle (enroulement).
    panel->drawRect(0, 0, w, h, WHITE);

    panel->setTextColor(WHITE);
    panel->setTextSize(3);
    panel->setCursor(m + 10, 12);
    panel->printf("ROT=%u", rot);

    panel->setTextSize(2);
    panel->setCursor(m + 10, 48);
    panel->printf("%dx%d", w, h);

    panel->setTextSize(1);
    panel->setCursor(m + 10, 74);
    panel->print("ROUGE=haut gauche");
    panel->setCursor(m + 10, 88);
    panel->print("VERT=haut droit");
    panel->setCursor(m + 10, 102);
    panel->print("BLEU=bas gauche");
    panel->setCursor(m + 10, 116);
    panel->print("JAUNE=bas droit");

    Serial.printf("ROT=%u -> %dx%d\n", rot, w, h);
}

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 3000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(300);

    Serial.println();
    Serial.println(F("=== Balayage des 4 rotations NV3041A - 28 aout 2026 ==="));
    Serial.println(F("Chercher : texte lisible a l'endroit, bandeau tenu en"));
    Serial.println(F("paysage, ROUGE reellement en haut a gauche, bordure"));
    Serial.println(F("blanche entierement visible sur les 4 cotes."));

    panel->begin();

    ledcSetup(LCD_BL_CHANNEL, 5000, 12);
    ledcAttachPin(LCD_BL, LCD_BL_CHANNEL);
    ledcWrite(LCD_BL_CHANNEL, 4095);
}

void loop() {
    for (uint8_t rot = 0; rot < 4; rot++) {
        drawFrame(rot);
        delay(5000);
    }
}
