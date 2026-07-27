# AquaLook Firmware — EventLog

- Référence : FW-005
- Statut : relié au code
- Maturité : D4
- Sources : `src/EventLog.h`, `src/EventLog.cpp`, `src/FaultManager.*`, `src/WebManager.*`

## Mission

`EventLog` centralise les événements utiles au diagnostic local. Il alimente les routes de consultation et complète `FaultManager` sans se substituer à lui.

## Implémentation confirmée

- buffer circulaire de 60 entrées ;
- message borné à 72 caractères ;
- niveaux d’événement ;
- horodatage actuel basé sur `millis()` ;
- exposition Web via `/api/logs` et pages associées ;
- acquittement et défauts gérés par les composants dédiés.

## Point important

Le code courant ne remplace pas automatiquement l’horodatage `millis()` par l’heure NTP absolue. Toute évolution vers un double horodatage doit préserver l’ordre des événements avant synchronisation et gérer les corrections d’heure.

## Invariants

- aucun mot de passe, clé API, jeton ou certificat privé dans les logs ;
- messages bornés ;
- journalisation non bloquante ;
- limitation des événements répétitifs ;
- une erreur critique active aussi le mécanisme de défaut approprié ;
- le journal ne constitue pas à lui seul une preuve persistante après reboot.

## Risques ouverts

- protection de concurrence du buffer à renforcer ou démontrer ;
- rétention limitée et volatile ;
- risque de saturation par répétition ;
- absence d’heure absolue dans les entrées actuelles.

## Points d’extension

- ajouter un événement : catégorie, niveau, données minimales et absence de secret ;
- ajouter une persistance : quota, rotation, usure et comportement SD absente ;
- ajouter une télémétrie distante : filtrage, confidentialité, reprise et déduplication.

## Tests requis

- rotation du buffer ;
- chaînes longues ;
- appels concurrents ;
- endpoints JSON ;
- répétition d’erreurs ;
- audit de secrets ;
- fonctionnement avant et après NTP.

## Références

- `docs/engineering/10_TIME_AND_EVENTLOG.md`
- `docs/engineering/23_SECURITY_OPERATIONS.md`
- `docs/engineering/24_DIAGNOSTICS_AND_OBSERVABILITY.md`
