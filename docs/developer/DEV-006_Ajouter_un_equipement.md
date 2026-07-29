# AquaLook Developer Guide — Ajouter un équipement

- Référence : DEV-006
- Statut : actif
- Maturité : D4

## Étapes

1. définir le besoin fonctionnel et le propriétaire de l’état ;
2. vérifier si un type existant convient ;
3. compléter le modèle dans `EquipmentModel` sans introduire d’adresse matérielle dans la zone ;
4. ajouter les validations de type, index, rôle et dépendances ;
5. adapter `EquipmentManager` pour construire le plan ;
6. implémenter l’action dans un backend ou adaptateur séparé ;
7. définir les comportements dry-run, erreur et mode dégradé ;
8. ajouter les tests déterministes ;
9. compiler legacy et V4 ;
10. mettre à jour Engineering, Firmware et checkpoint.

## Règles

- aucune sortie ne s’active avec une configuration invalide ;
- une dépendance absente produit un résultat explicite ;
- conserver une limite claire du nombre d’étapes ;
- ne pas contourner les sécurités de durée ou de relais ;
- toute action électrique exige une preuve matérielle.

## Validation

Utiliser `test_execution_engine`, puis valider le backend concerné sur cible. Tester doublons, index hors bornes, équipement désactivé, dépendance absente et rôle relais incorrect.

## Références

- `docs/firmware/FW-010_EquipmentManager.md`
- `docs/engineering/36_DETAILED_EQUIPMENT_MODEL_SCHEMA.md`
