# AquaLook Developer Guide — Ajouter un scénario shadow V4

- Référence : DEV-018
- Statut : actif
- Maturité : D4

## Objectif

Observer un nouveau plan V4 en parallèle du chemin actif sans donner au scénario shadow un accès au matériel.

## Étapes

1. construire le modèle et la topologie à partir des données réelles ;
2. réserver des indices libres sans modifier les affectations actives ;
3. créer un plan déterministe et validé ;
4. soumettre le plan au slot de zone concerné ;
5. faire progresser le moteur depuis la boucle non bloquante ;
6. journaliser transitions, étapes, pompe partagée et incohérences ;
7. comparer chaque transition au comportement legacy ;
8. tester recouvrement de zones et arrêt d'urgence ;
9. maintenir une frontière stricte sans backend physique ;
10. documenter les écarts avant toute activation réelle.

## Interdictions

- écrire sur I2C depuis le shadow ;
- réutiliser une affectation active de façon ambiguë ;
- présenter une concordance de traces comme validation matérielle.

## Références

- `docs/firmware/FW-016_EquipmentExecutionEngine_et_ShadowRuntime.md`
- `docs/developer/DEV-017_Qualifier_une_fonction_legacy_en_V4.md`
