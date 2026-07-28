# AquaLook — Architecture de mise à jour distante OTA

## Statut

- Niveau documentaire visé : D4
- Branche de consolidation initiale : `work/remote-ota-github`
- Jalons couverts : OTA-2.0 à OTA-2.2
- Firmware matériel de référence : `ProgrammeArrosage_v4`

## 1. Finalité

Le sous-système OTA permet à AquaLook de préparer une mise à jour distante sans compromettre la fonction critique d'arrosage.

L'architecture sépare strictement :

- le runtime complet, qui exécute l'arrosage, l'écran, le Web et les services courants ;
- le mode maintenance minimal, seul autorisé à établir une connexion TLS GitHub ;
- la future installation OTA, encore désactivée tant que le manifeste, le téléchargement en flux, le SHA-256 et le rollback ne sont pas validés.

## 2. Architecture validée

```text
Navigateur /ota
      |
      | POST commande maintenance
      v
WebManager
      |
      | contrôle arrosage inactif
      | persistance commande NVS
      v
Redémarrage différé
      |
      v
MaintenanceSetupWrapper
      |
      | tâche FreeRTOS dédiée, cœur 0, pile 16384 octets
      v
MaintenanceBoot
      |
      | Wi-Fi minimal
      | TLS GitHub
      | probe ou future lecture manifeste
      | persistance du résultat NVS
      v
Redémarrage vers runtime normal
      |
      v
GET /api/maintenance/last-result
      |
      v
Affichage /ota
```

## 3. Composants

### `MaintenanceRequestStore`

Stocke la commande de maintenance dans le namespace NVS `aq_maint`.

La commande est effacée avant exécution afin d'empêcher toute boucle de redémarrage persistante.

### `MaintenanceSetupWrapper`

Intercepte le démarrage avant le `setup()` nominal. Lorsqu'une commande est présente, il lance la maintenance dans une tâche FreeRTOS dédiée, suspend le chemin nominal et prévoit un repli sûr si la tâche ne peut pas être créée.

### `MaintenanceBoot`

Exécute uniquement les opérations autorisées en mode minimal. OTA-2.2 valide le probe HTTPS GitHub, la mesure mémoire, la persistance du résultat et le retour automatique au runtime normal.

### `MaintenanceResultStore`

Stocke dans le namespace NVS `aq_maint_res` :

- validité ;
- succès ;
- commande ;
- ligne HTTP ;
- durée TLS ;
- uptime maintenance ;
- heap minimale ;
- détail d'erreur.

### `WebManager`

Expose :

- `GET /ota` ;
- `POST /api/maintenance/probe-github` ;
- `GET /api/maintenance/last-result`.

Le POST est refusé avec une zone active et le redémarrage est exécuté hors callback AsyncTCP.

## 4. Invariants de sécurité

1. Aucune opération OTA pendant un arrosage actif.
2. Aucun TLS GitHub dans le runtime complet.
3. Aucune modification du moteur d'arrosage.
4. Aucune libération de sprites pour récupérer de la mémoire.
5. Aucun usage de `initVariant()`.
6. Maintenance dans une tâche dédiée.
7. Commande NVS effacée avant exécution.
8. Retour automatique au runtime normal après succès ou échec.
9. Aucune écriture dans `app1` avant validation complète de l'installation.
10. `INSTALL_UPDATE` reste désactivé.

## 5. État des partitions

Le matériel réel utilise un layout dual OTA :

- `app0` / `ota_0` : 2 031 616 octets ;
- `app1` / `ota_1` : 2 031 616 octets ;
- `otadata` : 8 192 octets.

OTA-2.2 a été validé avec `app0` active et `app1` non écrite.

## 6. Modes dégradés

- SSID absent : résultat d'échec persisté puis retour normal.
- Wi-Fi indisponible : timeout borné puis retour normal.
- TLS impossible : erreur détaillée persistée puis retour normal.
- Réponse HTTP absente ou invalide : échec persisté puis retour normal.
- Création de tâche impossible : repli vers le démarrage nominal.
- Arrosage actif : commande refusée sans redémarrage.

## 7. Observabilité validée

Les logs `Maintenance:` donnent au minimum :

- commande détectée ;
- entrée en mode minimal ;
- mémoire avant et après TLS ;
- durée de connexion ;
- ligne HTTP ;
- persistance du résultat ;
- garantie `otaWrite=no`.

Validation OTA-2.2 sur V4 :

```text
Maintenance: GitHub TLS connected=yes durationMs=1596
Maintenance: GitHub status=HTTP/1.1 404 Not Found
Maintenance: resultat persiste success=yes tlsMs=1596 heapMin=144172
Maintenance: resultat command=probe_github success=yes otaWrite=no
```

## 8. Étape suivante

OTA-2.3 introduira `CHECK_VERSION` selon `docs/ota/GITHUB_MANIFEST_V1.md`.

Cette étape pourra lire et valider le manifeste, mais ne devra ni télécharger le firmware, ni ouvrir une partition OTA, ni modifier la partition de démarrage.
