# 11 — OTA-3.0 : téléchargement et validation du firmware

## Statut

- Branche : `agent/ota-3.0-download-test-v591`
- Base fonctionnelle : tag `v5.9.1`
- Commit code validé avant documentation : `6808f58bb0f386a17a2c24d5bb25fe0500410d43`
- Version embarquée pendant le test : `5.9.1`
- Release distante détectée : `5.9.2`
- Cible matérielle : `v4`
- Environnement PlatformIO : `ProgrammeArrosage_v4`
- Carte : `esp32-2432S028`
- Port utilisé lors du test : `COM3`
- Date de validation matérielle : 2026-07-30

## Lecture obligatoire avant toute évolution OTA

Lire dans cet ordre :

1. `AGENTS.md`
2. `docs/codex/00_CONTEXT.md`
3. `docs/codex/03_INVARIANTS.md`
4. `docs/codex/05_BUILD_AND_TEST.md`
5. le présent document
6. `docs/checkpoints/CHECKPOINT_2026-07-30_OTA-3.0_DOWNLOAD_VERIFIED.md`
7. `src/MaintenanceBoot.cpp`
8. `src/MaintenanceRequest.h` et `src/MaintenanceRequest.cpp`
9. `src/MaintenanceResult.h` et `src/MaintenanceResult.cpp`
10. `src/OtaDownloadTest.h` et `src/OtaDownloadTest.cpp`
11. `src/WebManager.h`

Ne jamais reprendre l’OTA depuis un extrait de conversation ou un ancien patch. La branche Git et ces documents constituent la source de vérité.

## Fonctionnalités validées sur matériel

### CHECK_VERSION

La chaîne suivante a été validée :

```text
Demande Web
→ enregistrement NVS
→ redémarrage en maintenance minimale
→ connexion Wi-Fi
→ connexion TLS GitHub
→ lecture du manifeste
→ parsing JSON
→ sélection de la cible v4
→ comparaison 5.9.1 / 5.9.2
→ persistance du résultat
→ redémarrage normal
```

Résultats observés :

- HTTP 200 ;
- manifeste : 868 octets ;
- firmware attendu : 1 365 088 octets ;
- mise à jour disponible ;
- cible : `v4` ;
- environnement : `ProgrammeArrosage_v4`.

### DOWNLOAD_UPDATE_TEST

La chaîne suivante a été validée :

```text
Résultat CHECK_VERSION valide
→ demande Web de test
→ redémarrage en maintenance minimale
→ connexion Wi-Fi
→ téléchargement complet du firmware GitHub
→ calcul SHA-256 en flux
→ comparaison taille et SHA-256
→ persistance du résultat
→ redémarrage normal
```

Résultats observés :

- octets téléchargés : 1 365 088 ;
- durée observée : 54 769 ms sur partage de connexion iPhone ;
- taille conforme ;
- SHA-256 conforme ;
- résultat : `firmware-download-verified` ;
- aucune écriture dans une partition OTA.

## Interface Web `/ota`

La page expose :

- vérification de version ;
- test de téléchargement et SHA-256 ;
- test de connexion GitHub ;
- affichage du dernier résultat persistant ;
- polling de `/api/maintenance/last-result` toutes les deux secondes ;
- rechargement automatique après détection d’un nouveau résultat ;
- roue animée, barre indéterminée, compteur de temps et messages d’étapes pendant l’attente.

Limite ergonomique connue : la zone d’attente apparaît sous les boutons et peut être située bas dans la fenêtre sur un petit écran. Une évolution future pourra la déplacer au-dessus des boutons ou la rendre fixe, sans modifier la logique OTA.

## Fichiers concernés

- `src/MaintenanceBoot.cpp` : sélection de la commande et exécution minimale.
- `src/MaintenanceRequest.h/.cpp` : commande `DOWNLOAD_UPDATE_TEST`.
- `src/MaintenanceResult.h/.cpp` : persistance des tailles, durée et SHA calculé.
- `src/OtaDownloadTest.h/.cpp` : téléchargement HTTPS, redirections, taille et SHA-256.
- `src/WebManager.h` : routes, JSON, page `/ota`, polling et attente animée.

## Invariants et restrictions

- `client.setInsecure()` est encore utilisé pour ce jalon expérimental.
- Ne pas présenter cette étape comme une validation CA ou une chaîne de confiance de production.
- Aucune API `Update` n’est appelée.
- Aucune partition OTA n’est écrite.
- La partition active et la partition inactive ne sont pas modifiées par `DOWNLOAD_UPDATE_TEST`.
- Une opération est refusée si une zone d’arrosage est active.
- Le firmware Legacy doit continuer à compiler.
- Le firmware V4 doit compiler et être testé sur matériel pour toute évolution OTA.
- Le port COM doit être reconfirmé avant chaque téléversement.

## Prochaine étape autorisée

Le palier suivant peut préparer l’écriture contrôlée dans la partition OTA inactive, mais seulement après décision explicite et avec :

1. validation TLS par CA ou mécanisme de confiance documenté ;
2. contrôle de taille avant écriture ;
3. contrôle SHA-256 avant activation ;
4. gestion d’échec et rollback ;
5. journalisation détaillée ;
6. tests sur la seule carte V4 sous surveillance ;
7. conservation du chemin Legacy compilable.
