# Checkpoint AquaLook — OTA-2.1 — Déclenchement Web du probe GitHub

Date : 2026-07-23

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/remote-ota-github`
- Étape précédente : OTA-2.0, démarrage nominal validé avec wrapper `setup()`.

## Objectif

Permettre à l'utilisateur de déclencher immédiatement un test HTTPS GitHub depuis une interface Web, sans attendre une fenêtre horaire et sans lancer TLS dans le runtime complet.

## Interface ajoutée

URL locale :

```text
http://<adresse-aqualook>/ota
```

La page propose le bouton :

```text
Tester GitHub maintenant
```

Elle rappelle explicitement que :

- AquaLook va redémarrer ;
- le test s'exécute en mode maintenance minimal ;
- aucune partition OTA n'est écrite ;
- le module revient automatiquement au fonctionnement normal.

## API ajoutée

```text
POST /api/maintenance/probe-github
```

### Réponse acceptée

```json
{"ok":true,"restart":true,"command":"probe_github"}
```

Code HTTP : `202`.

### Refus possibles

- `409 watering-active` : au moins une zone est active ;
- `503 runtime-not-ready` : pointeurs du runtime non initialisés ;
- `500 nvs-write-failed` : échec de l'enregistrement de la commande.

## Séquence de fonctionnement

1. La page demande une confirmation utilisateur.
2. L'API vérifie toutes les zones actives via l'état de sortie effectif.
3. Si aucune zone n'arrose, elle enregistre `PROBE_GITHUB` dans `aq_maint/request`.
4. Elle programme un redémarrage différé de 750 ms.
5. `WebManager::update()` réalise le redémarrage hors callback AsyncTCP.
6. Au boot suivant, le wrapper `setup()` détecte la commande.
7. Une tâche FreeRTOS dédiée de 16 384 octets de pile exécute `MaintenanceBoot` sur le cœur 0.
8. `MaintenanceBoot` exécute le probe GitHub dans l'état mémoire minimal.
9. La commande est effacée avant exécution.
10. Le module redémarre automatiquement en mode normal.

## Fichiers modifiés

### `src/WebManager.h`

Position : méthode `registerFaultRoutes()`.

Ajouts :

- inclusion de `MaintenanceRequest.h` ;
- page autonome `GET /ota` ;
- endpoint `POST /api/maintenance/probe-github` ;
- vérification qu'aucune zone n'est active ;
- sauvegarde NVS de `PROBE_GITHUB` ;
- utilisation du mécanisme de redémarrage différé existant.

### `src/MaintenanceSetupWrapper.cpp`

Position : wrapper du symbole `setup()`.

Ajouts validés :

- création d'une tâche FreeRTOS dédiée ;
- pile de 16 384 octets ;
- exécution sur le cœur 0 ;
- suspension du `loopTask` pendant la maintenance ;
- retour sûr au démarrage nominal si la tâche ne peut pas être créée.

## Invariants

1. Aucun TLS n'est lancé depuis le runtime complet.
2. Aucune partition OTA n'est écrite.
3. Le test est impossible pendant un arrosage actif.
4. Le redémarrage n'est pas exécuté dans la tâche AsyncTCP.
5. Le runtime nominal reste inchangé sans action utilisateur.
6. `CHECK_VERSION` et `INSTALL_UPDATE` restent non exécutables.
7. La demande NVS est effacée avant le probe pour éviter toute boucle de redémarrage.

## Validation matérielle — 23 juillet 2026

Le parcours complet a été validé sur le module réel via COM3.

### Déclenchement Web

```text
Maintenance Web: probe GitHub demande, redemarrage programme
```

Le premier redémarrage logiciel a été correctement déclenché.

### Entrée en maintenance

```text
Maintenance: entree sure avant setup type=probe_github
Maintenance: demande detectee type=probe_github
Maintenance: mode minimal actif command=probe_github otaWrite=no
```

### Mémoire avant TLS

```text
heapFree=193128
heapMin=192636
```

La heap libre au début du mode maintenance était d'environ 246 Ko avant connexion Wi-Fi, puis 193 Ko juste avant TLS.

### Connexion GitHub

```text
Maintenance: GitHub TLS connected=yes durationMs=2234
Maintenance: GitHub status=HTTP/1.1 404 Not Found
Maintenance: resultat command=probe_github success=yes otaWrite=no
```

Le code HTTP `404` est accepté pour ce test de transport : il démontre que la négociation TLS, l'envoi de la requête HTTP et la réception d'une réponse GitHub ont réussi.

### Mémoire après TLS

```text
heapFree=152344
heapMin=143668
```

Après fermeture de la connexion :

```text
heapFree=192996
```

La consommation TLS temporaire est d'environ 40,8 Ko et la heap revient presque à son niveau précédent après fermeture.

### Retour au fonctionnement normal

Le second redémarrage a restauré AquaLook normalement :

- partitions OTA conformes ;
- I2C `0x20` détecté ;
- SD montée ;
- relais initialisés ;
- planificateur actif ;
- écran et tactile initialisés ;
- serveur Web actif ;
- Wi-Fi et NTP reconnectés ;
- météo relancée.

## Décision

OTA-2.1 est validé de bout en bout.

La base permet désormais de déclencher manuellement depuis le Web une opération de maintenance minimale, de joindre GitHub en HTTPS, puis de revenir automatiquement au programmateur normal sans intervention physique.

## Points indépendants observés

Ces éléments ne remettent pas en cause OTA-2.1 :

- absence de partition core dump, déjà connue ;
- callback APB SPI dupliqué, déjà observé ;
- requêtes navigateur vers `favicon.ico` absentes de LittleFS ;
- quelques boucles lentes et une lecture météo JSON échouée après le retour au runtime nominal.

Ils doivent rester suivis séparément et ne doivent pas être mélangés au moteur OTA.

## Suite

OTA-2.2 doit ajouter :

1. la persistance du résultat du dernier probe ;
2. son affichage sur `/ota` après retour au mode normal ;
3. une commande `CHECK_VERSION` fondée sur un manifeste GitHub défini et versionné ;
4. toujours aucune écriture dans `app1` avant validation complète du manifeste et du téléchargement en flux.
