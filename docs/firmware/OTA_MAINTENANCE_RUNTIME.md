# AquaLook — Fonctionnement firmware du mode maintenance OTA

## 1. Périmètre

Ce document décrit le fonctionnement réellement implémenté et validé jusqu'à OTA-2.2.

Il complète l'architecture générale décrite dans `docs/engineering/OTA_REMOTE_UPDATE_ARCHITECTURE.md`.

## 2. Déclenchement depuis le runtime normal

La route `POST /api/maintenance/probe-github`, déclarée dans `WebManager::registerFaultRoutes()`, exécute la séquence suivante :

1. vérifie que les pointeurs du runtime sont prêts ;
2. parcourt toutes les zones configurées ;
3. lit l'état de sortie effectif via l'adaptateur runtime lorsqu'il est disponible ;
4. refuse la commande si une zone est active ;
5. enregistre `MaintenanceRequest::PROBE_GITHUB` dans la NVS ;
6. programme un redémarrage différé de 750 ms ;
7. répond au navigateur avant le redémarrage.

Réponses principales :

- `202` : commande acceptée ;
- `409 watering-active` : arrosage actif ;
- `503 runtime-not-ready` : runtime non prêt ;
- `500 nvs-write-failed` : échec d'enregistrement.

## 3. Entrée avant le setup nominal

`MaintenanceSetupWrapper.cpp` détecte la commande avant l'exécution du `setup()` nominal.

Le mode maintenance utilise :

- une tâche FreeRTOS dédiée ;
- une pile de 16 384 octets ;
- le cœur 0 ;
- la suspension du chemin nominal pendant l'opération.

Si la tâche ne peut pas être créée, le firmware revient au démarrage nominal afin de ne pas immobiliser le programmateur.

## 4. Exécution du probe

`MaintenanceBoot::runIfRequested()` :

1. charge la commande ;
2. l'efface avant exécution ;
3. refuse toute commande non implémentée ;
4. charge la configuration Wi-Fi ;
5. connecte le Wi-Fi avec timeout ;
6. ouvre un `WiFiClientSecure` uniquement en maintenance ;
7. mesure la durée TLS ;
8. envoie une requête HTTP GitHub ;
9. lit la première ligne HTTP ;
10. ferme le client ;
11. persiste le résultat ;
12. redémarre vers le runtime normal.

Le probe actuel considère comme succès la réception d'une ligne commençant par `HTTP/`. Le `404 Not Found` est donc acceptable pour ce test de transport.

## 5. Persistance du résultat

Le résultat est stocké dans `aq_maint_res` par `MaintenanceResultStore`.

Champs actuels :

| Clé NVS | Signification |
|---|---|
| `valid` | présence d'un résultat exploitable |
| `success` | succès ou échec |
| `tls_ms` | durée TLS |
| `uptime_ms` | uptime au moment de l'enregistrement |
| `heap_min` | heap minimale observée |
| `command` | commande exécutée |
| `http` | ligne HTTP reçue |
| `detail` | motif d'échec ou détail complémentaire |

Les chaînes sont bornées par les tableaux du modèle `MaintenanceResult`.

## 6. Restitution Web

`GET /api/maintenance/last-result` renvoie un JSON sans cache.

La page `/ota` charge ce résultat et affiche :

- état ;
- ligne HTTP ;
- durée TLS ;
- uptime maintenance ;
- heap minimale ;
- détail éventuel.

Pendant un nouveau test, l'ancien résultat peut rester visible jusqu'au retour du module. Après rechargement, le nouveau résultat doit correspondre au log série.

## 7. Validation matérielle OTA-2.2

Environnement réellement chargé :

```text
ProgrammeArrosage_v4
```

Résultat :

```text
TLS : 1596 ms
HTTP : HTTP/1.1 404 Not Found
Heap minimale : 144172 octets
Persistance : réussie
Retour runtime : réussi
app1 : non écrite
```

Le refus pendant un arrosage actif a également été validé sans redémarrage.

## 8. Éléments connus hors périmètre

Les messages suivants ne sont pas causés par OTA-2.2 :

- absence de partition core dump ;
- callback APB dupliqué ;
- alertes de boucles lentes ;
- premier essai météo parfois en échec puis récupération.

Ils doivent rester traités dans leurs chantiers respectifs.
