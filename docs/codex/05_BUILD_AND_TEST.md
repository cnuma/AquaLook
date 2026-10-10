# 05 — Compilation et tests

Mis à jour le 10 octobre 2026. V4 est le seul moteur d’exécution ; le moteur
historique a été supprimé le 8 septembre 2026 (commit `2750866`) et n’entre
plus dans aucune procédure.

## Environnements PlatformIO

| Env | Usage |
|---|---|
| `ProgrammeArrosage_s3` | **production et validation**, module `.141` |
| `ProgrammeArrosage_s3_n16r8` | banc N16R8 sans écran (non validé sur matériel) |
| `test_*_s3` (`test_relay_s3`, `test_sd_s3`, …) | bancs matériels ciblés |
| `ProgrammeArrosage` | carte CYD en cours d’abandon : compilée **seulement** sur demande explicite |

`ProgrammeArrosage_legacy` et `ProgrammeArrosage_v4` ne sont que des alias
historiques de `ProgrammeArrosage` ; ne pas les utiliser.

Les fichiers `test_*.cpp` définissent chacun `setup()`/`loop()` : tout nouveau
banc doit être exclu du `build_src_filter` de `[env:ProgrammeArrosage]`
(hérité par les envs S3), sinon le build de production échoue.

## Port série obligatoire

Avant la première compilation, le premier téléversement ou l’ouverture du
moniteur d’une session, demander sur quel port COM la carte est connectée.

- ne jamais supposer que le port de `platformio.ini` ou d’une ancienne
  session est encore valable ;
- utiliser le marqueur `<PORT_COM>` tant que le port n’est pas confirmé ;
- `pio device list` pour identifier les ports disponibles ;
- conserver le port confirmé pour toute la session, le redemander si la carte
  n’est plus détectée.

## Chaîne courante

```powershell
git -c core.whitespace=cr-at-eol diff --check
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

- `upload` compile avant de téléverser ; si la compilation échoue, rien n’est
  écrit sur la carte. Build S3 complet : environ 5 minutes.
- Sans téléversement prévu : `pio run -e ProgrammeArrosage_s3`.
- `--dtr 1` est obligatoire sur le S3 (USB natif) : sans lui le moniteur reste
  muet. Le port se ré-énumère à chaque redémarrage.
- Fermer le moniteur avant un téléversement (il tient le port).
- `announce_reboot.py` ouvre la fenêtre de maintenance de la campagne de soak
  avant tout flash de `.141`.
- `git diff --check` simple signale à tort les fins de ligne CRLF : utiliser
  `-c core.whitespace=cr-at-eol`.
- Un build qui échoue juste après un changement de bibliothèque peut réussir
  au second passage (patchs appliqués au build, `tools/patch_asyncwebserver.py`).

## LittleFS

`littlefs/` est le `data_dir` PlatformIO. Après toute modification :

```powershell
pio run -e ProgrammeArrosage_s3 -t buildfs
```

Les pages Web vivent sur la SD (`data/`) : elles se déposent sur `.141`
(`/api/debug/deploy-*`) ou se publient par `tools/publish_web_assets.py`
(fichiers d’abord, manifeste en dernier) puis `tools/check_published_web.py`.

## NVS

Avant un flash qui change un format persisté :

```powershell
python tools/nvs_backup.py save <PORT_COM> avant-maj.bin
```

Toute nouvelle taille de structure est ajoutée à la garde de longueur de
`ConfigManager::load()`.

## Règle de validation

Une fonction n’est validée que si :

1. elle compile dans `ProgrammeArrosage_s3` ;
2. son chemin est réellement instancié et appelé dans le runtime ;
3. elle a été testée sur la carte avec un effet observable ;
4. le résultat observé correspond au résultat attendu documenté.

Consigner pour chaque essai : profil flashé, port série, fichier et fonction,
point d’entrée exécuté, zone ou matériel utilisé, résultat attendu, résultat
observé, écarts et tests non effectués. Vérifier l’identité réellement
exécutée (série ou `/api/diagnostics`, champ `build`).

Les tests relais commencent par une seule zone, durée courte, sous
surveillance.

## Matrice de validation minimale

| Type de changement | Compile S3 | buildfs | Test sur `.141` |
|---|---:|---:|---:|
| C++ métier | Oui | Si `littlefs/` touché | Selon impact |
| Relais, équipements, moteur V4 | Oui | Non | Obligatoire |
| Pages Web (`data/`) | Non (sauf code associé) | Non | Dépôt SD + navigateur |
| `littlefs/` | Oui | Oui | Oui |
| Format NVS | Oui | Non | Obligatoire, avec sauvegarde NVS |
| LCD / tactile | Oui | Si assets touchés | Obligatoire |
| Serveur (`cloud/php-mutualized/`) | Non | Non | Banc local MariaDB, puis production après publication |
| Documentation seule | Non | Non | Non |

## Observer un module

- Série en priorité : le journal `/api/logs` est en RAM et se vide à chaque
  redémarrage.
- `/api/diagnostics` : `build`, `resetReason`, `wifi.rssi`, heap,
  `runtimeComponents`.
- `/api/health`, `/api/adminStatus`, `/api/logs.txt`.
- Décoder un plantage :
  `xtensa-esp32s3-elf-addr2line -pfiaC -e .pio/build/ProgrammeArrosage_s3/firmware.elf <adresses>`.

## Tests Web de non-régression

Page principale, zones, planning, modales, démarrage et arrêt manuels,
modification de zone, créneaux, météo, paramètres utilisateur, verrouillage
administrateur, sauvegarde Wi-Fi avant redémarrage, journaux, portail captif,
pages Santé, Synchro cloud, OTA, scripts.

## Tests NVS

Démarrage sans NVS, sauvegarde, redémarrage, chargement, CRC invalide, taille
invalide, réinitialisation, migration de schéma.

## Tests planning

Heure non synchronisée, créneau actif/inactif, plusieurs zones et créneaux,
changement de jour, passage de minuit, intervalle, pluie sous/au-dessus du
seuil, durée maximale.

## Tests relais

Démarrage sûr (tout OFF), logique directe et inverse, XL9535, MCP23017 si
disponible, première et dernière zone, arrêt manuel, timeout de sécurité,
redémarrage pendant un arrosage.

## Critères de livraison

- compilation `SUCCESS` de `ProgrammeArrosage_s3` pour tout changement de
  firmware (compilation seule ou commande `upload`) ;
- buildfs `SUCCESS` si `littlefs/` change ;
- diff contrôlé, état Git explicite ;
- port série et profil réellement flashé indiqués ;
- tests matériels exécutés et non exécutés listés.
