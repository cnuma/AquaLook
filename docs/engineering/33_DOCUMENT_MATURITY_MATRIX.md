# AquaLook Engineering Reference — Matrice de maturité documentaire

- Version documentaire : 1.1
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
| relais | 08 | D3 | chaîne générale identifiée | API backend et topologies exactes |
| Web | 09 | D3 | WebManager identifié | inventaire exhaustif routes/méthodes |
| temps et EventLog | 10 | D3 | appels principaux identifiés | API EventLog et formats JSON |
| affichage et tactile | 13 | D3 | fichiers et ordre Runtime identifiés | API et fonctions de rendu exactes |
| SD et ressources | 14, 27 | D3 | handlers et ordre de fallback identifiés | manifeste et tests automatisés |
| modèle V4 et météo | 16 | D2 | objets Runtime identifiés | frontières et données stabilisées |
| build et validation | 17, 30 | D3 | commandes réelles documentées | CI et preuves archivées |
| réseau | 18 | D3 | managers et ordre Runtime identifiés | états et timeouts exacts |
| HTTPS et sessions | 19 | D2 | architecture cible | implémentation et tests négatifs |
| MQTT | 20 | D2 | architecture cible | topics et schémas confirmés |
| OTA | 21 | D2 | architecture cible | partitions et signature validées |
| sécurité | 23 | D3 | registre de risques | procédures testées |
| observabilité | 24 | D3 | EventLog et profiler identifiés | formats, rétention et page diagnostic |
| maintenance | 25 | D3 | procédure documentaire | exercice de restauration |
| catalogues et index | 28 à 35 | D4 | index et registre de traçabilité | mise à jour automatique ou checkpoint |

## Priorités suivantes

1. relais et backend physique ;
2. Web et inventaire des routes ;
3. EventLog et diagnostics JSON ;
4. affichage/tactile ;
5. réseau et timeouts.

## Règles de progression

- aucun D4 sans fichiers et symboles réels ;
- aucun D5 sans tests exécutés et preuve archivée ;
- tout comportement matériel exige une preuve P5 ;
- une modification du code peut faire redescendre temporairement la maturité.
