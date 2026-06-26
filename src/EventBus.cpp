#include "EventBus.h"

// ── Définition des membres statiques ──────────────────────────
bool EventBus::displayDirty     = false;
bool EventBus::configDirty      = false;
bool EventBus::wifiDirty        = false;
bool EventBus::captiveRequested = false;
