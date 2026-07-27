# AquaLook Developer Guide — Ajouter un diagnostic JSON

- Référence : DEV-013
- Statut : actif
- Maturité : D4

## Principe

Un diagnostic décrit un état existant ; il ne devient pas propriétaire de cet état et ne déclenche pas d'action matérielle.

## Étapes

1. identifier le manager propriétaire et la donnée réellement disponible ;
2. choisir une route diagnostique existante ou justifier une nouvelle route ;
3. définir noms, types, unités, valeurs nulles et stabilité du contrat ;
4. sérialiser avec une capacité JSON bornée ;
5. éviter allocations répétées et lectures lentes dans le callback ;
6. exclure mots de passe, clés API, jetons et données permettant leur déduction ;
7. distinguer état, validité, âge et origine de la donnée ;
8. ajouter les cas nominal, indisponible et invalide ;
9. mettre à jour la documentation de route et la traçabilité ;
10. vérifier l'effet mémoire et le temps de réponse.

## Validation

Tester le schéma, les types, les bornes, l'absence de secrets et le comportement lorsque le producteur n'est pas initialisé.

## Références

- `docs/firmware/FW-006_WebManager.md`
- `docs/engineering/09_WEB_AND_HTTP_INTERFACES.md`
- `docs/engineering/24_DIAGNOSTICS_AND_OBSERVABILITY.md`
