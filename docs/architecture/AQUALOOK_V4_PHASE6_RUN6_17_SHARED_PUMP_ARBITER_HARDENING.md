# AquaLook V4 — Phase 6 — Run 6.17

## Objet

Durcir l’arbitre de pompe partagée avant toute connexion matérielle.

## Base

- branche : `feature/aqualook-v4-domain`
- base : `15d9dbffed28261ef2da39d45a5a2bbdc410d9ad`
- Run 6.16 : arbitrage partagé validé sur deux zones

## Cas traités

- START répété sur une zone déjà acquise : compteur inchangé, aucun second `PUMP_ON` ;
- STOP sans START préalable : compteur inchangé, aucun `PUMP_OFF` ;
- remplacement d’un plan actif : annulation propre du moteur de zone ;
- échec de chargement d’un plan : restauration de l’état d’acquisition précédent ;
- contrôle permanent entre le compteur global et les drapeaux par zone ;
- réparation automatique d’une incohérence détectée ;
- remise à zéro globale via `emergencyStopAll()`.

## Nouveaux diagnostics

```text
transition=DUPLICATE_START
transition=ORPHAN_STOP
consistent=yes
Shadow pump arbiter: EMERGENCY_STOP users=0 consistent=yes passive=yes
```

## Invariant de sécurité

Le composant reste totalement passif : aucune commande de pompe, aucun accès aux relais, aucun accès au backend physique et aucune modification de la persistance.

## Validation attendue

1. Compiler V4 et legacy.
2. Démarrer deux fois la même zone sans STOP intermédiaire et vérifier que `users` reste à 1.
3. Arrêter deux fois la même zone et vérifier que le second STOP annonce `ORPHAN_STOP`, sans `PUMP_OFF`.
4. Vérifier que toutes les lignes de synthèse annoncent `consistent=yes`.
