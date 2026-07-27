# AquaLook Developer Guide — Modifier la boucle Runtime sans la bloquer

- Référence : DEV-015
- Statut : actif
- Maturité : D4

## Principe

La boucle coordonne des services courts. Toute opération réseau, stockage ou calcul potentiellement longue doit être découpée, bornée ou déplacée dans une tâche contrôlée.

## Étapes

1. identifier le point d'appel réel dans `setup()` ou `loop()` ;
2. définir cadence, budget temporel et état conservé entre deux appels ;
3. remplacer les attentes par une machine d'états ou une tâche unique ;
4. fixer timeout, retry et backoff ;
5. interdire les écritures Flash répétées dans la boucle rapide ;
6. instrumenter la section avec `RuntimeProfiler` lorsque pertinent ;
7. rendre l'état et les erreurs observables ;
8. tester sans Wi-Fi, sans SD et pendant un arrosage actif ;
9. vérifier heap, plus grand bloc et overruns ;
10. documenter l'ordre exact conservé dans `main.cpp`.

## Critères de refus

Pas de `delay()` long, de boucle d'attente réseau, de retry agressif ni de traitement qui retarde l'arrêt de sécurité des relais.

## Références

- `docs/firmware/FW-001_main.md`
- `docs/firmware/FW-004_Runtime.md`
- `docs/engineering/15_RUNTIME_AND_PROFILING.md`
- `docs/engineering/24_DIAGNOSTICS_AND_OBSERVABILITY.md`
