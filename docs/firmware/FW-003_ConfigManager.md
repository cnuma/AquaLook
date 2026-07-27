# AquaLook Firmware — ConfigManager

- Référence : FW-003
- Statut : relié au code
- Maturité : D4
- Sources : `src/ConfigManager.h`, `src/ConfigManager.cpp`, `platformio.ini`

## Mission

`ConfigManager` est propriétaire du chargement, de la validation et de la persistance de la configuration AquaLook.

## Stockages confirmés

- namespace NVS : `aqualook` ;
- clés : `config` et `intAnchors` ;
- schéma NVS : `CFG_NVS_SCHEMA = 1` ;
- LittleFS monté par `ConfigManager` ;
- SD réservée aux ressources et données volumineuses, pas aux paramètres critiques.

## Responsabilités

- charger les valeurs persistées ;
- appliquer des valeurs par défaut sûres ;
- borner et valider les champs ;
- exposer les sous-configurations système, Wi-Fi, NTP, météo, affichage, zones et manuel ;
- sauvegarder hors des callbacks réseau sensibles ;
- signaler les changements via `EventBus` lorsque nécessaire.

## Invariants

- un schéma persistant ne change jamais silencieusement ;
- toute migration possède une stratégie de retour ou de valeur par défaut ;
- aucune écriture Flash répétitive dans la boucle rapide ;
- les secrets ne sont ni journalisés ni exposés par les diagnostics ;
- la configuration invalide ne provoque pas d’activation matérielle.

## Flux Web

Les handlers Web valident et copient la demande. Les écritures lourdes sont différées vers `WebManager::update()` ou réalisées par les setters dans un contexte autorisé.

## Points d’extension

1. ajouter le champ à la structure concernée ;
2. définir valeur par défaut et bornes ;
3. décider persistance et version de schéma ;
4. mettre à jour sérialisation, désérialisation et migration ;
5. exposer l’API uniquement si nécessaire ;
6. ajouter tests reboot, configuration absente et configuration corrompue ;
7. mettre à jour les références Engineering et Firmware.

## Tests requis

- premier démarrage ;
- sauvegarde puis reboot ;
- NVS absente ou invalide ;
- bornes numériques et chaînes trop longues ;
- compatibilité legacy/V4 ;
- changement de nombre de zones ;
- absence de secret dans logs et JSON.

## Références

- `docs/engineering/07_CONFIGURATION_AND_PERSISTENCE.md`
- `docs/engineering/27_FILE_AND_STORAGE_MAP.md`
- `docs/developer/DEV-005_Bonnes_pratiques_de_developpement.md`
