# AquaLook Firmware — Runtime et profiler

- Référence : FW-004
- Statut : relié au code
- Maturité : D4
- Sources : `src/main.cpp`, `src/RuntimeProfiler.*`, `src/SystemDiagnostics.*`

## Mission

Le Runtime coordonne les managers sans bloquer la boucle Arduino. `RuntimeProfiler` mesure le temps mural des segments critiques et alimente les diagnostics.

## Principes

- chaque service avance par petites étapes ;
- aucune dépendance à Internet pour l’arrosage local ;
- les écritures persistantes et redémarrages sont différés hors AsyncTCP ;
- les traitements réseau longs sont bornés ou déportés ;
- les modes dégradés isolent le service fautif sans arrêter les sécurités locales.

## Cycle

```text
loopEnter
  -> mises à jour réseau/temps/services
  -> Scheduler et sorties
  -> Web et actions différées
  -> affichage
  -> diagnostics
loopExit
```

L’ordre exact reste celui de `src/main.cpp` au commit ciblé.

## Observabilité

`RuntimeProfiler` collecte les durées des sections instrumentées. `SystemDiagnostics` expose les groupes `system`, `build`, `memory`, `loop`, `web` et `wifi`. Le seuil d’overrun courant est de 100 ms, avec limitation de journalisation.

## Invariants

- pas de `delay()` long dans le chemin nominal ;
- pas d’attente réseau infinie ;
- pas d’écriture Flash dans la boucle rapide ;
- le watchdog ne remplace pas une architecture non bloquante ;
- un service distant défaillant ne bloque pas les relais ni l’arrêt de sécurité.

## Tests requis

- fonctionnement sans Wi-Fi ;
- météo indisponible ;
- SD absente ;
- charge Web répétée ;
- arrosage actif pendant les services réseau ;
- suivi du heap et du plus grand bloc ;
- absence d’overruns répétés.

## Références

- `docs/engineering/15_RUNTIME_AND_PROFILING.md`
- `docs/engineering/24_DIAGNOSTICS_AND_OBSERVABILITY.md`
- `docs/firmware/FW-001_main.md`
