# CHECKPOINT AquaLook — branche `refactor/static-assets-sd` — v5.8.0

Date de validation : 5 juillet 2026  
Dépôt : `cnuma/AquaLook`  
Branche : `refactor/static-assets-sd`  
Version fonctionnelle : `5.8.0`  
Commit fonctionnel validé : `25069b0b8ad366089c9d5daeca34c4f116cadf0f`

## 1. Objet du checkpoint

Ce checkpoint clôture l’étape de migration des ressources Web d’AquaLook vers la carte SD, de réduction de LittleFS et d’agrandissement des partitions OTA.

Il constitue la source de vérité de reprise pour la suite du projet, sauf instruction explicite contraire.

Le moteur d’arrosage, la gestion des relais, la planification, la configuration NVS et les invariants de sécurité n’ont pas été modifiés dans cette étape.

## 2. État matériel confirmé

Carte cible : CYD / ESP32-2432S028R compatible `esp32dev`.

Identification réelle obtenue avec `esptool.py flash_id` :

- puce : `ESP32-D0WD-V3`, révision 3.1 ;
- CPU : double cœur, 240 MHz ;
- quartz : 40 MHz ;
- Flash physique : 4 Mo ;
- tension Flash : 3,3 V ;
- PSRAM observée : 0 octet sur cette configuration.

Commande de diagnostic utilisée :

```powershell
pio pkg exec --package "tool-esptoolpy" -- esptool.py --chip esp32 --port COM9 flash_id
```

## 3. Nouvelle organisation de la Flash

Table de partitions : `aqualook_partitions.csv`.

Découpage validé :

| Partition | Taille |
|---|---:|
| NVS | 20 Kio |
| OTA data | 8 Kio |
| `app0` | 1 984 Kio |
| `app1` | 1 984 Kio |
| LittleFS | 64 Kio |

Les deux slots OTA disposent chacun de `0x1F0000`, soit 2 031 616 octets.

Ancien état : environ 1 408 Kio par slot OTA et environ 1,19 Mio pour LittleFS.

Gain : environ 576 Kio supplémentaires par slot OTA.

Important : ce changement de table de partitions nécessite une première programmation USB. Une mise à jour OTA classique ne peut pas remplacer la table de partitions en toute sécurité.

## 4. Résultat de compilation validé

Compilation PlatformIO réussie avec l’environnement `ProgrammeArrosage`.

```text
RAM:   20.5% — 67 216 / 327 680 octets
Flash: 62.4% — 1 268 613 / 2 031 616 octets
```

Marge firmware disponible dans chaque slot OTA : environ 763 003 octets.

Cette marge rend réalistes les évolutions prévues : OTA, portail autonome, débitmètres, MQTT et sécurité TLS, sous réserve de mesurer l’impact de chaque ajout.

## 5. Architecture des ressources Web

### 5.1 Carte SD

Le dossier Git `data/` reste la source complète des ressources Web destinées à la carte SD.

Arborescence attendue sur la carte :

```text
/www/index.html
/www/...
```

Le script de synchronisation reste :

```powershell
.\tools\sync-sd-assets.ps1 -SdDrive D: -Clean
```

Le script :

- copie récursivement `data/` vers `/www` ;
- vérifie chaque fichier ;
- compare les SHA-256 source/destination ;
- échoue si un fichier est absent ou différent.

### 5.2 LittleFS minimal

`platformio.ini` définit désormais :

```ini
[platformio]
data_dir = littlefs
```

Le dossier `littlefs/` est séparé de `data/`.

LittleFS ne contient plus les ressources Web complètes. Il ne conserve que le minimum technique indispensable, actuellement le marqueur `littlefs/splash.jpg` utilisé par le splash texte de transition.

La configuration persistante est stockée dans NVS, pas dans LittleFS.

### 5.3 Fallback firmware

Les secours minimaux restent embarqués dans le firmware :

- page de configuration minimale `/setup` ;
- page de logs minimale `/logs` ;
- logo SVG minimal généré par le firmware ;
- endpoint `/api/storage` ;
- APIs et commandes nécessaires au fonctionnement local.

En cas d’absence ou de panne de la carte SD, le moteur d’arrosage reste autonome.

## 6. Gestion de la carte SD

Gestionnaire : `StorageManager`.

Le stockage utilise `SdFat` en SPI logiciel.

États principaux :

- `READY` ;
- `SD_UNAVAILABLE` ;
- `WEB_ASSETS_MISSING` ;
- `READ_ERROR`.

La sentinelle de contrôle est `/www/index.html`.

Le contrôle de santé est exécuté toutes les 2 secondes par `StorageManager::update()` désormais appelé dans `loop()`.

Comportement validé :

1. carte présente au démarrage : ressources Web servies depuis SD ;
2. retrait à chaud : défaut détecté en environ 2 secondes ;
3. interface complète SD indisponible ;
4. fallback minimal conservé ;
5. moteur d’arrosage non interrompu ;
6. réinsertion à chaud non remontée automatiquement ;
7. un redémarrage est nécessaire après réinsertion.

Cette absence de remontage automatique est volontairement conservée pour la version 5.8.0.

## 7. Signalisation des erreurs

Gestionnaire central : `FaultManager`.

Défauts actuellement prévus :

- relais I²C ;
- Wi-Fi ;
- système de fichiers ;
- erreur logicielle ;
- stockage SD.

### 7.1 LED physique

Une erreur non acquittée déclenche le motif rouge défini dans `FaultManager::resolveColor()`.

Un défaut actif acquitté conserve un rappel rouge périodique.

### 7.2 LCD

Une icône est affichée à proximité du texte `AquaLook` dans le bandeau supérieur :

- triangle rouge clignotant : erreur non acquittée ;
- triangle rouge entouré de vert : erreur acquittée mais défaut encore actif ;
- aucune icône : aucun défaut actif et aucune erreur non acquittée.

L’indicateur repose sur l’état global de `FaultManager`, pas uniquement sur la SD.

Nuance :

- `FaultManager::setActive()` représente un défaut persistant jusqu’au retour à la normale ;
- `EventLog::log(LOG_ERROR, ...)` signale une erreur non acquittée, mais ne crée pas automatiquement un défaut actif durable.

## 8. Versionnement

Version publiée : `5.8.0`.

Sources de version :

- `src/BuildInfo.h` ;
- `tools/version_build.py`.

Informations visibles au splash et dans la page système :

- produit ;
- version ;
- numéro de build Git ;
- SHA court ;
- branche ;
- signature `#cNuma`.

Commit fonctionnel validé :

```text
25069b0b8ad366089c9d5daeca34c4f116cadf0f
```

## 9. Fichiers principaux modifiés pendant l’étape

- `aqualook_partitions.csv` : nouvelle répartition Flash ;
- `platformio.ini` : séparation `data/` / `littlefs/` ;
- `littlefs/splash.jpg` : marqueur minimal du splash texte ;
- `data/splash.jpg` : retiré des ressources SD ;
- `src/StorageManager.cpp` : détection de retrait SD ;
- `src/main.cpp` : appel périodique de `storageMgr.update()` ;
- `src/FaultManager.cpp` : indicateur LCD global ;
- `src/BuildInfo.h` : version 5.8.0 ;
- `tools/version_build.py` : version automatique 5.8.0.

## 10. Invariants à préserver

1. Une panne de carte SD ne doit jamais arrêter le moteur d’arrosage.
2. Le fonctionnement des relais et de la planification ne dépend pas du Web.
3. La configuration active reste stockée dans NVS.
4. Les ressources Web complètes restent sur SD.
5. LittleFS reste minimal et ne doit pas redevenir un stockage Web principal.
6. Un fallback local minimal doit rester accessible sans SD.
7. Les deux partitions OTA doivent rester de taille identique.
8. Toute modification de la table de partitions impose une validation USB complète.
9. Aucun fichier du moteur d’arrosage ne doit être modifié pendant les travaux purement stockage/Web sans nécessité démontrée.
10. Après chaque migration de ressources, vérifier les tailles, les doublons HTML/CSS/JS, la construction LittleFS et la copie SD.

## 11. Procédure de reconstruction complète

Depuis PowerShell, dans le dépôt :

```powershell
git switch refactor/static-assets-sd
git pull
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t buildfs
```

Première installation après modification des partitions :

```powershell
pio run -e ProgrammeArrosage -t upload
pio run -e ProgrammeArrosage -t uploadfs
```

Synchronisation SD :

```powershell
.\tools\sync-sd-assets.ps1 -SdDrive D: -Clean
```

## 12. Tests matériels validés

- compilation réussie ;
- occupation Flash 62,4 % ;
- occupation RAM 20,5 % ;
- démarrage normal ;
- splash versionné ;
- SD reconnue ;
- pages servies depuis `/www` ;
- retrait de carte détecté à chaud ;
- triangle rouge clignotant visible ;
- état acquitté visible en rouge entouré de vert ;
- fonctionnement nominal confirmé par l’utilisateur.

## 13. Limites connues

- la réinsertion de la SD n’est pas détectée sans redémarrage ;
- le code de compatibilité JPEG/splash reste transitoire ;
- la migration historique LittleFS vers NVS est encore présente ;
- le fallback minimal doit être testé régulièrement pour éviter une dépendance involontaire à des CSS/JS présents uniquement sur SD ;
- l’OTA applicative reste à implémenter et à tester réellement.

## 14. Suite recommandée

Ordre conseillé :

1. nettoyer définitivement le chemin JPEG historique et le marqueur `splash.jpg` ;
2. inventorier précisément toutes les ressources de `data/` et leurs dépendances ;
3. vérifier la page complète et les fallbacks sans SD ;
4. documenter la procédure de préparation d’une carte SD de production ;
5. implémenter l’OTA du firmware avec contrôle de version, validation et rollback ;
6. tester une mise à jour OTA réelle entre les deux slots ;
7. poursuivre ensuite les fonctions roadmap : portail autonome, débitmètres, communication externe.

## 15. Commande de reprise

Pour reprendre exactement sur cette base :

```powershell
git switch refactor/static-assets-sd
git pull
git checkout 25069b0b8ad366089c9d5daeca34c4f116cadf0f
```

Pour poursuivre normalement sur la branche après le commit documentaire de checkpoint, rester sur la tête de `refactor/static-assets-sd`.
