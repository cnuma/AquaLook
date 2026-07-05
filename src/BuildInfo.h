#pragma once

#ifndef AQUALOOK_VERSION
#define AQUALOOK_VERSION "5.8.0-rc1"
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

namespace BuildInfo {
constexpr const char* PRODUCT = "AquaLook";
constexpr const char* VERSION = AQUALOOK_VERSION;
constexpr const char* BUILD_NUMBER = AQUALOOK_BUILD_NUMBER;
constexpr const char* GIT_SHA = AQUALOOK_GIT_SHA;
constexpr const char* GIT_BRANCH = AQUALOOK_GIT_BRANCH;
constexpr const char* SIGNATURE = "#cNuma";
constexpr uint32_t SPLASH_MIN_READ_MS = 1500;
}
