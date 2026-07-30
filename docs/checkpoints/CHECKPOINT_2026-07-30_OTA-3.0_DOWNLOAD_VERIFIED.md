# Checkpoint AquaLook — OTA-3.0 téléchargement vérifié

## Référence

- Dépôt : `cnuma/AquaLook`
- Branche : `agent/ota-3.0-download-test-v591`
- Base de branche : tag `v5.9.1`
- Commit fonctionnel validé avant documentation : `6808f58bb0f386a17a2c24d5bb25fe0500410d43`
- Document OTA associé : `docs/codex/11_OTA_3_DOWNLOAD_VALIDATION.md`
- Date : 2026-07-30
- Dernière réponse du chat source : `AQL-R050`

## Source de vérité

Reprendre exclusivement depuis la branche Git ci-dessus et lire, dans l’ordre :

1. `AGENTS.md`
2. `docs/codex/00_CONTEXT.md`
3. `docs/codex/01_ARCHITECTURE.md`
4. `docs/codex/03_INVARIANTS.md`
5. `docs/codex/04_DEVELOPMENT_RULES.md`
6. `docs/codex/05_BUILD_AND_TEST.md`
7. `docs/codex/11_OTA_3_DOWNLOAD_VALIDATION.md`
8. le présent checkpoint
9. `platformio.ini`
10. les fichiers source OTA concernés.

Ne pas reprendre depuis un ancien extrait, un ZIP antérieur ou la mémoire d’un chat.

## État validé

### Compilation

```text
ProgrammeArrosage     SUCCESS
ProgrammeArrosage_v4  SUCCESS
```

### Matériel

- Carte V4 flashée sur `COM3`.
- Firmware affiché : `5.9.1`.
- Cible OTA : `v4`.
- Environnement : `ProgrammeArrosage_v4`.
- Layout dual OTA détecté et déclaré prêt.

### CHECK_VERSION

Validé matériellement de bout en bout :

```text
Web → NVS → reboot minimal → Wi-Fi → TLS GitHub → manifeste → JSON
→ sélection v4 → comparaison 5.9.1/5.9.2 → résultat NVS → reboot normal
```

### DOWNLOAD_UPDATE_TEST

Validé matériellement de bout en bout :

```text
Web → NVS → reboot minimal → Wi-Fi → téléchargement complet
→ 1 365 088 octets → SHA-256 conforme → résultat NVS → reboot normal
```

Durée observée : environ 54,8 s sur partage de connexion iPhone.

### Interface `/ota`

Validé :

- bouton de test de téléchargement ;
- résultat détaillé ;
- polling automatique ;
- rechargement automatique après retour du module ;
- indicateur animé ;
- barre indéterminée ;
- compteur de temps ;
- messages d’étape.

Retour utilisateur : fonctionnement nettement amélioré. Limite connue : la zone animée est placée assez bas et peut ne pas être immédiatement visible.

## Fichiers fonctionnels du jalon

- `src/MaintenanceBoot.cpp`
- `src/MaintenanceRequest.h`
- `src/MaintenanceRequest.cpp`
- `src/MaintenanceResult.h`
- `src/MaintenanceResult.cpp`
- `src/OtaDownloadTest.h`
- `src/OtaDownloadTest.cpp`
- `src/WebManager.h`

## Invariants préservés

- aucune écriture dans une partition OTA ;
- aucune utilisation de l’API `Update` ;
- refus pendant un arrosage actif ;
- retour automatique au démarrage normal ;
- persistance NVS du résultat ;
- compilation Legacy conservée ;
- qualification matérielle sur V4 ;
- port COM confirmé avant flash.

## Limites et risques

- `WiFiClientSecure::setInsecure()` est utilisé : aucune validation CA de production ;
- aucune écriture de partition inactive n’est encore implémentée ;
- aucun rollback OTA n’est encore testé ;
- aucun test de coupure réseau pendant téléchargement n’a été réalisé ;
- aucun test de firmware volontairement corrompu n’a été réalisé ;
- la barre Web est indéterminée : elle indique l’activité, pas le pourcentage réel ;
- la branche diverge volontairement de `main`, car elle part de `v5.9.1` afin de tester la détection de `v5.9.2`.

## Procédure de reprise

```powershell
Set-Location "C:\Users\emman\OneDrive\Documents\VsCode_travail\arrosage"

git fetch origin
git switch agent/ota-3.0-download-test-v591
git pull --ff-only origin agent/ota-3.0-download-test-v591

git status
Get-Content VERSION
Get-Content AGENTS.md
Get-Content docs\codex\11_OTA_3_DOWNLOAD_VALIDATION.md
Get-Content docs\checkpoints\CHECKPOINT_2026-07-30_OTA-3.0_DOWNLOAD_VERIFIED.md

pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

Avant tout upload :

```powershell
pio device list
```

Puis utiliser le port réellement confirmé.

## Prochaine décision

Ne pas passer directement à l’installation OTA. Le prochain palier doit d’abord définir le contrat de confiance, l’écriture de la partition inactive, la validation finale, l’activation, le rollback et les scénarios de panne.
