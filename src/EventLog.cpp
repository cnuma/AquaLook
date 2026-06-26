#include "EventLog.h"

// ── Définitions des membres statiques ─────────────────────────
// Une seule définition par programme (ODR) — ici dans EventLog.cpp.
// PlatformIO compile automatiquement tous les .cpp de src/.
LogEntry EventLog::_buf[LOG_CAPACITY];
uint8_t  EventLog::_head     = 0;
uint8_t  EventLog::_count    = 0;
bool     EventLog::_hasErrors = false;
