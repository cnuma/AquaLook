# AquaLook Engineering Reference — Matrice de maturité documentaire

- Version documentaire : 1.4
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
| Web | 09 | D4 | inventaire exhaustif des routes et méthodes du firmware courant | tests automatisés de contrat et sécurité |
| temps et EventLog | 10 | D4 | structure, capacité, API, routes et horodatage réel extraits | tests de concurrence et évolution double horodatage éventuelle |
| observabilité | 24 | D4 | API, seuils et schéma JSON extraits | tests automatiques du contrat JSON et de charge |
| affichage et tactile | 13 | D4 | API, bus, modes, cadences, tactile et redraw extraits | tests automatisés et preuves de calibration P5 |
| réseau | 18 | D4 | machine d’états, délais, retries, portail et scan extraits | tests automatisés et sécurisation du portail captif |
| SD et ressources | 14, 27 | D4 | API, états, sentinelle, manifeste URL/support, MIME et fallbacks extraits | manifeste de carte versionné et tests automatiques |
| modèle V4 et météo | 16 | D4 | API EquipmentManager, plans, limites pompe, tâche météo et endpoint extraits | schéma EquipmentModel versionné, HTTPS et tests automatiques |
| build et validation | 17, 30 | D3 | commandes réelles documentées | CI et preuves archivées |
| HTTPS et sessions | 19 | D2 | architecture cible | implémentation et tests négatifs |
| MQTT | 20 | D2 | architecture cible | topics et schémas confirmés |
| OTA | 21 | D2 | architecture cible | partitions et signature validées |
| sécurité | 23 | D3 | registre de risques | suppression des secrets dans les logs, protection AP, HTTPS OWM et procédures testées |
| maintenance | 25 | D3 | procédure documentaire | exercice de restauration |
| catalogues et index | 28 à 35 | D4 | index et registre de traçabilité | mise à jour automatique ou checkpoint |

## Priorités suivantes

1. build, tests et matrice anti-régression ;
2. schéma détaillé du modèle d’équipements ;
3. correction des écarts de cybersécurité détectés ;
4. contrats automatisés HTTP, stockage, réseau et météo ;
5. passage D5 des composants disposant déjà de preuves matérielles.

## Règles de progression

- aucun D4 sans fichiers et symboles réels ;
- aucun D5 sans tests exécutés et preuve archivée ;
- tout comportement matériel exige une preuve P5 ;
- une modification du code peut faire redescendre temporairement la maturité ;
- une incohérence découverte pendant la consolidation doit être documentée, pas masquée.