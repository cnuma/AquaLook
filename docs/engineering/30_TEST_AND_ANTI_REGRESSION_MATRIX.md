# AquaLook Engineering Reference — Matrice de test et anti-régression

- Version documentaire : 1.0
- Statut : référence initiale
- Dernière consolidation : 2026-07-27
- Sources : AGENTS.md, checkpoints et stratégie qualité
- Maturité : D3

## Objet

Cette matrice relie les domaines critiques aux contrôles minimaux exigés avant intégration.

## Matrice

| Domaine | Validation statique | Test fonctionnel | Test dégradé | Preuve attendue |
|---|---|---|---|---|
| Scheduler | compilation, invariants | déclenchement et fin | sans Wi-Fi / NTP | logs et observation |
| relais | compilation, logique directe/inverse | activation d’une zone courte | contrôleur absent | mesure matérielle |
| configuration | schéma et bornes | sauvegarde / relecture | NVS invalide | état après reboot |
| Web | routes et IDs | GET/POST réel | ressource absente | réponse HTTP et effet |
| affichage | chemin de redraw | rendu et hot-reload | tactile ou écran indisponible | observation LCD |
| tactile | initialisation SPI | coordonnées et actions | contrôleur absent | test matériel |
| SD / LittleFS | `buildfs` si `data/` change | résolution des ressources | SD retirée | chargement des fallbacks |
| NTP / EventLog | compilation | bascule `millis()` vers heure absolue | serveur NTP absent | chronologie des logs |
| Runtime | analyse du point d’appel | boucle non bloquante | services distants absents | profiler et fonctionnement |
| sécurité | revue des secrets et entrées | succès et refus | heure ou stockage absent | journaux sans secrets |
| OTA / MQTT | contrat et menaces | selon implémentation | réseau interrompu | preuve de repli |

## Chaîne obligatoire

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4
```

Pour un upload matériel :

```powershell
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Si `data/` est modifié :

```powershell
pio run -e ProgrammeArrosage -t buildfs
```

Les noms d’environnements et ports sont confirmés dans le checkpoint courant avant exécution.

## Contrôles anti-régression

- vérifier le point d’appel de toute fonction ajoutée ;
- vérifier les modes concernés, pas seulement le cas nominal ;
- examiner `git diff --check` ;
- rechercher les duplications HTML, CSS, JS et IDs ;
- contrôler la taille LittleFS ;
- conserver les fallbacks legacy tant que leur retrait n’est pas validé ;
- documenter les tests matériels non réalisables ;
- ne jamais confondre compilation, upload et validation fonctionnelle.

## Critères de blocage

Une intégration est bloquée si :

- la compilation requise échoue ;
- un secret apparaît dans le diff ;
- une sécurité relais est supprimée ;
- une route ou un schéma persistant change sans documentation ;
- le résultat fonctionnel demandé n’est pas relié à un point d’exécution ;
- un fichier livré n’a pas été validé avec l’outil cible alors que cela était possible.

## Références

- `17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md` ;
- `docs/architecture/QUALITY.md` ;
- `docs/codex/06_ANTI_REGRESSION.md` ;
- `AGENTS.md`.
