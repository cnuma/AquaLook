# Checkpoint AquaLook — OTA-2.2 — Persistance et affichage du dernier probe

Date : 2026-07-28

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/remote-ota-github`
- Étape précédente validée : OTA-2.1
- Firmware matériel testé : `ProgrammeArrosage_v4`

## Objectif validé

OTA-2.2 ajoute la persistance du dernier résultat de maintenance GitHub en NVS et son affichage sur la page `/ota` après retour au fonctionnement normal.

Les informations conservées sont :

- succès ou échec ;
- commande exécutée ;
- ligne HTTP reçue ;
- durée TLS ;
- uptime disponible pendant la maintenance ;
- heap minimale observée ;
- détail d'erreur éventuel.

## Fichiers concernés

### `src/MaintenanceResult.h`

Ajout du modèle persistant `MaintenanceResult` et de l'interface `MaintenanceResultStore`.

### `src/MaintenanceResult.cpp`

Ajout du stockage NVS dans le namespace :

```text
aq_maint_res
```

### `src/MaintenanceBoot.cpp`

Le probe GitHub produit désormais un résultat détaillé, l'enregistre en NVS avant le redémarrage normal et conserve l'invariant `otaWrite=no`.

### `src/WebManager.h`

Ajouts dans `registerFaultRoutes()` :

- API `GET /api/maintenance/last-result` ;
- affichage du dernier résultat sur `/ota` ;
- maintien du refus pendant un arrosage actif.

### `docs/ota/GITHUB_MANIFEST_V1.md`

Définition du contrat de manifeste destiné à la future commande `CHECK_VERSION`.

## Validation compilation

Les deux environnements PlatformIO ont été compilés avec succès :

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

Le firmware exécuté sur le module réel est :

```text
ProgrammeArrosage_v4
```

## Validation matérielle

### Probe GitHub

Le cycle complet a été validé depuis `/ota`.

Logs principaux :

```text
Maintenance: entree sure avant setup type=probe_github
Maintenance: demande detectee type=probe_github
Maintenance: mode minimal actif command=probe_github otaWrite=no
Maintenance: GitHub TLS connected=yes durationMs=1596
Maintenance: GitHub status=HTTP/1.1 404 Not Found
Maintenance: resultat persiste success=yes tlsMs=1596 heapMin=144172
Maintenance: resultat command=probe_github success=yes otaWrite=no
```

Résultat :

- TLS GitHub réussi ;
- durée TLS : `1596 ms` ;
- ligne HTTP : `HTTP/1.1 404 Not Found` ;
- heap minimale observée : `144172 octets` ;
- résultat enregistré en NVS ;
- aucune écriture OTA.

### Affichage Web

Après le retour au fonctionnement normal et rechargement de `/ota`, la valeur affichée correspond exactement au log série :

```text
1596 ms
```

La ligne HTTP, l'uptime maintenance et la heap minimale sont également affichés.

### Refus pendant un arrosage

Le test de sécurité a été validé :

- le probe est refusé lorsqu'une zone est active ;
- le message de refus est affiché sur `/ota` ;
- aucun redémarrage n'est déclenché ;
- aucune commande de maintenance n'est exécutée.

### Retour au runtime normal

Après la maintenance, AquaLook V4 revient normalement au fonctionnement complet :

- partition active `app0` ;
- `app1` non écrite ;
- layout dual OTA valide ;
- I2C détecté ;
- SD montée ;
- relais initialisés ;
- planificateur actif ;
- écran et tactile initialisés ;
- serveur Web actif ;
- Wi-Fi et NTP reconnectés ;
- météo relancée.

## Invariants confirmés

1. Aucune mise à jour pendant un arrosage actif.
2. Aucun TLS dans le runtime complet.
3. Aucun usage de `initVariant()`.
4. Maintenance exécutée dans une tâche dédiée.
5. Aucune libération de sprites.
6. Aucune modification du moteur d'arrosage.
7. La commande NVS est effacée avant exécution afin d'éviter une boucle de redémarrage.
8. Aucune écriture dans `app1`.
9. `CHECK_VERSION` reste non exécutable.
10. `INSTALL_UPDATE` reste désactivé.

## Points indépendants observés

Les messages suivants restent connus et ne remettent pas en cause OTA-2.2 :

- absence de partition core dump ;
- callback APB UART/SPI dupliqué ;
- alertes de boucles lentes ;
- premier essai météo parfois échoué puis récupération automatique.

Ces sujets restent séparés du chantier OTA.

## Décision

OTA-2.2 est validé sur matériel réel avec le firmware V4.

## Suite — OTA-2.3

La prochaine étape est l'implémentation de `CHECK_VERSION`.

Elle devra uniquement :

1. être déclenchée depuis `/ota` ;
2. redémarrer en maintenance minimale ;
3. télécharger un manifeste GitHub de taille plafonnée ;
4. valider son schéma et ses champs ;
5. sélectionner la cible correspondant au firmware courant ;
6. comparer la version publiée avec la version installée ;
7. persister et afficher le résultat ;
8. revenir automatiquement au runtime normal.

Toujours interdit pendant OTA-2.3 :

- téléchargement du firmware ;
- `Update.begin()` ;
- `Update.write()` ;
- `esp_ota_begin()` ;
- `esp_ota_write()` ;
- toute écriture dans `app1` ;
- toute modification de la partition de démarrage.
