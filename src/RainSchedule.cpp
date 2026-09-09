#include "RainSchedule.h"

namespace RainSchedule {

bool rainBlocksDay(
    const ZoneSchedule& zs,
    const DaySchedule& ds,
    uint8_t col,
    const WeatherManager* weather
) {
    bool hasAny = false;
    for (uint8_t s = 0; s < MAX_SLOTS; s++) {
        if (ds.slots[s].enabled) { hasAny = true; break; }
    }
    if (!hasAny) return false;

    const ForecastDay fd = weather ? weather->getForecastDay(col) : ForecastDay{};
    return fd.valid && fd.rainMm >= zs.rain.thresholdMm;
}

bool intervalDayIsPlanned(
    const ZoneSchedule& zs,
    uint32_t todayEpochDay,
    uint8_t daysAhead
) {
    const uint32_t targetDay = todayEpochDay + daysAhead;
    const uint32_t interval  = zs.intervalDays > 0 ? zs.intervalDays : 1;
    const uint32_t anchor    = zs.intervalAnchorDay;

    return anchor > 0 &&
           targetDay >= anchor &&
           ((targetDay - anchor) % interval) == 0;
}

} // namespace RainSchedule
