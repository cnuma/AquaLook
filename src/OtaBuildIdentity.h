#pragma once

#ifndef AQUALOOK_VERSION
#define AQUALOOK_VERSION "unknown"
#endif

#ifndef AQUALOOK_BUILD_NUMBER
#define AQUALOOK_BUILD_NUMBER "local"
#endif

#ifndef AQUALOOK_GIT_SHA
#define AQUALOOK_GIT_SHA "unknown"
#endif

#ifndef AQUALOOK_GIT_BRANCH
#define AQUALOOK_GIT_BRANCH "unknown"
#endif

#ifndef AQUALOOK_OTA_TARGET
#define AQUALOOK_OTA_TARGET "legacy"
#endif

#ifndef AQUALOOK_PIO_ENV
#define AQUALOOK_PIO_ENV "ProgrammeArrosage"
#endif

namespace OtaBuildIdentity {
constexpr char PRODUCT[] = "AquaLook";
constexpr char BOARD[] = "esp32-2432S028";
constexpr char MANIFEST_HOST[] = "github.com";
constexpr char MANIFEST_PATH[] = "/cnuma/AquaLook/releases/latest/download/aqualook-manifest.json";
constexpr char VERSION[] = AQUALOOK_VERSION;
constexpr char BUILD_NUMBER[] = AQUALOOK_BUILD_NUMBER;
constexpr char GIT_SHA[] = AQUALOOK_GIT_SHA;
constexpr char GIT_BRANCH[] = AQUALOOK_GIT_BRANCH;
constexpr char OTA_TARGET[] = AQUALOOK_OTA_TARGET;
constexpr char PLATFORMIO_ENVIRONMENT[] = AQUALOOK_PIO_ENV;
}
