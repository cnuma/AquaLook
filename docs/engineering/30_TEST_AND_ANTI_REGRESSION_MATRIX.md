# AquaLook Engineering Reference — Matrice de test et anti-régression

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Sources : `platformio.ini`, bancs `src/test_*.cpp`, code des managers et checkpoints
- Maturité : D4

## Objet

Cette matrice relie chaque domaine critique aux environnements PlatformIO, aux contrôles fonctionnels, aux tests dégradés et au niveau de preuve attendu.

## Matrice consolidée

| Domaine | Build obligatoire | Test nominal | Test dégradé | Preuve minimale |
|---|---|---|---|---|
| Scheduler | legacy + V4 | déclenchement, fin, simultanéité | Wi-Fi/NTP absents | P4, P5 si relais |
| configuration/NVS | legacy + V4 | sauvegarde, relecture, reboot | schéma invalide ou valeurs hors bornes | P4 |
| relais/topologie | legacy + V4 + `test_relais` | voie unique, logique et mapping | carte absente, erreur I²C | P5 |
| moteur V4 | V4 + `test_execution_engine` | plans start/stop | lien ou équipement invalide | P4 |
| Web/API | legacy + V4 | chaque GET/POST et effet | JSON invalide, ressource absente | P4 |
| affichage | legacy + V4 | vues, refresh, hot-reload | écran en veille, état réseau absent | P5 |
| tactile | `calibration` + firmware | coordonnées et actions | dalle absente/non calibrée | P5 |
| SD/LittleFS | firmware + `buildfs` si besoin | résolution SD/LittleFS/firmware | SD absente ou retirée | P4/P5 |
| Wi-Fi | firmware | connexion et portail | SSID absent, 5 échecs, retour réseau | P4 |
| NTP/EventLog | firmware | synchro et chronologie réelle | NTP absent | P4 |
| météo | legacy + V4 | fetch et données 5 jours | clé absente, HTTP KO, JSON invalide | P4 |
| sécurité | builds impactés | succès autorisé | refus, secret absent, entrée hostile | P4 |

## Environnements de test réels

- `test_execution_engine` : moteur passif, sans runtime ni relais physique ;
- `test_relais` : diagnostic I²C et séquence relais ;
- `calibration` : acquisition et calibration XPT2046 ;
- `debug_boot` : diagnostic interactif du démarrage ;
- `ProgrammeArrosage_legacy` et `ProgrammeArrosage_v4` : firmwares complets à comparer.

## Contrôles transverses obligatoires

```powershell
git diff --check
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4
```

Selon le périmètre :

```powershell
pio run -e test_execution_engine
pio run -e test_relais
pio run -e calibration
pio run -e ProgrammeArrosage -t buildfs
```

## Règles anti-régression

- vérifier le point d’appel réel de toute fonction ajoutée ;
- tester tous les modes affectés, pas seulement le chemin nominal ;
- préserver le fallback legacy tant que son retrait n’est pas explicitement validé ;
- comparer les comportements legacy et V4 sur les cas communs ;
- ne jamais confondre dry-run pompe et commande physique ;
- vérifier les routes, méthodes, champs JSON et codes HTTP ;
- vérifier les priorités SD/LittleFS/firmware ;
- contrôler les secrets dans le diff et les logs série ;
- archiver le commit, les commandes et les observations dans le checkpoint.

## Corrections de référence

L’ancien libellé « bascule EventLog de `millis()` vers heure absolue » est retiré : le code courant stocke uniquement un timestamp relatif `millis()`. Le test porte sur ce comportement réel tant qu’une évolution d’horodatage n’est pas implémentée.

## Critères de blocage

- compilation requise en échec ;
- test ciblé non exécuté alors qu’il est disponible ;
- sécurité de durée relais supprimée ou contournée ;
- divergence legacy/V4 non expliquée ;
- route ou schéma persistant modifié sans contrat mis à jour ;
- secret présent dans le diff, les logs ou les captures ;
- changement matériel sans preuve P5 ou mention explicite non validée ;
- documentation D4 contredite par le code.

## Traçabilité exigée

Chaque bilan de test indique :

- commit testé ;
- environnement ;
- commande exacte ;
- cible et port ;
- résultat de compilation ;
- résultat d’upload ;
- observation fonctionnelle ;
- tests dégradés ;
- limites et tests non réalisés.

## Références

- `platformio.ini` ;
- `17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md` ;
- `src/test_execution_engine.cpp` ;
- `src/test_relais.cpp` ;
- `src/calibration_touch.cpp` ;
- `docs/architecture/QUALITY.md` ;
- `docs/codex/06_ANTI_REGRESSION.md`.

## Historique

### 1.1

Consolidation D4 des environnements, preuves et critères d’anti-régression.