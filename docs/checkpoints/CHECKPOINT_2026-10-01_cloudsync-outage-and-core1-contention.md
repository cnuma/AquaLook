# Checkpoint — Panne CloudSync de ~22h40 résolue ; gel résiduel cœur 1 identifié, correctif tenté puis retiré (NON résolu)

- Date : 2026-10-01
- Branche : `feat/moteur-de-regles`
- Commit de base (HEAD au début de la session) : `4176218`
  (`fix(cloud): recul vers l'intervalle nominal apres 3 echecs CloudSync`)
- **Aucun commit créé pendant cette session** — tous les changements
  décrits ci-dessous sont dans l'arbre de travail, non commités, en
  attente de validation longue durée et d'une décision explicite de
  commit.

## Objectif

Deux chantiers enchaînés :

1. Diagnostiquer et résoudre une panne CloudSync continue depuis le
   29 sept. 2026 ~09h57 (dernier succès) jusqu'au 1er oct. ~08h37
   (premier succès après reprise), soit environ 22h40 d'échec à 100 %.
2. À la demande explicite de l'utilisateur après résolution, auditer et
   corriger un gel résiduel de la boucle principale (jusqu'à 543,7 ms,
   mesuré sur la lecture tactile pendant la veille écran).

## Matériel et environnement PlatformIO

- Carte : ESP32-S3 JC4827W543C_I, module de production `.141`
  (`192.168.1.141`, dev mais zones/relais réels câblés).
- Environnement : `ProgrammeArrosage_s3` (production) pour le résultat
  final ; sept nouveaux environnements de bancs isolés créés et utilisés
  pendant la campagne (voir plus bas), tous non commités.
- Port : COM4, `monitor_dtr = 1` obligatoire (USB natif S3).

## Partie 1 — Résolution de la panne CloudSync (aucun changement de code nécessaire)

### Campagne d'isolation (huit bancs successifs, tous RAS)

Nouveau fichier `src/test_cloudsync_probe_common.h/.cpp` (logique
partagée : connexion WiFi + reproduction fidèle des deux phases réelles de
`CloudSync::run()` — `POST /v1/report` et `GET /v1/pending-command`, mêmes
en-têtes, mêmes timeouts que `CloudSync.cpp`) puis six variantes, chacune
ajoutant un seul composant :

| Étape | Fichier | Ajout testé | Résultat |
|---|---|---|---|
| 1 | `test_cloudsync_probe.cpp` | rien (TLS+requête seule) | 8/8 puis 5/5 cycles — écarte blocage IP/matériel et blocage par contenu de requête |
| 2 | `test_cloudsync_probe_asynctcp.cpp` | AsyncTCP/ESPAsyncWebServer + trafic LAN réel | 5/5 (147 req LAN) |
| 3 | `test_cloudsync_probe_sd.cpp` | accès SD périodiques (bus HSPI dédié) | 5/5 (57 E/S, 0 erreur) |
| 4 | `test_cloudsync_probe_display.cpp` | rafraîchissements écran QSPI | 5/5 (81 rafraîchissements) |
| 5 | `test_cloudsync_probe_combined.cpp` | SD+AsyncTCP+écran ensemble | 8/8 (0 erreur, 257 req LAN) |
| 6 | `test_cloudsync_probe_wifimanager.cpp` | sonde keepalive WiFiManager reproduite sur le MÊME cœur que CloudSync (cadence accélérée 7s) | 8/8 (23 sondes, 0 collision) |
| 7 | (réutilise l'étape 1) | 8h en continu sur le banc le plus simple | 1146/1147 cycles, 1 seul échec isolé auto-résolu, heap/RSSI parfaitement stables |

### Verdict

Aucune des huit hypothèses testées n'a reproduit la panne. La reprise du
firmware de production tel quel (sans aucun changement de code) a suffi :
premier succès CloudSync à 08:37:44 le 1er oct., confirmé stable sur
plusieurs dizaines de cycles consécutifs ensuite (intervalle nominal
respecté, recul après échec isolé observé en direct).

**Conclusion retenue** : état transitoire côté hébergeur/réseau, déclenché
par le plantage du 29 sept. matin combiné au défaut de martelage à 30s
(déjà corrigé par le commit `4176218`, présent dans cette même session mais
antérieur), résorbé avec le temps. Non confirmée par un accès aux journaux
serveur (hors de portée depuis ce poste) — voir
`docs/engineering/18_NETWORK_AND_WIFI.md`, section "Écarts ouverts".

### Fichiers créés (non commités)

`src/test_cloudsync_probe_common.h`, `src/test_cloudsync_probe_common.cpp`,
`src/test_cloudsync_probe.cpp`, `src/test_cloudsync_probe_asynctcp.cpp`,
`src/test_cloudsync_probe_sd.cpp`, `src/test_cloudsync_probe_display.cpp`,
`src/test_cloudsync_probe_combined.cpp`,
`src/test_cloudsync_probe_wifimanager.cpp`, plus sept entrées
`[env:test_cloudsync_probe_s3*]` ajoutées à `platformio.ini` (avec
exclusion correspondante ajoutée à `[env:ProgrammeArrosage]` et
`[env:debug_boot]` pour éviter un conflit `setup()`/`loop()` au prochain
build de production — étape indispensable, sans quoi le build de
production échoue).

## Partie 2 — Gel résiduel de boucle (cœur 1), identifié ; correctif tenté puis retiré (NON résolu)

### Découverte

`/api/diagnostics -> runtimeComponents.display` dominait largement les
autres composants (jusqu'à 571 ms) alors que ses deux sous-mesures
existantes (`displayFullRedraw`, `displayDynamic`) restaient chacune sous
100 ms. Lecture de `DisplayManager::update()` : la branche "écran en
veille" appelle `getTouchPoint()` (lecture I2C GT911) DIRECTEMENT, sans
passer par `handleTouch()` — donc sans être couverte par le composant
`DISPLAY_TOUCH` du profileur, malgré le commentaire de ce composant
prétendant le contraire.

Nouveau composant `RuntimeProfiler::Component::DISPLAY_SLEEP_TOUCH` ajouté
et flashé en production. **Confirmé par mesure directe** : jusqu'à
543 763 µs (543,7 ms) — très au-delà du pire cas théorique d'un simple
timeout I2C (`Wire.setTimeOut(10)`, 2 transactions/appel ≈ 20-30 ms), donc
signe d'une préemption de tâche plutôt que d'un blocage matériel I2C.

Vérifié par lecture de `include/config.h` que le tactile (`Wire`) et le
relais (`RELAY_WIRE_BUS` = `Wire1` sur S3) sont bien sur deux périphériques
I2C matériels séparés — hypothèse d'un bus partagé écartée.

### Audit complet des tâches FreeRTOS (cœur 0 / cœur 1)

Voir `docs/engineering/15_RUNTIME_AND_PROFILING.md`, section "Tâches
dédiées et répartition cœur 0 / cœur 1" pour le tableau complet et le
détail des fichiers/lignes. Constat central : `loopTask` et les trois
tâches réseau du cœur 1 (`cloud-sync`, `weather-fetch`, `wifi-keepalive`)
tournaient toutes à priorité FreeRTOS égale (1) — aucune ne pouvait
préempter les autres, contrairement à AsyncTCP (priorité 10) déjà isolée
au cœur 0 depuis le 28 sept. 2026 pour la même classe de problème.

### Correctif tenté PUIS RETIRÉ (résultat négatif)

`src/main.cpp`, en tête de `setup()` — déployé puis retiré le même jour :

```cpp
vTaskPrioritySet(NULL, 2);  // RETIRE -- voir ci-dessous
```

Élevait `loopTask` strictement au-dessus des trois tâches réseau du cœur 1
(restées à 1). Déployé sur `.141`, observé ~10 minutes, puis **retiré**
pour deux raisons convergentes :

1. **N'a pas résolu le problème ciblé** : un nouveau pic de 557 761 µs
   (557,8 ms) sur `DISPLAY_SLEEP_TOUCH` mesuré MALGRÉ le changement —
   magnitude quasi identique au pic d'avant correctif (543,7 ms). Indique
   que la cause n'est probablement PAS une simple inégalité de priorité
   FreeRTOS entre tâches de même cœur — hypothèse de départ invalidée,
   pas seulement insuffisamment réglée.
2. **A coïncidé avec une dégradation de CloudSync**, stable depuis sa
   reprise (partie 1) : 3 échecs consécutifs immédiatement après ce
   flash, dont un avec la signature SÉVÈRE de l'ancienne panne de 22h40
   (plafond de sécurité 25 s déclenché, 45,7 s de blocage, erreur mbedTLS
   -78) — jamais revue depuis la reprise. Hypothèse retenue mais NON
   prouvée formellement : préempter plus agressivement `cloud-sync`
   pendant sa poignée de main TLS peut étirer sa durée jusqu'à provoquer
   un reset côté serveur/réseau.

**Code revenu à l'état d'avant la tentative** (appel supprimé, commentaire
explicatif détaillé laissé à l'emplacement exact dans `src/main.cpp`) dès
que le second signal (dégradation CloudSync) a été observé, sans attendre
une confirmation statistique complète — le risque sur une fonctionnalité
critique tout juste rétablie dépassait le gain non confirmé sur un
problème secondaire. **Le gel tactile de boucle reste NON RÉSOLU** à la
clôture de ce checkpoint ; voir "Écarts" ci-dessous pour la suite
recommandée.

**Un diagnostic temporaire distinct a aussi été ajouté PUIS RETIRÉ par
prudence**, avant la tentative `vTaskPrioritySet` ci-dessus : un log
`EventLog` dans `WiFiManager::keepaliveProbeTask()` (même cœur 1 que
CloudSync) ajouté pour corréler des horodatages, retiré après un
entrelacement de sortie série suggérant un effet observateur. Code revenu
à l'original, hormis un commentaire explicatif.

## Commandes de compilation et uploads réellement exécutés

```
pio run -e ProgrammeArrosage_s3
pio run -e ProgrammeArrosage_s3 -t upload --upload-port COM4
```

Buildé et flashé avec succès SIX fois pendant la session (reprise
production initiale, ajout `DISPLAY_SLEEP_TOUCH`, ajout puis retrait du
log keepalive, ajout PUIS retrait de `vTaskPrioritySet`). Chaque build a
réussi sans erreur ; `.pio/libdeps/.../SdFat/src/SdFat.h:461` émet un
avertissement `#warning` préexistant, sans rapport avec ces changements.

Les huit environnements de bancs isolés (`test_cloudsync_probe_s3*`) ont
tous été buildés ET flashés avec succès pendant la Partie 1.

## Résultats, mesures et logs utiles

- CloudSync : 08:37:44 premier succès, puis succès répétés à intervalle
  nominal (5 min, `intervalMinutes` configuré par l'utilisateur) sur plus
  d'une heure d'observation continue au moment de la rédaction ; un seul
  épisode de 3 échecs consécutifs (09:46-09:47) expliqué par le
  rattrapage de configuration après redémarrage (`lastSyncedRevision`
  reparti à la valeur sentinelle, donc deux connexions par cycle au lieu
  d'une le temps du rattrapage) — recul vers 5 min confirmé après le 3e
  échec.
- `DISPLAY_SLEEP_TOUCH` avant tentative : pic mesuré à 543 763 µs.
- `DISPLAY_SLEEP_TOUCH` ~5 min après déploiement de `vTaskPrioritySet` :
  0 événement "lent" (>50 ms) — résultat initialement encourageant, mais
  **contredit ~10 min plus tard** par un nouveau pic à 557 761 µs,
  confirmant que le correctif ne protège pas de façon fiable contre le
  phénomène (voir "Partie 2" ci-dessus pour l'analyse complète et la
  décision de retrait).
- 3 échecs CloudSync consécutifs mesurés immédiatement après le flash de
  `vTaskPrioritySet` (10:18:23, 10:19:10, 10:19:55 — le dernier avec
  plafond de sécurité déclenché, 45,7 s, erreur -78), contre une stabilité
  totale sur l'heure et demie précédente. `vTaskPrioritySet` retiré et
  reflashé à 10:2x ; reprise de l'observation CloudSync post-retrait en
  cours à la clôture de ce checkpoint (voir "Écarts" pour l'état exact au
  moment de la rédaction).

## Décisions et invariants confirmés

- `INV-RUN-007`, `INV-RUN-008` (nouveaux, `docs/engineering/15_RUNTIME_AND_PROFILING.md`).
- `INV-NET-008`, `INV-NET-009`, `INV-NET-010` (nouveaux, `docs/engineering/18_NETWORK_AND_WIFI.md`).
- Préférence utilisateur actée en mémoire de session : face à un module
  montrant un motif de correctifs accumulés sans refonte (ex. `WiFiManager`,
  16 commits `fix(wifi)` successifs), proposer une réécriture propre plutôt
  qu'un patch de plus — pas encore appliquée ici, aucun module n'a été
  formellement identifié comme nécessitant cette réécriture à ce stade.

## Écarts, risques et limitations

- **`DISPLAY_SLEEP_TOUCH` (gel tactile en veille, jusqu'à 543-558 ms) reste
  NON RÉSOLU.** La tentative `vTaskPrioritySet` a été invalidée par la
  mesure elle-même (pic revenu à magnitude quasi identique malgré le
  changement) — la vraie cause reste à trouver. Ne pas repartir sur
  l'hypothèse "priorité FreeRTOS égale" sans nouvel élément ; envisager
  plutôt : instrumentation plus fine à l'intérieur même de `getTouchPoint()`
  (isoler la portion I2C de la portion "attente de planification"), ou
  observation de ce qui tourne précisément sur le cœur 1 au moment exact
  d'un pic (nécessiterait une instrumentation sans écriture série pour
  éviter l'effet observateur déjà rencontré avec le log keepalive).
- CloudSync : stable avant la tentative `vTaskPrioritySet` (08:37-10:18,
  ~1h40, RSSI sain -62 à -64dBm tout du long), dégradé pendant (3 échecs
  consécutifs 10:18-10:20, DONT DEUX avec la signature sévère complète —
  plafond de sécurité 25s puis échec à 45-46s, pas un seul comme
  initialement caractérisé), RSSI vérifié SAIN (-64dBm) sur cette même
  fenêtre — écarte une coïncidence d'antenne, renforce l'imputation à
  `vTaskPrioritySet`. Correctif retiré et reflashé immédiatement après.
  **Complication pour la vérification post-retrait** : le redémarrage qui
  a suivi le retrait s'est reconnecté avec un RSSI dégradé (-89dBm) —
  problème d'antenne S3 intermittent déjà rencontré par le passé (connecteur
  marginal, un reconnect logiciel ne retablit pas toujours une bonne liaison
  RF, sans lien avec ce chantier), qui peut perturber les tout prochains
  cycles CloudSync indépendamment du retrait. **Premier signal positif
  obtenu avant clôture** : le tout premier cycle CloudSync après le
  retrait (10:28:22) a réussi (`rapport=ok config=ok`), malgré un RSSI
  toujours dégradé (-88dBm) à ce moment — cohérent avec un retour à la
  stabilité, mais un seul cycle ne suffit pas à le confirmer
  définitivement ; à vérifier sur plusieurs cycles supplémentaires à la
  reprise (`/api/adminStatus -> wifi.rssi` et `cloudSync.lastSyncOk`).
- La cause exacte côté hébergeur de la panne de 22h40 n'est pas confirmée
  (pas d'accès aux journaux serveur depuis ce poste).
- Rien n'est commité : la reprise par un autre ingénieur doit d'abord
  relire `git status`/`git diff` avant toute action — `src/main.cpp` est
  revenu à son comportement d'origine (pas de `vTaskPrioritySet`), mais le
  commentaire explicatif de la tentative y reste, à relire avant de
  retenter quoi que ce soit sur ce point précis.
- Fichiers non liés à ce chantier déjà présents comme modifiés dans l'arbre
  de travail au début de la session (`data/app.js`, `data/index.html`,
  `data/style-base.css`, `src/OtaBuildIdentity.h`, `src/SystemDiagnostics.cpp`,
  `src/WebManager.cpp`, `cloud/php-mutualized/*`, `tools/soak/ledger.json`)
  — probablement une autre session en parallèle, ne pas les committer par
  erreur avec ce chantier.

## Procédure de reprise

1. `git status` et `git diff` pour voir l'état exact (rien n'est commité).
2. **Priorité 1 : confirmer le retour à la stabilité CloudSync** après le
   retrait de `vTaskPrioritySet` — plusieurs cycles réussis consécutifs à
   l'intervalle nominal, aucune récidive de la signature sévère
   (plafond de sécurité + erreur -78). Si une instabilité persiste malgré
   le retrait, elle n'est PAS liée à `vTaskPrioritySet` (déjà retiré) —
   reprendre l'investigation de la Partie 1 depuis le début.
3. **Priorité 2 : reprendre la recherche de la vraie cause du gel tactile**
   (`DISPLAY_SLEEP_TOUCH`) une fois CloudSync confirmé stable, SANS
   réessayer `vTaskPrioritySet` tel quel. Voir pistes ci-dessus.
4. Une fois l'un ou l'autre chantier conclu : commit séparé pour chaque
   élément validé (pas tout en un seul commit, voir mémoire
   `commit-per-validated-change`) — ne committer QUE ce qui est
   effectivement validé sur matériel, pas la tentative `vTaskPrioritySet`
   qui a été retirée.
5. Avant tout nouveau flash de `.141` : pas de fenêtre de maintenance
   requise pour cette campagne (autorisation explicite de l'utilisateur,
   contexte dev, pas de prod derrière).

## Documents Engineering, Firmware et Developer mis à jour

- `docs/engineering/15_RUNTIME_AND_PROFILING.md` → 1.2
- `docs/engineering/18_NETWORK_AND_WIFI.md` → 1.2
- Ce checkpoint.

## Références

- `docs/engineering/15_RUNTIME_AND_PROFILING.md`
- `docs/engineering/18_NETWORK_AND_WIFI.md`
- `docs/architecture/CLOUD_REMOTE_CONFIG.md`
- `src/CloudSync.h`, `src/CloudSync.cpp`
- `src/WiFiManager.h`, `src/WiFiManager.cpp`
- `src/DisplayManager.cpp`
- `src/RuntimeProfiler.h`, `src/RuntimeProfiler.cpp`
- `src/main.cpp`
