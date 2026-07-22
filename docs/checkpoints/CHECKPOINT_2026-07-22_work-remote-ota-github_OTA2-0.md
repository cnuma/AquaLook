# Checkpoint AquaLook — OTA-2.0 — Socle des commandes de maintenance

Date : 2026-07-22

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/remote-ota-github`
- Checkpoint précédent : `docs/checkpoints/CHECKPOINT_2026-07-22_work-remote-ota-github_OTA1-1_RESULT.md`
- Décision précédente : TLS GitHub fonctionne en firmware isolé mais échoue dans le runtime complet avec `SSL - Memory allocation failed (-32512)`.

## Objectif

Introduire un mode de maintenance extensible, commandé par une valeur typée persistée en NVS, et capable de prendre la main avant `setup()` sans charger TFT, tactile, serveur Web, météo, planificateur, équipements ou notifications.

Aucune écriture dans `app0`, `app1` ou `otadata` n'est ajoutée pendant OTA-2.0.

## Modèle de commande

```cpp
enum class MaintenanceRequest : uint8_t {
    NONE = 0U,
    PROBE_GITHUB = 1U,
    CHECK_VERSION = 2U,
    INSTALL_UPDATE = 3U,
    RECOVERY = 4U,
    FACTORY_RESET = 5U
};
```

La commande est stockée dans :

- namespace NVS : `aq_maint` ;
- clé : `request`.

Une valeur absente, inconnue ou invalide équivaut à `NONE`.

## Fichiers ajoutés

### `src/MaintenanceRequest.h`

Déclare l'énumération et l'interface du stockage NVS.

### `src/MaintenanceRequest.cpp`

Implémente :

- lecture de la demande ;
- validation de la valeur brute ;
- sauvegarde ;
- effacement ;
- nom textuel stable des commandes.

### `src/MaintenanceBoot.h`

Déclare le dispatcher du mode maintenance minimal.

### `src/MaintenanceBoot.cpp`

Implémente uniquement `PROBE_GITHUB` :

1. connexion Wi-Fi bloquante avec timeout 30 secondes ;
2. connexion TLS vers `api.github.com:443` ;
3. requête HTTP `HEAD` ;
4. journalisation heap libre, minimum et plus grand bloc contigu ;
5. aucune écriture OTA ;
6. redémarrage vers le mode normal après succès ou échec.

La validation des certificats est désactivée uniquement pour cette qualification transport. Cela ne constitue pas la sécurité finale de l'OTA.

### `src/MaintenanceEarlyEntry.cpp`

Définit le hook Arduino-ESP32 `initVariant()`, exécuté avant `setup()`.

Comportement :

- `NONE` : retour immédiat, démarrage nominal inchangé ;
- `PROBE_GITHUB` : chargement minimal de la configuration, exécution du probe, puis redémarrage ;
- toute autre commande : effacement, refus explicite, puis démarrage normal.

## Invariants de sécurité

1. La demande est effacée avant son exécution pour empêcher une boucle de redémarrage persistante.
2. Une commande non implémentée ne bloque jamais le programmateur.
3. En cas d'échec NVS, le démarrage normal reste prioritaire.
4. Aucun sprite n'est libéré.
5. Aucun composant du moteur d'arrosage n'est modifié.
6. Aucune partition OTA n'est écrite.
7. `INSTALL_UPDATE`, `RECOVERY` et `FACTORY_RESET` sont uniquement réservés dans le modèle ; ils ne sont pas exécutables.

## État actuel de l'interface

OTA-2.0 fournit le socle interne uniquement. Aucun bouton Web n'est encore ajouté.

Le run suivant OTA-2.1 ajoutera une API locale contrôlée permettant de :

- lire l'état maintenance ;
- demander `PROBE_GITHUB` ;
- redémarrer après vérification qu'aucun arrosage n'est actif ;
- afficher le résultat du dernier probe après retour au mode normal.

`CHECK_VERSION` et `INSTALL_UPDATE` resteront désactivés jusqu'à définition et validation du manifeste GitHub.

## Validation requise

Compiler les profils :

```powershell
git switch work/remote-ota-github
git pull
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

Critères :

- compilation Legacy réussie ;
- compilation V4 réussie ;
- taille firmware toujours inférieure à 85 % de `0x1F0000` ;
- démarrage nominal inchangé lorsqu'aucune demande NVS n'existe ;
- écran, tactile, Web et planificateur inchangés.

## Limite connue

Le hook `initVariant()` doit être confirmé par compilation et par un boot nominal réel sur la version Arduino-ESP32 utilisée par le projet (`espressif32 6.13.0`). Aucun upload ne doit être fait avant compilation réussie des deux profils.
