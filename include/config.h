#pragma once

#define WIFI_SSID               ""
#define WIFI_PASSWORD           ""
#define WIFI_RETRY_INTERVAL     30000UL

#define NTP_SERVER1             "pool.ntp.org"
#define NTP_SERVER2             "time.nist.gov"
#define GMT_OFFSET              3600L
#define DST_OFFSET              3600L
#define NTP_SYNC_INTERVAL       3600000UL

#define OWM_API_KEY             ""
#define OWM_CITY                ""
#define OWM_COUNTRY             "FR"
#define OWM_CHECK_INTERVAL_MS   7200000UL

#if AQUALOOK_BOARD_S3
// Guition JC4827W543C_I (ESP32-S3) - broches confirmees sur materiel
// reel (docs/architecture/HW_JC4827W543_PORT_IMPACT.md §8), memes
// valeurs que la section [jc4827w543c_i] de platformio.ini (AQ_S3_*).
// Bus I2C relais DEDIE, distinct du tactile - propre a ce banc de test,
// pas une caracteristique intrinseque de la carte.
#define SDA_PIN                 AQ_S3_RELAY_SDA
#define SCL_PIN                 AQ_S3_RELAY_SCL
#define XL9535_ADDR             AQ_S3_RELAY_ADDR

// TAMC_GT911 (bibliotheque tierce) est cablee en dur sur l'objet Wire
// global - son begin() y appelle Wire.begin(4,8), ce qui deplacerait le
// bus deja configure sur 17/18 pour le relais si les deux partageaient
// le meme peripherique I2C. Trouve par crash/erreurs reels le 27 aout
// 2026 (flot d'erreurs "i2cRead returned Error -1" en continu - le
// relais retentait sur des broches qui n'etaient plus les siennes).
// Le relais utilise donc Wire1 (deuxieme peripherique I2C materiel de
// l'ESP32-S3), laissant Wire entierement au tactile.
#define RELAY_WIRE_BUS Wire1

// Pas de voyant RGB embarque sur cette carte (constate le 25 aout 2026,
// §8 test 7) - ScreenManager.cpp n'attache plus ces broches
// physiquement pour AQUALOOK_BOARD_S3 (voir ScreenManager.cpp), ces
// constantes ne sont donc plus lues, mais definies par coherence.
#define RGB_LED_RED_PIN         4
#define RGB_LED_GREEN_PIN       16
#define RGB_LED_BLUE_PIN        17
#define RGB_LED_ACTIVE_LOW      0

// Tactile GT911 capacitif - pas d'etalonnage (§5 du document d'impact),
// ces 4 constantes restent definies pour compiler mais ne sont plus
// lues par DisplayManager::getTouchPoint() sur cette carte (voir
// AQUALOOK_TOUCH_GT911 dans DisplayManager.cpp).
#define TOUCH_IRQ               AQ_S3_TOUCH_INT
#define TOUCH_MOSI              -1
#define TOUCH_MISO              -1
#define TOUCH_CLK               -1
#define TOUCH_CS                -1

// Carte SD, bus SPI dedie distinct du QSPI ecran (confirme sur materiel
// reel - SDHC, ecriture/lecture identique).
#define SD_CS_PIN               AQ_S3_SD_CS
#define SD_SCLK_PIN             AQ_S3_SD_SCLK
#define SD_MISO_PIN             AQ_S3_SD_MISO
#define SD_MOSI_PIN             AQ_S3_SD_MOSI
#define SD_SPI_FREQUENCY        10000000UL
#else
#define SDA_PIN                 27
#define SCL_PIN                 22
#define XL9535_ADDR             0x20
#define RELAY_WIRE_BUS Wire  // bus unique, pas de tactile I2C a menager ici

#define RGB_LED_RED_PIN         4
#define RGB_LED_GREEN_PIN       16
#define RGB_LED_BLUE_PIN        17
#define RGB_LED_ACTIVE_LOW      0

#define TOUCH_IRQ               36
#define TOUCH_MOSI              32
#define TOUCH_MISO              39
#define TOUCH_CLK               25
#define TOUCH_CS                33

// Lecteur microSD integre a la CYD ESP32-2432S028R.
// Bus SPI dedie, distinct des broches TFT et tactile configurees plus haut.
#define SD_CS_PIN               5
#define SD_SCLK_PIN             18
#define SD_MISO_PIN             19
#define SD_MOSI_PIN             23
#define SD_SPI_FREQUENCY        10000000UL
#endif

#define TOUCH_X_MIN             300
#define TOUCH_X_MAX             3758
#define TOUCH_Y_MIN             324
#define TOUCH_Y_MAX             3790

#define MAX_ZONES               16
#define MAX_RELAIS              16
#define NB_ZONES                2

#define NB_DAYS                 7
#define MAX_SLOTS               5

#define DEFAULT_RAIN_THRESHOLD  2.0f
#define DEFAULT_FORECAST_HOURS  24
#define MAX_FORECAST_HOURS      48

#define MAX_WATERING_DURATION_MS (3600000UL)

#define SCHEDULE_MODE_DAYS      0
#define SCHEDULE_MODE_INTERVAL  1

#define MANUAL_WATERING_DURATION_MIN 10
