# AquaLook Engineering Reference — Matrice de maturité documentaire

- Version documentaire : 1.5
- Statut : active
- Dernière consolidation : 2026-07-27
- Maturité : D4

## Niveaux

| Niveau | Définition |
|---|---|
| D0 | absent |
| D1 | description fonctionnelle |
| D2 | architecture et interfaces |
| D3 | invariants, séquences et tests |
| D4 | exploitable par un nouvel ingénieur et relié aux symboles principaux |
| D5 | référence validée, maintenue, testée et reliée au code |

## Matrice courante

| Domaine | Document | Niveau | Preuve actuelle | Prochaine condition |
|---|---|---:|---|---|
| gouvernance | 03 à 05, 11 | D4 | règles versionnées | audit d’application sur checkpoint |
| Scheduler | 06 | D4 | API, structures et callback extraits | tests automatisés complets |
| configuration | 07 | D4 | clés NVS, schéma, structures et boot extraits | tests de migration archivés |
| Runtime | 15 | D4 | ordre exact `setup()`/`loop()` et chaîne de commande | seuils profiler et tests automatisés |
| relais | 08 | D4 | API, topologie, contrôleurs, logique et chaîne physique extraits | matrice matérielle par topologie et preuves P5 |
| Web | 09 | D4 | inventaire exhaustif des routes et méthodes | tests automatisés de contrat et sécurité |
| temps et EventLog | 10 | D4 | structure, capacité, API, routes et horodatage réel | tests de concurrence |
| observabilité | 24 | D4 | API, seuils et schéma JSON | tests de contrat JSON et de charge |
| affichage et tactile | 13 | D4 | API, bus, modes, cadences, tactile et redraw | tests automatisés et calibration P5 |
| réseau | 18 | D4 | machine d’états, délais, retries, portail et scan | sécurisation AP et tests automatiques |
| SD et ressources | 14, 27 | D4 | API, états, sentinelle, manifeste URL/support, MIME et fallbacks | manifeste de carte versionné |
| modèle V4 et météo | 16, 36 | D4 | structures, relations, validations, plans, limites pompe et tâche météo | persistance stabilisée, HTTPS et tests automatiques |
| build et validation | 17, 30 | D4 | environnements, filtres, commandes, preuves et blocages extraits | CI et preuves archivées |
| HTTPS et sessions | 19 | D2 | architecture cible | implémentation et tests négatifs |
| MQTT | 20 | D2 | architecture cible | topics et schémas confirmés |
| OTA | 21 | D2 | architecture cible | partitions et signature validées |
| sécurité | 23 | D3 | registre de risques | corrections firmware et procédures testées |
| maintenance | 25 | D3 | procédure documentaire | exercice de restauration |
| catalogues et index | 28 à 36 | D4 | index, registre et schéma détaillé | automatisation ou checkpoint |

## Priorités suivantes

1. correction firmware des écarts de cybersécurité confirmés ;
2. contrats automatisés HTTP, stockage, réseau et météo ;
3. persistance/versionnement du modèle d’équipements ;
4. manifeste versionné de la carte SD de référence ;
5. passage D5 des domaines disposant de preuves reproductibles.

## Règles de progression

- aucun D4 sans fichiers et symboles réels ;
- aucun D5 sans tests exécutés et preuve archivée ;
- tout comportement matériel exige une preuve P5 ;
- une modification du code peut faire redescendre temporairement la maturité ;
- une incohérence découverte pendant la consolidation doit être documentée, pas masquée.