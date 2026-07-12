#pragma once

#include <Arduino.h>

namespace AquaLook {
namespace Time {

// Comparaison de deadline compatible avec le debordement de millis()/micros().
// Valide tant que l'echeance est distante de moins de 2^31 ticks.
inline bool deadlineReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

inline bool elapsedAtLeast(uint32_t now, uint32_t since, uint32_t duration) {
    return static_cast<uint32_t>(now - since) >= duration;
}

inline uint32_t remainingUntil(uint32_t now, uint32_t deadline) {
    return deadlineReached(now, deadline) ? 0U : static_cast<uint32_t>(deadline - now);
}

// Arrondi superieur sans addition susceptible de deborder.
// Exemples : 1 ms -> 1 s ; 1000 ms -> 1 s ; 1001 ms -> 2 s.
inline uint32_t millisecondsToSecondsCeil(uint32_t milliseconds) {
    return (milliseconds / 1000U) + ((milliseconds % 1000U) != 0U ? 1U : 0U);
}

} // namespace Time
} // namespace AquaLook
