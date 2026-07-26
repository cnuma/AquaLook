# AquaLook Engineering Reference — Relais et commande des équipements

- Version documentaire : 1.0
- Statut : référence initiale
- Dernière consolidation : 2026-07-27
- Sources : `AGENTS.md`, checkpoints relais, topologie matérielle, code du dépôt
- Composants : `RelaisManager`, XL9535-K2V5, bus I²C
- Maturité : D3

## Mission

La chaîne de commande transforme une demande logique de zone en état physique d’une sortie, tout en préservant les sécurités de durée et l’indépendance entre planification et matériel.

## Plateforme actuelle

- contrôleur d’E/S : XL9535-K2V5 ;
- adresse I²C actuelle : `0x20` ;
- logique directe : `1` correspond à l’état actif ;
- bus I²C : SDA `27`, SCL `22` ;
- nombre de zones actives : 1 à 8.

## Architecture

```mermaid
flowchart LR
  SCH[ScheduleManager] -->|callback / demande| MAIN[Chaîne câblée dans main.cpp]
  MAIN --> REL[RelaisManager]
  REL --> I2C[Bus I²C]
  I2C --> XL[XL9535-K2V5 0x20]
  XL --> OUT[Relais / électrovannes]
```

## Responsabilités

- initialiser les sorties dans un état sûr ;
- convertir une affectation logique en canal physique ;
- activer et désactiver les sorties ;
- conserver un état logiciel cohérent ;
- appliquer la durée maximale de sécurité ;
- remonter les erreurs de communication.

## Responsabilités exclues

- calcul d’occurrence ;
- interprétation météo ;
- décision de démarrage ;
- modification de la configuration persistante.

## Invariants

### INV-REL-001

Le Scheduler ne pilote jamais directement le matériel.

### INV-REL-002

Une modification de logique directe/inverse ou de contrôleur est critique et exige un test matériel.

### INV-REL-003

La sécurité de durée maximale ne doit pas être supprimée.

### INV-REL-004

Au démarrage, les sorties sont placées dans l’état sûr prévu avant toute exécution.

### INV-REL-005

Les tests matériels commencent par une seule zone et une durée courte, sous surveillance.

## Modes dégradés

- périphérique I²C absent : refus de commande pour les sorties concernées ;
- erreur bus : journalisation et tentative de récupération selon le code ;
- configuration de topologie invalide : absence d’activation plutôt qu’activation sur une voie non déterminée.

## Interfaces matérielles

| Interface | Valeur actuelle | Usage |
|---|---:|---|
| I²C SDA | GPIO 27 | Données vers le contrôleur relais |
| I²C SCL | GPIO 22 | Horloge du bus |
| Adresse | `0x20` | XL9535-K2V5 actuel |
| Logique | directe | `1` = ON |

## Tests

- scan I²C et détection à `0x20` ;
- activation d’une seule voie ;
- correspondance zone/voie ;
- arrêt à la durée maximale ;
- redémarrage avec toutes les sorties sûres ;
- modes 1 à 8 zones ;
- récupération après erreur I²C.

Environnement ciblé :

```text
pio run -e test_relais
```

## Références

- `AGENTS.md` — section Relais et sécurité ;
- documents de topologie relais ;
- checkpoints matériels validés ;
- roadmap des extensions I²C et des futurs MCP23017.

## Historique

### 1.0

Première consolidation de la chaîne de commande des équipements.
