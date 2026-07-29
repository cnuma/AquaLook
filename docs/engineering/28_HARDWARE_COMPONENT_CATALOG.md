# AquaLook Engineering Reference — Catalogue matériel

- Version documentaire : 1.0
- Statut : catalogue initial
- Dernière consolidation : 2026-07-27
- Sources : checkpoints matériels et documents 12 à 17
- Maturité : D2

## Objet

Ce catalogue fournit un point d’entrée unique vers les composants matériels. Les références exactes, variantes et brochages doivent toujours être vérifiés dans le code et la configuration de compilation du commit ciblé.

## Plateforme principale

| Composant | Rôle | Interface | État |
|---|---|---|---|
| ESP32 CYD | calcul, réseau et orchestration | interne | plateforme validée |
| TFT 320 × 240 | affichage local | SPI | validé |
| XPT2046 | tactile | VSPI séparé | validé |
| lecteur microSD | ressources et stockage optionnel | SPI | validé avec fallback |

## Commande des équipements

| Composant | Rôle | Interface | État |
|---|---|---|---|
| contrôleur relais historique | activation des sorties | bus matériel du projet | validé |
| XL9535 | extension E/S des cartes relais historiques | I²C | présent selon matériel |
| MCP23017 | extension E/S étudiée | I²C | évolution référencée |
| électrovannes | actionneurs d’arrosage | relais | validées |
| pompe | équipement partagé | modèle shadow / relais selon configuration | transitoire |

## Réseau et temps

| Composant | Rôle | État |
|---|---|---|
| Wi-Fi ESP32 | réseau local | validé |
| NTP distant | heure absolue | validé, non indispensable au boot |

## Fiche obligatoire d’un composant

Toute nouvelle fiche contient :

- référence fabricant et variante exacte ;
- fonction dans AquaLook ;
- tension et consommation ;
- interfaces, adresses et broches ;
- bibliothèque ou driver ;
- séquence d’initialisation ;
- état sûr au boot ;
- modes dégradés ;
- tests matériels ;
- risques et substitutions compatibles.

## Invariants

### INV-CAT-HW-001

Une référence générique ne remplace pas l’identification de la variante réellement montée.

### INV-CAT-HW-002

Tout changement de contrôleur, logique directe/inverse ou brochage est critique.

### INV-CAT-HW-003

Une validation matérielle est distincte d’une compilation réussie.

## Références

- `12_HARDWARE_PLATFORM.md` ;
- `13_DISPLAY_AND_TOUCH.md` ;
- `08_RELAY_AND_EQUIPMENT_CONTROL.md` ;
- `16_V4_EQUIPMENT_MODEL_AND_WEATHER.md`.
