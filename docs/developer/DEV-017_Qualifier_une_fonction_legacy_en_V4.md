# AquaLook Developer Guide — Qualifier une fonction legacy en V4

- Référence : DEV-017
- Statut : actif
- Maturité : D4

## Méthode

1. identifier le déclencheur legacy, le callback, le backend et l'effet matériel ;
2. tracer le chemin V4 équivalent depuis `main.cpp` ;
3. vérifier l'instanciation, `begin()`, les dépendances et l'appel réel ;
4. exécuter d'abord le mode shadow et comparer les transitions ;
5. activer le backend V4 uniquement pour un périmètre borné ;
6. compiler legacy et V4 ;
7. flasher V4 et tester une zone, durée courte, sous surveillance ;
8. consigner profil, port COM, sortie mesurée, résultat et écarts ;
9. conserver legacy tant que la campagne n'est pas complète ;
10. mettre à jour Engineering, Firmware et checkpoint.

## Critère de qualification

Une compilation, un test unitaire ou une trace shadow ne suffit pas. La fonction est qualifiée V4 seulement si le chemin V4 réellement exécuté produit l'effet matériel attendu sans régression collatérale.

## Références

- `AGENTS.md`
- `docs/firmware/FW-015_V4PilotRuntime.md`
- `docs/engineering/17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md`
