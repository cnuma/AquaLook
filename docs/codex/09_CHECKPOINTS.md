# 09 — Checkpoints et état de référence

## Checkpoint courant

```text
AquaLook_2026-06-27_main_checkpoint_complet_a2cf490.zip
```

## Commit

```text
a2cf490aa446c7006557f8df62e1f995f6767359
```

Message : `Fusionne la refonte du parametrage`.

## Contenu fonctionnel

- migration de la configuration vers NVS ;
- LittleFS réservé aux ressources ;
- 1 à 8 zones fonctionnelles ;
- contrôleurs XL9535 et MCP23017 exposés ;
- planning jours fixes et intervalle ;
- météo et seuil de pluie ;
- interface Web ;
- écran LCD ;
- paramètres utilisateur/admin ;
- manuel utilisateur professionnel.

## Refonte du paramétrage incluse

- titre Paramètres ;
- bouton admin verrouillé/déverrouillé ;
- clé session `aqualook-admin-unlocked` ;
- couleurs déplacées dans Zones ;
- réglages relais masqués hors admin ;
- système replié ;
- optimisation de taille de `index.html`.

## Validation connue

- compilation firmware réussie ;
- buildfs réussi ;
- état Git propre lors de la création ;
- checkpoint vérifié ;
- manuel DOCX réparé et ouvert avec succès.

## Règle de reprise

1. cloner le dépôt ;
2. vérifier le commit ;
3. lire AGENTS.md et docs/codex ;
4. compiler ;
5. construire LittleFS ;
6. ne modifier qu’après validation de la base.

## Nouveau checkpoint

Créer un nouveau checkpoint après fusion d’une feature majeure, évolution de la persistance, modification matérielle, refonte Web/LCD, correction critique ou mise à jour significative du socle Codex.
