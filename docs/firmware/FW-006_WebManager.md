# AquaLook Firmware — WebManager

- Référence : FW-006
- Statut : relié au code
- Maturité : D4
- Sources : `src/WebManager.h`, `src/WebManager.cpp`

## Mission

`WebManager` expose l’interface HTTP locale, sérialise les états en JSON et transmet les demandes utilisateur aux managers concernés.

## Cycle de vie

- `begin(...)` injecte NTP, météo, relais, Scheduler, configuration et Wi-Fi ;
- `setupRoutes()` enregistre les routes ;
- `_server.begin()` démarre le serveur HTTP sur le port 80 ;
- `update()` exécute hors AsyncTCP les sauvegardes et redémarrages différés.

## Responsabilités

- routes GET d’état, diagnostic, configuration et journaux ;
- routes POST JSON de programmation et de configuration ;
- portail captif et redirections des OS ;
- validation des paramètres et réponses JSON ;
- délégation aux managers propriétaires des données.

## Invariants

- aucune écriture LittleFS longue dans un callback AsyncTCP ;
- répondre avant un redémarrage ;
- une route modifiant l’état utilise POST ;
- les chemins `/api/` ne sont pas servis par la SD ;
- aucun secret ne doit apparaître dans les logs.

## Risques ouverts

- HTTP sans authentification forte ;
- sessions et CSRF non implémentés ;
- correction du log Wi-Fi suivie par l’issue sécurité dédiée.

## Tests

- contrats statiques dans `tests/contracts/` ;
- tests positifs et négatifs des schémas JSON ;
- vérification de l’effet réel après réponse ;
- compilation legacy et V4.

## Références

- `docs/engineering/09_WEB_AND_HTTP_INTERFACES.md`
- `docs/engineering/19_HTTPS_AND_SESSIONS.md`
- `docs/developer/DEV-003_Ajouter_une_route_REST.md`
