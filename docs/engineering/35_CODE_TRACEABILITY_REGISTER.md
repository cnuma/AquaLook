# AquaLook Engineering Reference — Registre de traçabilité vers le code

- Version documentaire : 1.0
- Statut : actif
- Dernière consolidation : 2026-07-27
- Source : `main`, `AGENTS.md`, checkpoints validés et code du commit ciblé
- Maturité : D4

## Objet

Ce registre transforme progressivement les documents d’architecture en références directement reliées à l’implémentation. Une affirmation technique n’est considérée D4 que si elle est rattachée à un fichier, une classe, une fonction, une constante, une route, une structure persistée ou un test réel.

## Règle de traçabilité

Chaque entrée comporte au minimum :

- composant ;
- document d’architecture ;
- fichier source ou configuration ;
- symbole ou point d’entrée ;
- sens de l’appel ;
- invariant associé ;
- validation disponible ;
- commit ou checkpoint de référence.

## Registre initial

| Composant | Document | Ancrage code confirmé | Point d’entrée / relation | Invariant | Validation |
|---|---|---|---|---|---|
| Scheduler | `06_SCHEDULER.md` | `main.cpp` et implémentation `ScheduleManager` | le callback matériel est câblé dans `main.cpp` ; le Scheduler demande un état et ne pilote pas directement le matériel | `INV-SCH-001` | compilation legacy + V4, essais relais |
| Configuration | `07_CONFIGURATION_AND_PERSISTENCE.md` | implémentation `ConfigManager` | propriétaire unique du montage LittleFS et de la configuration NVS | `INV-CFG-001`, `INV-CFG-002` | redémarrage, lecture/écriture, migration |
| Relais | `08_RELAY_AND_EQUIPMENT_CONTROL.md` | callback matériel dans `main.cpp` et contrôleur relais | application sécurisée de la demande issue du Scheduler | `INV-REL-001` à `INV-REL-004` | test une zone, durée courte, sous surveillance |
| Web | `09_WEB_AND_HTTP_INTERFACES.md` | WebManager, `app.js`, EventBus | routes existantes conservées ; POST d’affichage positionne `EventBus::displayDirty` | `INV-WEB-*` | tests GET/POST, réponse avant reboot |
| EventLog | `10_TIME_AND_EVENTLOG.md` | implémentation `EventLog` | centralisation des événements applicatifs ; timestamps relatifs avant heure absolue | `INV-EVT-*` | logs série et diagnostics JSON |
| Affichage | `13_DISPLAY_AND_TOUCH.md` | DisplayManager, EventBus, configuration tactile | redraw complet contrôlé ; tactile sur bus séparé | `INV-DSP-*` | écran et tactile validés |
| Ressources Web | `14_SD_AND_STATIC_RESOURCES.md` | `SdStaticHandler` | résolution SD → LittleFS → fallback firmware pour le logo | `INV-SD-*` | `/www/logo.png`, fallback vérifié |
| Runtime | `15_RUNTIME_AND_PROFILING.md` | boucle principale et profiler | mesures en temps mural ; `yield()` mesuré mais exclu des alertes métier | `INV-RUN-*` | diagnostics JSON, tests matériels |

## Niveaux de preuve

- **P0** : assertion non reliée au code ;
- **P1** : fichier identifié ;
- **P2** : symbole ou point d’entrée identifié ;
- **P3** : chaîne d’appel et effets identifiés ;
- **P4** : test ou mesure reproductible associé ;
- **P5** : validation sur matériel ou environnement cible et checkpoint référencé.

Un document ne passe à D5 que si ses affirmations critiques atteignent au minimum P4, et P5 pour les comportements matériels.

## Procédure de consolidation

1. partir du commit stable ciblé ;
2. lire le document d’architecture ;
3. identifier les fichiers et symboles réels ;
4. relever les appels entrants et sortants ;
5. comparer code et documentation ;
6. supprimer ou reclasser toute affirmation non confirmée ;
7. ajouter les tests et commandes exacts ;
8. mettre à jour ce registre, la matrice de maturité et le checkpoint.

## Écarts ouverts

- signatures publiques exactes de `ScheduleManager` à relever ;
- clés NVS et versions de schéma à inventorier ;
- inventaire complet des routes et méthodes HTTP à extraire du WebManager ;
- fonctions exactes d’EventLog et format des diagnostics JSON à documenter ;
- chemins précis des implémentations Runtime, DisplayManager et contrôleur relais à inscrire ;
- couverture des tests automatisés à relier aux invariants.

## Références

- `AGENTS.md` ;
- `docs/engineering/05_EDITORIAL_STANDARD.md` ;
- `docs/engineering/30_TEST_AND_ANTI_REGRESSION_MATRIX.md` ;
- `docs/engineering/33_DOCUMENT_MATURITY_MATRIX.md` ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_MAIN_STEP6_CLOSED.md`.
