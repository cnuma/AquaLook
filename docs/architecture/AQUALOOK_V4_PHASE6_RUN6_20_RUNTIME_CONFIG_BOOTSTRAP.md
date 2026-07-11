# AquaLook V4 — Phase 6 — Run 6.20

## Objet

Brancher `EquipmentRuntimeConfigStore` au démarrage du firmware et utiliser la configuration persistée pour construire le scénario pompe shadow.

## Comportement au démarrage

1. `ConfigManager` charge la configuration historique.
2. `EquipmentRuntimeConfigStore` charge ou crée la clé NVS `equipCfg`.
3. Le mode, l’activation, l’index logique et les temporisations de pompe sont journalisés.
4. Le scénario pompe shadow n’est construit que si la pompe est activée et que son mode n’est pas `MODE_DISABLED`.

## Modes effectifs

- `MODE_DISABLED` : aucun scénario pompe ; les plans observés restent limités aux vannes.
- `MODE_SHADOW` : scénario pompe passif construit avec les délais persistés.
- `MODE_PHYSICAL` : demande journalisée, mais provisoirement rabattue vers le shadow ; aucune commande de pompe réelle n’est autorisée dans ce run.

## Invariants préservés

- aucune modification de la topologie réelle ;
- aucune affectation synthétique transmise à `RelaisManager` ;
- aucune commande physique de pompe ;
- chemin fonctionnel des vannes inchangé ;
- configuration absente ou invalide => pompe désactivée ;
- NVS historique `config` inchangée.

## Validation attendue au premier démarrage

La clé `equipCfg` étant absente, le store crée les valeurs sûres :

```text
Equipment config runtime: status=safe_defaults_created enabled=no mode=disabled assignment=255 delays=500/500
Shadow pump: desactive par configuration NVS
```

Un START manuel doit alors produire un plan shadow vanne uniquement :

```text
Shadow: zone 1 START accepted=yes steps=1 pump=no
```

Le relais réel de la vanne doit continuer à fonctionner normalement.
