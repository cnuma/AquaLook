# AquaLook Developer Guide — Ajouter un driver binaire V4

- Référence : DEV-020
- Statut : actif
- Maturité : D4

## Étapes

1. décrire le contrôleur, la carte, les ports et les niveaux actifs ;
2. implémenter le contexte du driver sans logique métier ;
3. définir les opérations bornées de lecture/écriture nécessaires ;
4. enregistrer le binding dans `BinaryActuatorDriverRegistry` ;
5. relier inventaire, port et driver dans `V4PilotRuntime` ;
6. refuser contrôleur, port ou rôle incompatibles ;
7. préserver un état sûr au boot et sur perte du bus ;
8. ajouter les tests déterministes du registre et du backend ;
9. valider sur un canal réel avant d'élargir le périmètre ;
10. conserver le backend legacy durant la qualification.

## Invariants

- aucune adresse matérielle cachée dans la logique métier ;
- capacité de registre bornée ;
- absence de driver traitée comme erreur ;
- aucune activation implicite lors de l'enregistrement ;
- preuve matérielle obligatoire.

## Références

- `docs/firmware/FW-015_V4PilotRuntime.md`
- `docs/developer/DEV-010_Ajouter_un_backend_materiel.md`
- `docs/engineering/08_RELAY_AND_EQUIPMENT_CONTROL.md`
