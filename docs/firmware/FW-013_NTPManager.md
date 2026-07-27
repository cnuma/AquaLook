# AquaLook Firmware — NTPManager

- Référence : FW-013
- Statut : relié au code
- Maturité : D4
- Sources : `src/NTPManager.h`, `src/NTPManager.cpp`

## Mission

`NTPManager` configure l'heure système ESP32, détecte la synchronisation et fournit des représentations temporelles utilisées par le Scheduler, le Web et l'affichage.

## Configuration runtime

`begin(ConfigManager*)` conserve la dépendance puis appelle la configuration NTP. `applyConfig()` lit le serveur, le décalage GMT et le décalage DST depuis `ConfigManager` et appelle `configTime()`. La configuration peut être réappliquée après changement sans imposer de redémarrage.

## Polling

`update()` sonde rapidement avant la première synchronisation, puis espace les contrôles après synchronisation. Les constantes du projet prévoient un poll initial de 500 ms et une resynchronisation nominale d'une heure.

## API

`isSynced()`, `getTimeStr()`, `getHHMM()`, `getHour()`, `getMinute()`, `getWeekday()`, `getDayOfMonth()` et `getEpochDay()` s'appuient sur `fillTm()`. Le jour de semaine suit `tm_wday` : 0 pour dimanche à 6 pour samedi.

## Invariants

- l'absence de NTP ne doit pas bloquer la boucle ;
- une heure non synchronisée doit rester identifiable ;
- les conversions de jour utilisées par le Scheduler doivent être explicites ;
- les paramètres runtime restent propriétaires de `ConfigManager`.

## Validation

Tester démarrage sans réseau, synchronisation tardive, changement serveur/fuseau, passage jour/mois et comportement du Scheduler avant et après synchronisation.

## Références

- `docs/engineering/10_NTP_AND_EVENTLOG.md`
- `docs/firmware/FW-002_Scheduler.md`
- `docs/firmware/FW-003_ConfigManager.md`
