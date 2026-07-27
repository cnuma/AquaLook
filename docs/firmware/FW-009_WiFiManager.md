# AquaLook Firmware — WiFiManager

- Référence : FW-009
- Statut : relié au code
- Maturité : D4
- Sources : `src/WiFiManager.h`, `src/WiFiManager.cpp`

## Mission

Gérer la connexion Wi-Fi STA, les reconnexions, le portail captif AP et le scan réseau sans bloquer le Runtime.

## Machine d’états

`IDLE`, `CONNECTING`, `CONNECTED`, `DISCONNECTED`, `CAPTIVE_STARTING`, `CAPTIVE_PORTAL`.

Les transitions temporisées passent par `PendingAction` afin de laisser le matériel se stabiliser sans `delay()` long.

## Paramètres confirmés

- timeout de connexion : 15 s ;
- intervalle de reconnexion : 30 s ;
- maximum : cinq tentatives automatiques ;
- DNS captif : port 53 ;
- scan réseau asynchrone.

## Invariants

- aucun blocage infini sur Wi-Fi ;
- le fonctionnement local essentiel reste disponible hors ligne ;
- les changements d’état sont journalisés sans secret ;
- le portail captif est explicite et observable.

## Risque ouvert

Le point d’accès de récupération est encore ouvert dans le firmware courant. Sa sécurisation est suivie par les contrats et le registre de risques.

## Références

- `docs/engineering/18_NETWORK_AND_WIFI.md`
- `docs/engineering/37_SECURITY_CONTRACTS_AND_CI.md`
- `docs/developer/DEV-009_Ajouter_un_service_reseau.md`
