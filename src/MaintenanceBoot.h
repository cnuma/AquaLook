#pragma once

class ConfigManager;

class MaintenanceBoot {
public:
    // Retourne true uniquement si le démarrage nominal ne doit pas continuer.
    // Les commandes non encore prises en charge sont effacées puis refusées
    // sans bloquer le programmateur.
    static bool runIfRequested(ConfigManager& configManager);
};
