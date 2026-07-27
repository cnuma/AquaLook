# AquaLook Engineering Reference — Matrice de maturité documentaire

- Version documentaire : 1.0
- Statut : référence initiale
- Dernière consolidation : 2026-07-27
- Maturité : D4

## Niveaux

| Niveau | Définition |
|---|---|
| D0 | absent |
| D1 | description fonctionnelle |
| D2 | architecture et interfaces |
| D3 | invariants, séquences et tests |
| D4 | exploitable par un nouvel ingénieur |
| D5 | référence validée, maintenue et reliée au code |

## Matrice courante

| Domaine | Document | Niveau | Prochaine condition |
|---|---|---:|---|
| gouvernance | 03 à 05, 11 | D4 | audit d’application sur checkpoint |
| système | 01, 02, 15 | D3 | lier chaque séquence aux fichiers et fonctions |
| planification | 06 | D3 | extraire signatures et tests automatisés |
| configuration | 07, 26, 27 | D3 | documenter schémas NVS/JSON exacts |
| relais | 08 | D3 | cartographie matérielle exacte et tests par topologie |
| Web | 09 | D3 | inventaire automatique des routes du code |
| temps et EventLog | 10 | D3 | formats d’événements et tests de correction d’heure |
| matériel | 12, 28 | D2-D3 | références et brochages extraits du build courant |
| affichage et tactile | 13 | D3 | liens précis vers classes et fonctions |
| SD et ressources | 14, 27 | D3 | manifeste de ressources et tests de fallback |
| modèle V4 et météo | 16 | D2 | stabilisation des frontières et données |
| build et validation | 17, 30 | D3 | automatisation CI et preuves archivées |
| réseau | 18 | D3 | états et timeouts exacts du code |
| HTTPS et sessions | 19 | D2 | implémentation et tests négatifs |
| MQTT | 20 | D2 | topics, QoS et schémas confirmés |
| OTA | 21 | D2 | stratégie de partition et signature validée |
| notifications | 22 | D2 | canaux et politique implémentés |
| sécurité | 23 | D3 | procédures testées et registre mis à jour |
| observabilité | 24 | D3 | page diagnostic et rétention confirmées |
| maintenance | 25 | D3 | exercice de restauration documenté |
| catalogues et index | 28 à 34 | D3-D4 | mise à jour automatique ou revue de checkpoint |

## Règles de progression

- aucun document ne passe à D3 sans invariants et tests ;
- aucun document ne passe à D4 sans permettre une reprise autonome ;
- aucun document ne passe à D5 sans liens vérifiés vers le code, tests exécutés et procédure de maintenance ;
- un changement d’architecture peut faire redescendre temporairement un document ;
- la maturité décrit le document, pas uniquement le composant logiciel.

## Priorités D5

1. relais et sécurités ;
2. configuration et persistance ;
3. Scheduler et Runtime ;
4. Web et interfaces exposées ;
5. OTA et cybersécurité avant exposition distante ;
6. sauvegarde et récupération.
