# Checkpoint AquaLook — OTA-2.2 — Persistance et affichage du dernier probe

Date : 2026-07-28

## 1. Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/remote-ota-github`
- Commit fonctionnel et matériel validé avant consolidation documentaire : `aa1989b3905708ac4c13185bd8e2de00c2485e50`
- Commit documentaire officiel de reprise : commit contenant la présente version de ce fichier
- Étape précédente validée : OTA-2.1
- Firmware matériel testé : `ProgrammeArrosage_v4`

La hiérarchie applicable est : code réel du commit ciblé, présent checkpoint, documents d'architecture et d'exploitation cités, puis historique des conversations.

## 2. Objectif validé

OTA-2.2 ajoute la persistance du dernier résultat de maintenance GitHub en NVS et son affichage sur la page `/ota` après retour au fonctionnement normal.

Les informations conservées sont :

- succès ou échec ;
- commande exécutée ;
- ligne HTTP reçue ;
- durée TLS ;
- uptime disponible pendant la maintenance ;
- heap minimale observée ;
- détail d'erreur éventuel.

## 3. Architecture et logique validées

Le runtime complet ne lance aucune connexion TLS GitHub. La page `/ota` demande une maintenance, le runtime vérifie qu'aucune zone n'arrose, enregistre la commande en NVS et programme un redémarrage différé.

Avant le `setup()` nominal, `MaintenanceSetupWrapper` détecte la demande et lance `MaintenanceBoot` dans une tâche FreeRTOS dédiée de 16 384 octets sur le cœur 0.

La demande est effacée avant son exécution. Le mode minimal connecte le Wi-Fi, effectue le probe TLS GitHub, persiste le résultat, ferme les ressources puis redémarre automatiquement vers le runtime normal.

Le résultat est ensuite relu par `GET /api/maintenance/last-result` et affiché sur `/ota`.

## 4. Fichiers de code concernés

### `src/MaintenanceResult.h`

Ajout du modèle persistant `MaintenanceResult` et de l'interface `MaintenanceResultStore`.

### `src/MaintenanceResult.cpp`

Ajout du stockage NVS dans le namespace :

```text
aq_maint_res
```

Clés actuelles : `valid`, `success`, `tls_ms`, `uptime_ms`, `heap_min`, `command`, `http`, `detail`.

### `src/MaintenanceBoot.cpp`

Le probe GitHub produit un résultat détaillé, l'enregistre en NVS avant le redémarrage normal et conserve l'invariant `otaWrite=no`.

### `src/WebManager.h`

Ajouts dans `registerFaultRoutes()` :

- API `GET /api/maintenance/last-result` ;
- affichage du dernier résultat sur `/ota` ;
- maintien du refus pendant un arrosage actif.

### `docs/ota/GITHUB_MANIFEST_V1.md`

Définition du contrat de manifeste destiné à la future commande `CHECK_VERSION`.

## 5. Consolidation documentaire durable

OTA-2.2 ne repose plus uniquement sur le checkpoint ou l'historique du chat. Les connaissances ont été réparties dans les documents de référence suivants.

### Architecture / ingénierie

`docs/engineering/OTA_REMOTE_UPDATE_ARCHITECTURE.md`

Contient :

- finalité et séparation runtime/maintenance/installation ;
- architecture des composants ;
- séquence complète ;
- invariants ;
- partitions ;
- modes dégradés ;
- observabilité ;
- limite d'OTA-2.3.

### Fonctionnement firmware

`docs/firmware/OTA_MAINTENANCE_RUNTIME.md`

Contient :

- déclenchement Web ;
- entrée avant `setup()` ;
- logique exacte du probe ;
- format NVS ;
- restitution Web ;
- validation matérielle ;
- événements connus hors périmètre.

### Guide développeur

`docs/developer/OTA_EXTENSION_GUIDE.md`

Contient :

- ordre de lecture ;
- méthode d'ajout d'une commande ;
- règles d'évolution du résultat NVS ;
- méthode d'implémentation de `CHECK_VERSION` ;
- identité explicite de build ;
- primitives interdites ;
- commandes de compilation, upload et série ;
- obligations documentaires de chaque palier.

### Contrat de données OTA

`docs/ota/GITHUB_MANIFEST_V1.md`

Contient le format versionné du futur manifeste GitHub et les règles de validation.

## 6. Matrice documentaire OTA-2.2

| Sujet | Document de référence | État |
|---|---|---|
| architecture OTA distante | `docs/engineering/OTA_REMOTE_UPDATE_ARCHITECTURE.md` | consolidé |
| fonctionnement du firmware | `docs/firmware/OTA_MAINTENANCE_RUNTIME.md` | consolidé |
| prolongement par un développeur | `docs/developer/OTA_EXTENSION_GUIDE.md` | consolidé |
| format du manifeste | `docs/ota/GITHUB_MANIFEST_V1.md` | défini, implémentation non commencée |
| preuve exacte de validation | présent checkpoint | validé matériel |
| installation du firmware | non documentée comme fonction active | volontairement désactivée |
| signature et rollback | à formaliser avant `INSTALL_UPDATE` | ouvert |

Aucune décision propre à OTA-2.2 ne doit désormais dépendre uniquement du chat.

## 7. Validation compilation

Les deux environnements PlatformIO ont été compilés avec succès :

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

Le firmware exécuté sur le module réel est :

```text
ProgrammeArrosage_v4
```

`data/` n'a pas été modifié par OTA-2.2 ; aucun `buildfs` n'était requis pour ce palier.

## 8. Validation matérielle

### 8.1 Probe GitHub

Le cycle complet a été validé depuis `/ota`.

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

### 8.2 Affichage Web

Après retour au fonctionnement normal et rechargement de `/ota`, la durée affichée correspond au log série :

```text
1596 ms
```

La ligne HTTP, l'uptime maintenance et la heap minimale sont également affichés.

### 8.3 Refus pendant un arrosage

Le test de sécurité a été validé :

- le probe est refusé lorsqu'une zone est active ;
- le message de refus est affiché sur `/ota` ;
- aucun redémarrage n'est déclenché ;
- aucune commande de maintenance n'est exécutée.

### 8.4 Retour au runtime normal

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

## 9. Invariants confirmés

1. Aucune mise à jour pendant un arrosage actif.
2. Aucun TLS dans le runtime complet.
3. Aucun usage de `initVariant()`.
4. Maintenance exécutée dans une tâche dédiée.
5. Aucune libération de sprites.
6. Aucune modification du moteur d'arrosage.
7. La commande NVS est effacée avant exécution.
8. Aucune écriture dans `app1`.
9. `CHECK_VERSION` reste non exécutable.
10. `INSTALL_UPDATE` reste désactivé.
11. Les délais réseau sont bornés.
12. Le programmateur revient au runtime normal après succès ou échec.

## 10. Risques, limites et dette restante

- `client.setInsecure()` est acceptable pour le probe de transport, mais ne constitue pas une sécurité suffisante pour une installation réelle.
- La signature du manifeste ou du firmware n'est pas encore définie.
- Le téléchargement en flux et le calcul SHA-256 ne sont pas implémentés.
- Le rollback matériel n'est pas validé.
- L'identité compile-time distinguant sans ambiguïté Legacy et V4 doit être centralisée pour OTA-2.3.
- Les messages d'absence de partition core dump, callback APB dupliqué, boucles lentes et récupération météo restent hors périmètre OTA.
- La branche OTA devra ultérieurement intégrer ou réconcilier le manuel d'ingénierie global présent sur les branches plus récentes du dépôt.

## 11. Décision

OTA-2.2 est validé sur matériel réel avec le firmware V4 et conforme à la règle de consolidation documentaire : architecture, fonctionnement firmware, méthode développeur et contrat de données sont enregistrés dans le dépôt.

## 12. Suite — OTA-2.3

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

## 13. Procédure exacte de reprise

```powershell
git switch work/remote-ota-github
git pull
git status
git rev-parse HEAD
git rev-parse origin/work/remote-ota-github
```

Les deux SHA doivent être identiques.

Lire ensuite :

1. le présent checkpoint ;
2. `docs/engineering/OTA_REMOTE_UPDATE_ARCHITECTURE.md` ;
3. `docs/firmware/OTA_MAINTENANCE_RUNTIME.md` ;
4. `docs/developer/OTA_EXTENSION_GUIDE.md` ;
5. `docs/ota/GITHUB_MANIFEST_V1.md` ;
6. le code réel de maintenance.

Compilation :

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

Upload V4 :

```powershell
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
```

Connexion série :

```powershell
pio device monitor -p COM3 -b 115200
```

Upload V4 puis connexion série :

```powershell
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3 ; pio device monitor -p COM3 -b 115200
```
