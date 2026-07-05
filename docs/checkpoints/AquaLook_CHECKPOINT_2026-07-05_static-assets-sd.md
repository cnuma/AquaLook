# CHECKPOINT AquaLook — 2026-07-05 — Static assets SD et allègement firmware

## 1. Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche active : `refactor/static-assets-sd`
- Commit fonctionnel validé : `aa2ebce93e5ebe5ce801949ee5276f97e206971f`
- Message du commit : `build: alleger le fallback du portail captif`
- Environnement : PlatformIO, `espressif32 @ 6.13.0`, carte `esp32dev`, framework Arduino
- Partition applicative : `1 441 792 octets`

Ce commit est la base de reprise. Ne pas repartir d’un ancien ZIP, d’un ancien checkpoint ou d’une reconstruction depuis l’historique.

## 2. État validé

### 2.1 Compilation

Compilation utilisateur réussie sur l’environnement `ProgrammeArrosage`.

Dernières métriques validées :

```text
RAM:   21.6% — 70 824 / 327 680 octets
Flash: 88.6% — 1 277 861 / 1 441 792 octets
```

État initial avant l’allègement :

```text
Flash: 90.1% — 1 298 857 octets
```

Gain total obtenu :

```text
20 996 octets, soit environ 20,5 Kio
```

### 2.2 Tests matériels validés

Tests réalisés par l’utilisateur sur un second module :

- démarrage et accès au portail captif avec carte SD ;
- portail complet servi depuis la SD ;
- scan Wi-Fi disponible ;
- sélection d’un réseau ;
- saisie du mot de passe ;
- sauvegarde et redémarrage ;
- fonctionnement sans carte SD ;
- fallback embarqué affiché ;
- saisie manuelle du SSID disponible ;
- comportement conforme dans les deux cas.

Conclusion utilisateur : **tests concluants**.

## 3. Modifications réalisées

### 3.1 Réduction des polices TFT embarquées

Fichier : `platformio.ini`

Polices retirées car inutilisées :

```text
LOAD_FONT6
LOAD_FONT7
LOAD_FONT8
SMOOTH_FONT
```

Polices conservées :

```text
LOAD_GLCD
LOAD_FONT2
LOAD_FONT4
LOAD_GFXFF
```

Gain mesuré : environ `10 852 octets` lors du premier passage.

### 3.2 Réutilisation de la police 12 pt pour le splash

Fichier : `src/Theme.h`

Modification :

```cpp
#define THEME_FONT_SPLASH &FreeSansBold12pt7b
```

La police `FreeSansBold18pt7b` n’est plus liée uniquement pour le fallback du splash.

Gain mesuré : `5 652 octets`.

### 3.3 Portail captif hybride SD / firmware

#### Portail complet sur SD

Fichier source synchronisé vers la carte :

```text
data/setup.html
```

Destination sur la SD :

```text
/www/setup.html
```

Fonctions conservées dans la version SD :

- scan des réseaux Wi-Fi ;
- liste des SSID ;
- puissance du signal en dBm ;
- indication des réseaux sécurisés ;
- sélection du SSID ;
- saisie manuelle possible ;
- sauvegarde via `POST /api/wifi` ;
- messages d’état et d’erreur.

Éléments volontairement retirés :

- barre de progression simulée ;
- spinner animé.

#### Routage hybride

Fichier : `src/SdStaticHandler.cpp`

Fonction modifiée :

```cpp
SdStaticHandler::mapRequestPath()
```

Comportement :

```text
GET /setup
  si SD disponible et /www/setup.html présent
    -> fichier SD
  sinon
    -> route embarquée de WebManager
```

Le handler SD décline proprement la requête si la SD ou le fichier manque, afin que le fallback firmware prenne la main.

#### Fallback embarqué

Fichier : `src/WebManager.cpp`

Bloc modifié :

```cpp
static const char CAPTIVE_HTML[] PROGMEM
```

Le fallback conserve :

- présentation lisible sur mobile ;
- champ SSID ;
- champ mot de passe ;
- bouton d’enregistrement ;
- validation du SSID ;
- messages de réussite et d’erreur ;
- appel `POST /api/wifi`.

Le fallback ne contient plus le scan Wi-Fi ni le JavaScript associé.

Gain mesuré : `4 492 octets`.

### 3.4 Synchronisation de la carte SD

Script :

```text
tools/sync-sd-assets.ps1
```

Source réelle :

```text
data/
```

Destination :

```text
<lecteur SD>:\www
```

Commande validée :

```powershell
.\tools\sync-sd-assets.ps1 -SdDrive D: -Clean
```

Le fichier `setup.html` doit être présent ici :

```text
D:\www\setup.html
```

Le dossier provisoire `sd-assets/www/setup.html` a été supprimé afin d’éviter une seconde source de vérité.

## 4. Gestion de la carte SD et défaut matériel

Fichiers concernés :

```text
src/FaultManager.h
src/StorageManager.h
src/StorageManager.cpp
src/SystemDiagnostics.cpp
```

État actuel :

- ajout de `FaultId::STORAGE_SD` ;
- défaut activé si la SD est absente ou inutilisable au démarrage ;
- contrôle périodique de santé environ toutes les 2 secondes ;
- retrait de la carte détecté en fonctionnement ;
- signalement par le mécanisme centralisé `FaultManager` et LED rouge ;
- test de présence basé actuellement sur `/www/index.html`.

Caveat architectural :

- la surveillance est appelée via le pont global `storageHealthUpdate()` depuis `SystemDiagnostics::loopEnter()` ;
- cela fonctionne, mais le raccordement propre à terme serait un appel explicite à `storageMgr.update()` depuis la boucle principale.

## 5. Invariants documentés mais non encore implémentés

Les invariants suivants ont été ajoutés dans `docs/codex/03_INVARIANTS.md` :

- F12 : version Git consultable sur la page Système LCD ;
- F13 : page Web « À propos » avec version, SHA court, origine/branche et date de compilation ;
- F14 : LCD, Web, diagnostics et logs utilisent une source de version unique à la compilation.

Décision correspondante dans `docs/codex/02_DECISIONS.md` :

- D013 : identité firmware centralisée et générée depuis Git/build.

Aucune implémentation fonctionnelle n’est encore présente pour ces invariants.

## 6. Analyse de taille réalisée

Principaux objets AquaLook mesurés avant le dernier allègement :

```text
WebManager.cpp.o       ~54,6 Ko
DisplayManager.cpp.o   ~46,9 Ko
ConfigManager.cpp.o    ~27,7 Ko
WeatherManager.cpp.o   ~15,1 Ko
```

Symboles remarquables :

```text
ConfigManager::loadLegacyJson()       4 371 octets
CAPTIVE_HTML initial                  6 503 octets
FreeSansBold18pt7b tables             ~5 643 octets
WeatherManager::fetch()               4 816 octets
page /logs embarquée                  ~2 525 octets
```

Important : les tailles des fichiers `.o` incluent le code généré pour chaque unité, mais ne doivent pas être confondues avec le gain final du linker. Toute suppression future doit être mesurée après compilation.

## 7. Reste à faire priorisé

### P1 — Page `/logs` hybride

Objectif : appliquer la même stratégie que pour le portail captif.

Comportement cible :

```text
SD présente et /www/logs.html disponible
  -> interface complète depuis la SD
sinon
  -> fallback embarqué fonctionnel
```

La version complète doit conserver :

- journal lisible ;
- état des défauts ;
- bouton d’acquittement ;
- rafraîchissement automatique ;
- lien de retour.

Le fallback embarqué doit conserver au minimum :

- iframe ou affichage de `/api/logs` ;
- état des défauts ;
- acquittement ;
- lien de retour.

Gain estimé : `1,5 à 2 Kio`.

Fichiers prévus :

```text
data/logs.html
src/SdStaticHandler.cpp
src/WebManager.h
```

Attention : plus de deux fichiers seront modifiés. Fournir automatiquement les fichiers complets ou le commit final prêt à récupérer.

### P2 — Mesure après `/logs`

Après modification :

```powershell
pio run -e ProgrammeArrosage
```

Relever :

```text
RAM
Flash
```

Puis refaire au besoin l’extraction des plus gros symboles depuis `firmware.elf`.

### P3 — Arbitrer la suppression de `loadLegacyJson()`

Fichier :

```text
src/ConfigManager.cpp
```

Fonction :

```cpp
ConfigManager::loadLegacyJson()
```

Elle ne sert que si :

- aucun bloc NVS valide n’existe ;
- un ancien `/config.json` est encore présent dans LittleFS.

Suppression possible uniquement après décision explicite :

- conserver la migration depuis les anciennes versions ;
- ou déclarer que tous les modules actifs sont déjà migrés vers NVS.

Gain attendu : `5 à 10 Kio` selon les fonctions et chaînes éliminées par le linker.

### P4 — Implémenter l’identité firmware centralisée

Créer une source unique de compilation, par exemple :

```text
src/BuildInfo.h
```

Informations à exposer :

```text
version applicative
SHA Git court
branche ou origine de build
DATE/TIME de compilation
```

Réutilisation prévue :

- page Système LCD ;
- page Web « À propos » ;
- diagnostics JSON ;
- logs de démarrage ;
- fallback Web minimal.

### P5 — Nettoyage du raccordement StorageManager

Déplacer à terme l’appel de surveillance SD :

```text
SystemDiagnostics::loopEnter()
```

vers la boucle principale, via :

```cpp
storageMgr.update();
```

Ne pas effectuer ce déplacement en même temps qu’une optimisation Flash non liée.

### P6 — Réévaluer `TJpg_Decoder`

Ne pas supprimer à l’aveugle.

Le symbole global `TJpgDec` représente surtout du BSS/RAM. Le coût Flash doit être déterminé par comparaison de builds.

Options futures :

- conserver le splash JPEG ;
- utiliser une image RGB565 ou un format plus simple ;
- utiliser uniquement un splash texte ;
- supprimer `TJpg_Decoder` si le gain mesuré justifie la perte.

### P7 — Revoir les partitions seulement en dernier recours

Le partitionnement actuel laisse environ :

```text
163 931 octets de marge applicative
```

à partir du build validé `1 277 861 / 1 441 792`.

Modifier la table de partitions ne réduit pas le binaire. Cela ne doit être envisagé qu’après optimisation du code et vérification des contraintes OTA/LittleFS.

## 8. Procédure de reprise

```powershell
git switch refactor/static-assets-sd
git pull
pio run -e ProgrammeArrosage
```

Valeur de référence attendue avant nouveau changement :

```text
RAM:   70 824 octets
Flash: 1 277 861 octets
```

Pour synchroniser la SD :

```powershell
.\tools\sync-sd-assets.ps1 -SdDrive D: -Clean
```

Tests minimum après modification Web/SD :

1. SD présente : charger `/setup` et vérifier le scan.
2. SD absente : charger `/setup` et vérifier la saisie manuelle.
3. Sauvegarder un SSID et vérifier le redémarrage.
4. Retirer la SD en fonctionnement et vérifier le défaut rouge.
5. Vérifier que la page principale reste accessible après plusieurs heures.
6. Comparer RAM et Flash avec les valeurs de référence.

## 9. Risques et points de vigilance

- L’ordre d’enregistrement des handlers AsyncWebServer est déterminant pour les fallbacks SD/firmware.
- Ne jamais rendre la configuration Wi-Fi dépendante de la présence de la SD.
- Ne jamais supprimer `/api/wifi/scan` tant que le portail SD l’utilise.
- Ne jamais écrire NVS ou LittleFS depuis un callback AsyncTCP si une opération différée existe déjà.
- Ne pas déplacer les fichiers Web dans un nouveau dossier sans modifier aussi `tools/sync-sd-assets.ps1`.
- `data/` est actuellement la source de vérité des fichiers destinés à `/www` sur la SD.
- Toute optimisation doit être compilée et mesurée séparément pour identifier son gain réel.

## 10. Derniers commits utiles

```text
3e8a893  build: retirer les polices TFT inutilisees
12058e1  build: reutiliser la police 12pt pour le splash
a70fb39  feat: ajouter le portail captif complet sur SD
e6875bc  feat: servir le portail captif complet depuis la SD
e2c3c41  fix: placer le portail SD dans la source synchronisee
9acd81f  chore: supprimer le doublon du portail SD
2f0afef  tools: ajouter la transformation du portail captif embarque
aa2ebce  build: alleger le fallback du portail captif
```

---

Checkpoint établi après compilation, tests avec et sans SD, et validation utilisateur du comportement hybride du portail captif.
