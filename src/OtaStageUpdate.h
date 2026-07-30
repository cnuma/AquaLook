#pragma once

#include "MaintenanceResult.h"

class OtaStageUpdate {
public:
    static MaintenanceResult run(const MaintenanceResult& validatedManifest);
};
