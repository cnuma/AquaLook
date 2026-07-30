# Checkpoint AquaLook — OTA-1.1 — TLS GitHub dans le runtime complet

Date : 2026-07-22

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/remote-ota-github`
- Checkpoint précédent : `docs/checkpoints/CHECKPOINT_2026-07-22_work-remote-ota-github_OTA1.md`
- OTA-1 isolé validé : TLS GitHub réussi, requête HTTP reçue, aucune écriture OTA.

## Objectif

Vérifier que la connexion HTTPS vers GitHub reste possible lorsque le firmware AquaLook complet est actif : écran, tactile, stockage, serveur Web, planificateur, relais et couches d’exécution.

## Modifications

### `src/OtaTlsProbe.h`

Interface minimale de la sonde, désactivée par défaut avec `AQUALOOK_OTA_TLS_PROBE=0`.

### `src/OtaTlsProbe.cpp`

- enregistrement d’un callback Wi-Fi réservé au profil OTA-1.1 ;
- lancement unique après obtention d’une adresse IP ;
- délai de stabilisation de 5 secondes ;
- tâche dédiée sur le cœur 0 avec pile de 8192 octets ;
- requête `HEAD` vers `api.github.com` ;
- validation TLS temporairement en mode `setInsecure()` ;
- métriques heap, minimum de heap, plus grand bloc contigu et marge de pile ;
- aucune écriture dans une partition OTA.

### `platformio_ota.ini`

Profil supplémentaire `ProgrammeArrosage_ota_tls_probe` héritant du firmware nominal complet et activant uniquement :

```ini
-DAQUALOOK_OTA_TLS_PROBE=1
```

Le fichier `platformio.ini` nominal n’est pas modifié.

## Procédure

Compiler :

```powershell
git switch work/remote-ota-github
git pull
pio run -c platformio_ota.ini -e ProgrammeArrosage_ota_tls_probe
```

Après `SUCCESS`, flasher sur COM3 :

```powershell
pio run -c platformio_ota.ini -e ProgrammeArrosage_ota_tls_probe -t upload --upload-port COM3
pio device monitor -p COM3 -b 115200
```

Relever toutes les lignes commençant par :

```text
OTA-1.1:
```

## Critères de validation

- firmware complet démarre normalement ;
- écran et tactile restent fonctionnels ;
- serveur Web reste accessible ;
- aucun relais n’est commandé par la sonde ;
- `tls connected=yes` ;
- réponse HTTP reçue, même `404` ;
- `result=success tls=yes request=yes otaWrite=no` ;
- heap et plus grand bloc relevés avant, pendant et après TLS ;
- aucune fuite ou fragmentation majeure après fermeture.

## Sécurité

- aucune écriture `app0`, `app1` ou `otadata` ;
- aucune installation OTA ;
- aucune libération de sprites ;
- aucun changement du tactile ou du portail captif ;
- validation de certificat désactivée uniquement pour qualifier le transport TLS.

## Suite

Après validation matérielle, reflasher le profil nominal :

```powershell
pio run -e ProgrammeArrosage -t upload --upload-port COM3
```

La suite OTA ne pourra introduire le téléchargement d’un manifeste qu’après analyse des mesures mémoire OTA-1.1.
