# Checkpoint AquaLook — OTA-2.1 — Déclenchement Web du probe GitHub

Date : 2026-07-22

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
7. `MaintenanceBoot` exécute le probe GitHub dans l'état mémoire minimal.
8. La commande est effacée avant exécution.
9. Le module redémarre automatiquement en mode normal.

## Fichier modifié

### `src/WebManager.h`

Position : méthode `registerFaultRoutes()`.

Ajouts :

- inclusion de `MaintenanceRequest.h` ;
- page autonome `GET /ota` ;
- endpoint `POST /api/maintenance/probe-github` ;
- vérification qu'aucune zone n'est active ;
- sauvegarde NVS de `PROBE_GITHUB` ;
- utilisation du mécanisme de redémarrage différé existant.

## Invariants

1. Aucun TLS n'est lancé depuis le runtime complet.
2. Aucune partition OTA n'est écrite.
3. Le test est impossible pendant un arrosage actif.
4. Le redémarrage n'est pas exécuté dans la tâche AsyncTCP.
5. Le runtime nominal reste inchangé sans action utilisateur.
6. `CHECK_VERSION` et `INSTALL_UPDATE` restent non exécutables.

## Validation requise

```powershell
git switch work/remote-ota-github
git pull
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

Après deux compilations `SUCCESS` :

```powershell
pio run -e ProgrammeArrosage -t upload --upload-port COM3
pio device monitor -p COM3 -b 115200
```

Test :

1. vérifier qu'aucune zone n'arrose ;
2. ouvrir `/ota` ;
3. cliquer sur `Tester GitHub maintenant` ;
4. relever les lignes `Maintenance:` ;
5. vérifier le second redémarrage et le retour complet du programmateur.

Test de sécurité complémentaire : activer temporairement une zone, appeler l'API et confirmer la réponse HTTP `409` sans redémarrage.