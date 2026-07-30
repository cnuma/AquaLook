#pragma once

#include "MaintenanceResult.h"

class OtaDownloadTest {
public:
    // Télécharge et vérifie le firmware sans ouvrir ni écrire une partition OTA.
    static MaintenanceResult run(const MaintenanceResult& validatedManifest);
};
