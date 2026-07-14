# CHECKPOINT — AquaLook — horodatage des logs après synchronisation NTP

Date : 14 juillet 2026

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche dédiée : `dev/log-timestamps-ntp`
- Base de création : `work/step7-run7-2` au commit `ebcfcd40a45c0324d7d0c03f27fce781e00a4998`

## Objectif

Conserver le temps écoulé depuis le boot tant que l'horloge système n'est pas valide, puis afficher automatiquement l'heure locale réelle dans les logs dès que la synchronisation NTP a réglé l'horloge ESP32.

## Modification

Fichier modifié :

- `src/EventLog.h`

Évolutions :

- ajout de l'époque réelle dans chaque `LogEntry` ;
- détection d'une horloge valide à partir du 1er janvier 2024 ;
- affichage série au format `HH:MM:SS.mmm` après synchronisation ;
- maintien du format uptime `HH:MM:SS.mmm` avant synchronisation ;
- ajout de `formatEntryTimestamp()` pour permettre aux interfaces de présenter le même horodatage ;
- aucune dépendance directe vers `NTPManager` : `EventLog` lit l'horloge système réglée par `configTime()`.

## Comportement attendu

Avant NTP :

```text
[00:00:07.329] [INF] NTP: synchronise 14/07/2026 08:25:54
```

Après ce message, les logs suivants doivent utiliser l'heure locale :

```text
[08:25:57.507] [WRN] Timing: ...
```

Le basculement est automatique et ne nécessite aucun appel depuis `main.cpp`.

## Invariants préservés

1. Aucun changement de `NTPManager`.
2. Aucun changement de `main.cpp`.
3. Aucun changement du fonctionnement des relais, du planning, du Web ou de la SD.
4. Les logs restent disponibles avant la synchronisation NTP.
5. Les millisecondes restent issues de `millis()` pour conserver une précision fine.

## Validation requise

Cette modification touche le journal utilisé par tout le firmware. Validation complète requise :

```powershell
git fetch --prune
git switch dev/log-timestamps-ntp
git pull --ff-only

pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Vérifier :

- logs uptime avant synchronisation ;
- message de synchronisation NTP ;
- heure locale réelle sur les messages suivants ;
- absence de régression fonctionnelle.

## Intégration future

Après validation, ce commit devra être intégré dans la branche Phase 7 active par cherry-pick ou fusion contrôlée, sans mélanger l'historique des deux chantiers.