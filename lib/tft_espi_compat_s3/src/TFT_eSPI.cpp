#include "TFT_eSPI.h"

// ═══════════════════════════════════════════════════════════════
//  TFT_eSPI - dessine directement sur le panneau NV3041A (QSPI)
// ═══════════════════════════════════════════════════════════════

TFT_eSPI::TFT_eSPI() {}

void TFT_eSPI::init() {
    // Brochage/parametres : section [jc4827w543c_i] de platformio.ini,
    // construction identique a celle validee sur materiel reel par
    // test_screen_s3.cpp (ips=AQ_S3_LCD_IPS=1, rotation=0).
    Arduino_DataBus *bus = new Arduino_ESP32QSPI(
        AQ_S3_LCD_CS, AQ_S3_LCD_SCK, AQ_S3_LCD_D0, AQ_S3_LCD_D1, AQ_S3_LCD_D2, AQ_S3_LCD_D3);
    Arduino_NV3041A *panel = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, AQ_S3_LCD_IPS);
    panel->begin();
    _target = panel;

    ledcSetup(AQ_S3_LCD_BL_CHANNEL, 5000, 12);
    ledcAttachPin(AQ_S3_LCD_BL, AQ_S3_LCD_BL_CHANNEL);
    ledcWrite(AQ_S3_LCD_BL_CHANNEL, 4095);
}

void TFT_eSPI::setRotation(uint8_t /*r*/) {
    // No-op documente dans le .h : orientation deja correcte.
}

void TFT_eSPI::fillScreen(uint16_t color) {
    if (!_target) return;
    _target->fillScreen(color);
}

void TFT_eSPI::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
    if (!_target) return;
    _target->fillRect(x, y, w, h, color);
}

void TFT_eSPI::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
    if (!_target) return;
    _target->drawRect(x, y, w, h, color);
}

void TFT_eSPI::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint16_t color) {
    if (!_target) return;
    _target->fillRoundRect(x, y, w, h, radius, color);
}

void TFT_eSPI::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint16_t color) {
    if (!_target) return;
    _target->drawRoundRect(x, y, w, h, radius, color);
}

void TFT_eSPI::drawFastVLine(int32_t x, int32_t y, int32_t h, uint16_t color) {
    if (!_target) return;
    _target->drawFastVLine(x, y, h, color);
}

void TFT_eSPI::drawFastHLine(int32_t x, int32_t y, int32_t w, uint16_t color) {
    if (!_target) return;
    _target->drawFastHLine(x, y, w, color);
}

void TFT_eSPI::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color) {
    if (!_target) return;
    _target->drawLine(x0, y0, x1, y1, color);
}

void TFT_eSPI::fillCircle(int32_t x, int32_t y, int32_t r, uint16_t color) {
    if (!_target) return;
    _target->fillCircle(x, y, r, color);
}

void TFT_eSPI::fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t color) {
    if (!_target) return;
    _target->fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

void TFT_eSPI::setTextColor(uint16_t fg) {
    _fg = fg;
    _bgOpaque = false;
}

void TFT_eSPI::setTextColor(uint16_t fg, uint16_t bg) {
    _fg = fg;
    _bg = bg;
    _bgOpaque = true;
}

void TFT_eSPI::setTextSize(uint8_t s) {
    if (!_target) return;
    _target->setTextSize(s);
}

void TFT_eSPI::setFreeFont(const GFXfont *f) {
    _font = f;
    if (_target) _target->setFont(f);
}

void TFT_eSPI::setTextDatum(uint8_t datum) {
    _datum = datum;
}

void TFT_eSPI::applyTextStyle() {
    if (!_target) return;
    _target->setFont(_font);
    if (_bgOpaque) _target->setTextColor(_fg, _bg);
    else _target->setTextColor(_fg);
}

// TFT_eSPI::drawString positionne le texte selon le datum courant
// (TL/TC/TR/ML/MC/MR/BL/BC/BR_DATUM), une fonctionnalite sans
// equivalent direct dans Adafruit_GFX/Arduino_GFX (setCursor+print
// seulement). getTextBounds(str, 0, 0, &x1, &y1, &w, &h) donne, pour un
// curseur hypothetique en (0,0), le coin haut-gauche reel (x1,y1) et les
// dimensions (w,h) de la boite englobante visuelle du texte - x1/y1 sont
// generalement negatifs (l'ascender depasse au-dessus de la ligne de
// base, qui est l'origine du curseur). Le curseur reel a placer pour que
// la boite englobante ait son coin voulu a (targetX,targetY) est donc
// simplement (targetX - x1, targetY - y1), independamment de l'alignement.
void TFT_eSPI::drawString(const char *text, int32_t x, int32_t y) {
    if (!_target || !text) return;
    applyTextStyle();

    int16_t x1, y1;
    uint16_t w, h;
    _target->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    int32_t targetX = x;
    int32_t targetY = y;
    switch (_datum) {
        case TL_DATUM: break;
        case TC_DATUM: targetX -= (int32_t)w / 2; break;
        case TR_DATUM: targetX -= (int32_t)w; break;
        case ML_DATUM: targetY -= (int32_t)h / 2; break;
        case MC_DATUM: targetX -= (int32_t)w / 2; targetY -= (int32_t)h / 2; break;
        case MR_DATUM: targetX -= (int32_t)w; targetY -= (int32_t)h / 2; break;
        case BL_DATUM: targetY -= (int32_t)h; break;
        case BC_DATUM: targetX -= (int32_t)w / 2; targetY -= (int32_t)h; break;
        case BR_DATUM: targetX -= (int32_t)w; targetY -= (int32_t)h; break;
        default: break;
    }

    _target->setCursor(targetX - x1, targetY - y1);
    _target->print(text);
}

int32_t TFT_eSPI::textWidth(const char *text) {
    if (!_target || !text) return 0;
    applyTextStyle();
    int16_t x1, y1;
    uint16_t w, h;
    _target->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return (int32_t)w;
}

int32_t TFT_eSPI::fontHeight() {
    if (!_target) return 0;
    applyTextStyle();
    int16_t x1, y1;
    uint16_t w, h;
    _target->getTextBounds("Hg", 0, 0, &x1, &y1, &w, &h);
    return (int32_t)h;
}

void TFT_eSPI::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data) {
    if (!_target || !data) return;
    if (_swapBytes) _target->draw16bitBeRGBBitmap(x, y, data, w, h);
    else _target->draw16bitRGBBitmap(x, y, data, w, h);
}

void TFT_eSPI::setSwapBytes(bool swap) {
    _swapBytes = swap;
}

size_t TFT_eSPI::write(uint8_t c) {
    if (!_target) return 0;
    return _target->write(c);
}

// ═══════════════════════════════════════════════════════════════
//  TFT_eSprite - bufferise dans son propre Arduino_Canvas
// ═══════════════════════════════════════════════════════════════

TFT_eSprite::TFT_eSprite(TFT_eSPI *parent) : _parent(parent) {}

TFT_eSprite::~TFT_eSprite() {
    deleteSprite();
}

void *TFT_eSprite::createSprite(int16_t w, int16_t h) {
    // Idempotent : un sprite deja alloue est d'abord libere (voir
    // commentaire de DisplayManager.h::suspendForMemoryRelief - un
    // createSprite() sur un sprite deja alloue perdrait l'ancien tampon
    // si on ne le liberait pas avant).
    deleteSprite();

    _canvas = new Arduino_Canvas(w, h, _parent ? _parent->target() : nullptr);
    // GFX_SKIP_OUTPUT_BEGIN : le panneau/bus QSPI passe est deja
    // initialise par TFT_eSPI::init() - sans ce flag, Arduino_Canvas::
    // begin() rappelle _output->begin() (donc Arduino_ESP32QSPI::begin()
    // une deuxieme fois), qui abandonne avec "SPI bus already
    // initialized" (ESP_ERR_INVALID_STATE). Trouve par crash reel a la
    // creation du premier sprite (DisplayManager::createSprites()) le
    // 27 aout 2026 - backtrace decodee via addr2line, pas suppose.
    if (!_canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
        delete _canvas;
        _canvas = nullptr;
        _target = nullptr;
        return nullptr;
    }
    _target = _canvas;
    _w = w;
    _h = h;
    return _canvas->getFramebuffer();
}

void TFT_eSprite::fillSprite(uint16_t color) {
    if (!_canvas) return;
    _canvas->fillScreen(color);
}

void *TFT_eSprite::getPointer() {
    if (!_canvas) return nullptr;
    return _canvas->getFramebuffer();
}

void TFT_eSprite::pushSprite(int32_t x, int32_t y) {
    if (!_canvas || !_parent || !_parent->target()) return;
    _parent->target()->draw16bitRGBBitmap(x, y, _canvas->getFramebuffer(), _w, _h);
}

void TFT_eSprite::deleteSprite() {
    if (_canvas) {
        delete _canvas;
        _canvas = nullptr;
    }
    _target = nullptr;
    _w = 0;
    _h = 0;
}
