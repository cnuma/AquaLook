# Checkpoint AquaLook — consolidation `main` du 5 juillet 2026

## 1. Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche stable : `main`
- Commit fonctionnel validé : `6cbddb9b38855b1421b5d4bea69afdba4c960800`
- Commit de consolidation intermédiaire : `f7eeaca` sur `integration/consolidation-2026-07`
- Branche fonctionnelle intégrée : `refactor/eventlog-centralise`
- Dernier commit fonctionnel de cette branche : `fc066a9`
- Date de validation : 5 juillet 2026
- Origine du checkpoint : `main`

Le commit `6cbddb9` est le merge validé de `integration/consolidation-2026-07` dans `main`. Il rassemble la roadmap déjà présente sur `main`, le moteur d’intervalle et ses corrections d’affichage, le diagnostic système et Web, la centralisation des défauts, les alarmes Web et la priorité de la LED RGB.

Le commit documentaire créé par l’ajout du présent fichier devient le commit officiel de reprise. Il ne modifie ni le firmware ni les ressources LittleFS.

## 2. État Git validé avant le checkpoint

État observé après fusion et push :

```text
On branch main
Your branch is up to date with 'origin/main'.
nothing to commit, working tree clean
```

Historique de tête :

```text
6cbddb9 (HEAD -> main, origin/main, origin/HEAD) Merge branch 'integration/consolidation-2026-07'
f7eeaca (integration/consolidation-2026-07) merge: consolide eventlog, diagnostics et alarmes Web
fc066a9 (refactor/eventlog-centralise) fix: finalise la priorite RGB et les alarmes Web
8b02347 fix: corrige la page blanche et integre le panneau d alarme
6e93fa2 fix: corrige le pilotage de la LED RGB d'alarme
11bdaaa test: desactive la veille WiFi pour analyser la latence web
7b11f9d feat: ajout du diagnostic systeme et web
```

## 3. Fonctionnalités consolidées

### 3.1 Programmation et mode intervalle

1. Mode intervalle basé sur une ancre fixe persistante.
2. Date de départ visible et modifiable explicitement.
3. Proposition explicite d’un nouvel ancrage lors d’un clic sur un jour.
4. Aucune modification silencieuse du cycle lors d’une lecture, d’un rendu ou d’un arrosage effectif.
5. Suppression complète d’une programmation intervalle.
6. Une occurrence bloquée par la pluie est sautée sans décaler la séquence.
7. Cohérence du calcul entre moteur, Web et LCD.
8. Décoration et hachurage du planning corrigés sans repeindre les zones appartenant aux sprites des boutons.

Règle de calcul à préserver :

```text
jour >= ancre ET (jour - ancre) modulo intervalle == 0
```

### 3.2 Diagnostic système et Web

1. Instrumentation légère de la boucle principale.
2. Mesures de mémoire : heap libre, minimum observé et plus grand bloc contigu.
3. Mesures de boucle : durée courante, moyenne, maximum et âge du dernier passage.
4. Détection d’un retard de boucle supérieur au seuil prévu.
5. Compteurs et temps de génération des réponses Web JSON.
6. Informations WiFi : état, adresse IP, RSSI, canal et MAC.
7. API : `GET /api/diagnostics`.
8. Page LittleFS : `/diagnostic.html`.
9. Actualisation de la page sans écriture flash.
10. Veille WiFi désactivée dans l’état actuel pour l’analyse de la latence du premier accès Web.

### 3.3 Défauts, journal et alarmes Web

1. Centralisation des défauts actifs via `FaultManager`.
2. Journalisation centralisée par `EventLog`.
3. Route `GET /api/faults`.
4. Route `POST /api/logs/ack`.
5. Page `/logs` avec acquittement des erreurs.
6. L’acquittement ne vide pas le journal.
7. Panneau d’alarme intégré à la page principale.
8. Alarme non acquittée : triangle et signalisation rouge clignotante.
9. Défaut actif acquitté : rappel visible et fond hachuré rouge/vert.
10. Masquage du panneau lorsqu’il ne reste plus de défaut actif ni d’alarme non acquittée.

### 3.4 LED RGB

1. Priorité absolue de l’alarme rouge sur les fonctions normales de la LED.
2. Erreur non acquittée : clignotement rouge périodique.
3. Erreur active acquittée : retour au fonctionnement normal avec rappel rouge périodique.
4. Erreur résolue et acquittée : fonctionnement normal.
5. Polarité et auto-test de démarrage corrigés dans l’état consolidé.

Les broches documentées pour la CYD ESP32-2432S028R sont :

- rouge : GPIO 4 ;
- vert : GPIO 16 ;
- bleu : GPIO 17.

Toute modification ultérieure de polarité ou de priorité RGB doit être traitée comme critique et vérifiée sur le matériel réel.

## 4. Fichiers intégrés par la consolidation

### 4.1 Documentation et configuration

- `AGENTS.md`
- `AquaLook_diagnostic_systeme.patch`
- `README.txt`
- `README_ALARMES_WEB.md`
- `README_DIAGNOSTIC.md`
- `README_MODIFICATIONS.md`
- `README_PRIORITE_RGB.md`
- `ROADMAP.md` conservé depuis `main`
- `aqualook_partitions.csv` — remplace `min_spiffs.csv`
- `platformio.ini`
- `include/config.h`
- `docs/checkpoints/CHECKPOINT_2026-07-02_40e1751.md`
- `docs/checkpoints/CHECKPOINT_2026-07-02_b11aa7c.md`

### 4.2 Ressources LittleFS

- `data/index.html`
- `data/app.js`
- `data/style.css`
- `data/alarm.css`
- `data/alarm.js`
- `data/diagnostic.html`

### 4.3 Firmware

- `src/main.cpp`
- `src/ConfigManager.cpp`
- `src/ConfigManager.h`
- `src/DisplayManager.cpp`
- `src/DisplayPlanningDecor.cpp`
- `src/EventLog.h`
- `src/FaultManager.cpp`
- `src/FaultManager.h`
- `src/RelaisManager.cpp`
- `src/RelaisManager.h`
- `src/ScheduleManager.cpp`
- `src/ScheduleManager.h`
- `src/ScreenManager.cpp`
- `src/ScreenManager.h`
- `src/SystemDiagnostics.cpp`
- `src/SystemDiagnostics.h`
- `src/WebManager.cpp`
- `src/WebManager.h`
- `src/WiFiManager.cpp`

## 5. Points d’intégration fonctionnels à préserver

### `src/main.cpp`

- initialisation du diagnostic dans `setup()` ;
- entrée et sortie de mesure de boucle dans `loop()` ;
- câblage du callback matériel entre `ScheduleManager` et la gestion des relais ;
- initialisation des gestionnaires dans un ordre compatible avec le matériel et le Web.

### `src/WebManager.cpp` / `src/WebManager.h`

- enregistrement de `/api/diagnostics` ;
- enregistrement de `/api/faults`, `/api/logs/ack` et `/logs` ;
- mesure des réponses Web ;
- réponse au client avant toute opération de redémarrage ;
- conservation des routes et des contrats JSON existants.

### `src/FaultManager.cpp` / `src/FaultManager.h`

- source centrale des défauts actifs ;
- distinction entre défaut actif, défaut acquitté et erreur non acquittée ;
- aucune suppression du journal lors de l’acquittement.

### `src/SystemDiagnostics.cpp` / `src/SystemDiagnostics.h`

- instrumentation sans écriture flash ;
- absence de métriques CPU inventées lorsque les statistiques FreeRTOS ne sont pas disponibles ;
- maintien d’un coût faible dans la boucle principale.

### `src/ConfigManager.cpp` / `src/ConfigManager.h`

- propriétaire unique du montage LittleFS ;
- propriétaire de la persistance de configuration ;
- ancre d’intervalle persistante comme source du cycle.

### `src/ScheduleManager.cpp` / `src/ScheduleManager.h`

- ne pilote jamais directement le matériel ;
- respecte l’ancre fixe du mode intervalle ;
- la pluie saute une occurrence sans déplacer la séquence ;
- le fonctionnement automatique exige une heure synchronisée.

### `src/DisplayManager.cpp` et `src/DisplayPlanningDecor.cpp`

- ne pas repeindre une zone déjà possédée par un sprite de bouton ;
- conserver les corrections du rendu 2 zones ;
- préserver la cohérence des jours actifs et hors cycle.

### `src/RelaisManager.cpp` / `src/RelaisManager.h`

- conserver la sécurité de durée maximale ;
- conserver l’abstraction entre moteur et matériel ;
- traiter toute inversion logique ou modification du contrôleur I2C comme critique.

### `src/WiFiManager.cpp`

- état actuel volontaire : veille WiFi désactivée pour analyser les lenteurs du premier accès ;
- ce réglage est un choix de diagnostic, pas encore une conclusion définitive sur la cause de la latence.

## 6. Invariants obligatoires

1. `main` est la branche stable.
2. `ConfigManager` reste le propriétaire unique du montage LittleFS et de la persistance.
3. La configuration persistante active reste en NVS.
4. LittleFS reste réservé aux ressources Web et au splash, hors migration historique contrôlée.
5. `ScheduleManager` ne pilote jamais directement les relais.
6. Toute activation matérielle passe par le callback prévu dans `main.cpp`.
7. La sécurité de durée maximale ne doit jamais être supprimée.
8. `intervalAnchorDay` reste la source fonctionnelle du cycle intervalle.
9. Le dernier arrosage effectif ne sert jamais d’origine au calcul du cycle.
10. La pluie ne modifie jamais l’ancre.
11. Une route de lecture ou un rendu ne doit produire aucune écriture de configuration.
12. Les routes Web, structures JSON et IDs HTML utilisés par `app.js` sont des contrats.
13. Toute modification de `data/` exige un `buildfs` réussi.
14. Les défauts actifs et leur acquittement restent distincts du contenu historique du journal.
15. L’alarme rouge conserve la priorité sur les animations RGB normales.
16. Limite fonctionnelle actuelle : 1 à 8 zones.
17. Ne pas introduire de traitement bloquant dans la boucle principale ni dans une route Web.
18. Ne pas déplacer la partie Web vers un second cœur sans profilage démontrant que cette architecture est nécessaire et sûre.

## 7. Environnement de compilation

Environnement principal défini dans `platformio.ini` :

```text
[env:ProgrammeArrosage]
platform = espressif32 @ 6.13.0
board = esp32dev
framework = arduino
filesystem = littlefs
partitions = aqualook_partitions.csv
monitor_speed = 115200
```

Dépendances principales :

- ArduinoJson `^7.0.0` ;
- ESPAsyncWebServer `^3.3.0` ;
- AsyncTCP `^3.3.0` ;
- TFT_eSPI `^2.5.43` ;
- TJpg_Decoder `^1.0.8` ;
- XPT2046_Touchscreen depuis le dépôt PaulStoffregen.

Environnements auxiliaires présents :

- `calibration` ;
- `test_relais` ;
- `debug_boot`.

## 8. Validations effectuées

### Git

- fusion de `refactor/eventlog-centralise` dans `integration/consolidation-2026-07` : terminée ;
- branche d’intégration poussée sur GitHub ;
- fusion de `integration/consolidation-2026-07` dans `main` : terminée ;
- push de `main` : réussi ;
- `main` alignée avec `origin/main` ;
- arbre de travail propre avant création du présent checkpoint.

### Compilation

- `pio run` / environnement principal : **SUCCESS**, confirmé par l’utilisateur après consolidation ;
- `pio run -t buildfs` : **SUCCESS**, confirmé par l’utilisateur après consolidation ;
- la compilation et le `buildfs` ont été réalisés avant la fusion finale dans `main` ; la fusion finale ne contient qu’un commit de merge des mêmes contenus validés.

### Web

- ressources Web intégrées et image LittleFS constructible ;
- routes de diagnostic et d’alarme présentes dans la consolidation ;
- aucune nouvelle validation matérielle longue durée de la latence Web n’a été rapportée après le merge final `6cbddb9`.

### LCD

- les corrections antérieures du mode intervalle et du rendu 2 zones sont conservées ;
- aucune nouvelle campagne matérielle LCD n’a été effectuée spécifiquement après la consolidation du 5 juillet.

### Matériel

- aucune nouvelle validation complète des relais, de la LED RGB, du WiFi et des alarmes n’a été effectuée spécifiquement sur `main` après le merge final ;
- les comportements matériels issus des branches intégrées doivent donc rester distingués de la validation compilation/buildfs.

## 9. Tests matériels recommandés après flash

1. Flasher le firmware et LittleFS issus du même commit.
2. Vérifier le démarrage sans activation intempestive d’un relais.
3. Vérifier une zone unique avec une durée courte et sous surveillance.
4. Tester le mode intervalle avant, le jour de et après l’ancre.
5. Vérifier qu’une occurrence pluie ne décale pas le cycle.
6. Ouvrir `/diagnostic.html` depuis un navigateur normal puis privé après plusieurs heures de fonctionnement.
7. Relever `loop.ageMs`, les maxima de boucle, le heap minimal et le plus grand bloc contigu.
8. Vérifier `/api/faults`, `/logs` et l’acquittement.
9. Provoquer un défaut contrôlé, vérifier le clignotement rouge, puis l’état acquitté et le rappel rouge.
10. Vérifier que le panneau d’alarme disparaît lorsque le défaut est résolu et acquitté.
11. Vérifier le rendu LCD dans les configurations 1, 2 et 8 zones utiles.
12. Laisser fonctionner plusieurs heures et vérifier le premier accès Web après une longue période sans requête.

## 10. Risques, limites et dettes techniques

1. La cause exacte de la lenteur du premier accès Web après plusieurs heures n’est pas encore démontrée.
2. La désactivation de la veille WiFi est une mesure d’analyse à évaluer sur la consommation et la stabilité.
3. La page de diagnostic mesure des indicateurs utiles, mais ne fournit pas une charge CPU complète par cœur lorsque les statistiques FreeRTOS ne sont pas activées.
4. LittleFS reste contraint ; surveiller la taille après toute évolution de `data/`.
5. Les fichiers Web doivent conserver des fins de ligne et un encodage maîtrisés pour éviter une croissance artificielle.
6. `AquaLook_diagnostic_systeme.patch` est une trace documentaire ; la source réelle de vérité reste le code intégré dans `main`.
7. Plusieurs README de travail sont encore présents à la racine ; une future consolidation documentaire pourra les déplacer dans `docs/` sans toucher au firmware.
8. `feature/resilience-coupures` n’a pas été fusionnée globalement. Les fonctions utiles de reprise après coupure et de reconnexion WiFi devront être réévaluées et réimplémentées sélectivement depuis `main`.
9. Ne pas fusionner en bloc les anciennes branches divergentes dans `main`.
10. La création d’une tâche Web dédiée sur le second cœur n’est pas retenue à ce stade ; commencer par exploiter les mesures de diagnostic et identifier le blocage réel.

## 11. Branches à conserver temporairement

- `main` — source de vérité stable ;
- `integration/consolidation-2026-07` — historique du sas d’intégration ;
- `refactor/eventlog-centralise` — historique fonctionnel du diagnostic et des alarmes ;
- `fix/encodage-web` — historique du moteur intervalle et des premières mesures Web ;
- `feature/resilience-coupures` — branche divergente à consulter uniquement pour réimplémentation sélective.

## 12. Procédure exacte de reprise

```powershell
cd C:\Users\Emman\OneDrive\Documents\VsCode_travail\arrosage
git switch main
git pull --ff-only origin main
git status
git rev-parse HEAD
```

Résultat attendu :

```text
On branch main
Your branch is up to date with 'origin/main'.
nothing to commit, working tree clean
```

Lire ensuite dans cet ordre :

1. `AGENTS.md`
2. `docs/codex/00_CONTEXT.md`
3. `docs/codex/01_ARCHITECTURE.md`
4. `docs/codex/03_INVARIANTS.md`
5. `docs/codex/04_DEVELOPMENT_RULES.md`
6. `docs/codex/05_BUILD_AND_TEST.md`
7. `docs/checkpoints/CHECKPOINT_2026-07-05_6cbddb9.md`
8. `ROADMAP.md`

Contrôles avant toute modification :

```powershell
git diff --check
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t buildfs
```

Envoi cohérent firmware + LittleFS :

```powershell
pio run -e ProgrammeArrosage -t upload
pio run -e ProgrammeArrosage -t uploadfs
```

## 13. Commandes Git utiles

Créer une branche de travail depuis la source stable :

```powershell
git switch main
git pull --ff-only origin main
git switch -c fix/nom-court-du-correctif
```

Comparer une branche à `main` :

```powershell
git diff --stat main...HEAD
git log --oneline --decorate --graph main..HEAD
```

Abandonner des modifications locales non validées :

```powershell
git restore .
```

Ne pas utiliser `git clean -fd`, `git reset --hard` ou une fusion globale d’une branche divergente sans avoir vérifié précisément les fichiers concernés.

## 14. Bloc minimal de reprise

```text
AquaLook — reprise depuis le checkpoint du 5 juillet 2026

Source de vérité :
- dépôt : cnuma/AquaLook
- branche : main
- commit fonctionnel : 6cbddb9b38855b1421b5d4bea69afdba4c960800
- commit officiel de reprise : commit documentaire contenant docs/checkpoints/CHECKPOINT_2026-07-05_6cbddb9.md

État validé :
- fusion de la consolidation dans main terminée et poussée ;
- compilation ProgrammeArrosage : SUCCESS ;
- buildfs LittleFS : SUCCESS ;
- diagnostic système/Web, FaultManager, alarmes Web et priorité RGB intégrés ;
- moteur intervalle à ancre fixe conservé ;
- validation matérielle complète après merge final restant à effectuer.

Lire AGENTS.md, docs/codex/, le checkpoint ci-dessus et ROADMAP.md avant toute modification.
Ne pas fusionner feature/resilience-coupures en bloc ; réimplémenter sélectivement depuis main.
```
