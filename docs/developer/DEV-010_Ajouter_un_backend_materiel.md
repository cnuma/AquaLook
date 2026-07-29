# AquaLook Developer Guide — Ajouter un backend matériel

- Référence : DEV-010
- Statut : actif
- Maturité : D4

## Principe

Un backend traduit une action logique validée en opération électrique. Il ne contient ni calendrier, ni décision météo, ni politique métier.

## Étapes

1. définir l’interface physique, les broches, adresses et niveaux actifs ;
2. réserver un propriétaire unique du bus ou périphérique ;
3. implémenter une API minimale et bornée ;
4. garantir un état sûr au boot et en cas d’erreur ;
5. intégrer le backend derrière l’adaptateur Runtime ;
6. ajouter la sélection de compilation sans casser legacy/V4 ;
7. valider les timeouts et absences de périphérique ;
8. créer un banc ciblé ;
9. mesurer les signaux et sorties sur matériel ;
10. documenter topologie, limites et procédure de récupération.

## Invariants

- aucune sortie intempestive au démarrage ;
- configuration invalide refusée avant accès matériel ;
- logique directe/inversée explicite ;
- durée maximale conservée ;
- défaut du bus observable ;
- test matériel obligatoire avant validation.

## Références

- `docs/firmware/FW-010_EquipmentManager.md`
- `docs/engineering/08_RELAY_AND_EQUIPMENT_CONTROL.md`
- `docs/engineering/17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md`
