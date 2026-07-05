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

#define SDA_PIN                 27
#define SCL_PIN                 22
#define XL9535_ADDR             0x20

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
