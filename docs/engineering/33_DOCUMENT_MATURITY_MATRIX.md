# AquaLook Engineering Reference — Matrice de maturité documentaire

- Version documentaire : 1.3
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
| SD et ressources | 14, 27 | D3 | handlers et ordre de fallback identifiés | manifeste et tests automatisés |
| modèle V4 et météo | 16 | D2 | objets Runtime identifiés | frontières et données stabilisées |
| build et validation | 17, 30 | D3 | commandes réelles documentées | CI et preuves archivées |
| HTTPS et sessions | 19 | D2 | architecture cible | implémentation et tests négatifs |
| MQTT | 20 | D2 | architecture cible | topics et schémas confirmés |
| OTA | 21 | D2 | architecture cible | partitions et signature validées |
| sécurité | 23 | D3 | registre de risques | suppression des secrets dans les logs, protection AP et procédures testées |
| maintenance | 25 | D3 | procédure documentaire | exercice de restauration |
| catalogues et index | 28 à 35 | D4 | index et registre de traçabilité | mise à jour automatique ou checkpoint |

## Priorités suivantes

1. SD, ressources statiques et manifeste ;
2. modèle V4 et météo ;
3. build, tests et matrice anti-régression ;
4. correction des écarts de cybersécurité détectés ;
5. automatisation des contrats D4.

## Règles de progression

- aucun D4 sans fichiers et symboles réels ;
- aucun D5 sans tests exécutés et preuve archivée ;
- tout comportement matériel exige une preuve P5 ;
- une modification du code peut faire redescendre temporairement la maturité ;
- une incohérence découverte pendant la consolidation doit être documentée, pas masquée.