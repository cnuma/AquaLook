# ADR-0002 — Capacités maximales initiales du domaine V4

- **Statut :** Acceptée pour la Phase 1, révisable après mesures réelles
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.1

## Contexte

AquaLook fonctionne sur ESP32 et doit utiliser des tableaux bornés. Les limites historiques sont centrées sur les zones et relais :

```text
MAX_ZONES = 16
MAX_ACTIVE_ZONES = 8
MAX_RELAY_ASSIGNMENTS = 16
```

La V4 doit représenter des cartes génériques, ports, équipements, capteurs, automatismes, dépendances et exécutions.

## Décision

Les plafonds initiaux suivants sont retenus pour le modèle isolé de Phase 1 :

```text
MAX_ZONES_V4            = 16
MAX_EQUIPMENTS_V4       = 32
MAX_SENSORS_V4          = 32
MAX_AUTOMATIONS_V4      = 32
MAX_DEPENDENCIES_V4     = 64
MAX_ACTIVE_EXECUTIONS   = 16
MAX_HARDWARE_BOARDS     = 8
MAX_PORTS_PER_BOARD     = 16
MAX_PORT_BINDINGS       = 64
```

Ces limites sont des capacités de représentation, pas des promesses de simultanéité.

## Justification

### Zones : 16

Conserve la capacité interne historique et permet une migration sans réduction.

### Équipements : 32

Permet 16 vannes plus pompes, vanne maîtresse, éclairages, ventilations et auxiliaires sans dépendre du nombre de zones.

### Capteurs : 32

Permet plusieurs mesures par zone et des capteurs globaux : débit, pression, niveau, pluie, humidité du sol, température.

### Automatismes : 32

Couvre les programmes d’arrosage et des automatismes climatiques futurs sans créer une capacité excessive.

### Dépendances : 64

Autorise en moyenne deux relations par équipement, avec marge pour pompe partagée et exclusions.

### Exécutions simultanées : 16

Plafond de suivi runtime. La politique de simultanéité réelle sera probablement bien inférieure et définie séparément.

### Cartes : 8

Correspond à une capacité raisonnable d’inventaire local et aux huit adresses courantes permises par trois bits d’adressage, sans supposer que toutes les cartes utilisent I²C.

### Ports par carte : 16

Couvre les expandeurs 16 bits comme XL9535 et MCP23017 et les cartes plus petites de 1, 2, 4 ou 8 ports.

### Affectations de ports : 64

Permet de lier des sorties, entrées, compteurs et signaux analogiques sans limiter le système au nombre d’équipements.

## Règles

1. Une carte peut déclarer moins de ports que `MAX_PORTS_PER_BOARD`.
2. Un port non exposé physiquement n’est pas déclaré comme utilisable.
3. Les limites ne doivent pas conduire à réserver des structures lourdes pour chaque emplacement.
4. Les descripteurs de modèles de cartes doivent être partagés ou placés en mémoire programme lorsque possible.
5. Les structures runtime doivent rester compactes et sans `String` durable.
6. Toute hausse d’une limite exige une mesure RAM et flash.

## Options rejetées

### 64 équipements parce que 8 cartes × 8 voies

Rejeté : la capacité physique théorique ne doit pas dicter directement le nombre d’objets métier.

### Une capacité dynamique sans plafond

Rejeté : imprévisible sur microcontrôleur et incompatible avec les exigences de sûreté.

### Conserver `MAX_RELAY_ASSIGNMENTS = MAX_ZONES`

Rejeté : les affectations concernent aussi pompes, entrées, capteurs et auxiliaires.

## Conséquences

- les noms de constantes deviennent génériques ;
- `RelayTopology` devient un adaptateur transitoire spécialisé ;
- le futur inventaire matériel accepte jusqu’à 8 cartes de 16 ports ;
- la capacité physique maximale descriptible est 128 ports, mais seulement 64 bindings actifs sont retenus initialement ;
- les tailles devront être vérifiées à chaque run d’introduction de structure.

## Condition de révision

Cette ADR devra être revue si le budget fixe du domaine V4 dépasse 12 Kio de RAM, hors buffers réseau, affichage et historique.
