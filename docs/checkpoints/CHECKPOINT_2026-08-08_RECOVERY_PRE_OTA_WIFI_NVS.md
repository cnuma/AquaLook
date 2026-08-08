# Checkpoint AquaLook — Récupération pré-OTA / WiFi-NVS validé

Date : 8 août 2026

## Objet

Ce checkpoint documente la reprise volontaire d'AquaLook depuis le dernier état matériel validé de l'étape 6, avant le chantier OTA distant récent, afin de disposer d'une nouvelle base de développement propre et démontrée sur matériel.

Aucun code fonctionnel n'est modifié par ce checkpoint.

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche de récupération : `recovery/pre-ota-validated-2026-07-13`
- Commit firmware testé : `adc5ff6d9c7e3a2bf335fa5303755dfa068cc7a8`
- Message du commit firmware : `docs: cloture l etape 6 avec checkpoint materiel valide`
- Parent technique : `73e547f6497c000efe1282952533e0d7b60b6aca`
- Profil matériel testé le 8 août 2026 : `ProgrammeArrosage_legacy`
- Port série utilisé : `COM3`
- Moniteur série : `115200 bauds`

Le commit `adc5ff6...` reste le commit firmware de référence. Le présent document constitue la preuve complémentaire de revalidation matérielle sur la branche de récupération.

## Motivation de la récupération

Les développements postérieurs au 13 juillet 2026 ont introduit de nombreux changements autour de l'OTA, du boot, du stockage, de la configuration, du Web et du runtime. La récupération a donc été effectuée depuis le checkpoint matériel validé de fin d'étape 6 plutôt que depuis `main` ou une branche OTA.

La branche a été créée directement depuis `adc5ff6d9c7e3a2bf335fa5303755dfa068cc7a8` puis récupérée sur le poste de test. L'arbre Git local a été vérifié propre avant compilation et flash.

## État Git vérifié avant test

```text
On branch recovery/pre-ota-validated-2026-07-13
Your branch is up to date with 'origin/recovery/pre-ota-validated-2026-07-13'.
nothing to commit, working tree clean

adc5ff6 ... docs: cloture l etape 6 avec checkpoint materiel valide
```

## Chaîne de compilation et flash utilisée

PlatformIO Core observé : `6.1.19`.

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e ProgrammeArrosage_legacy
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e ProgrammeArrosage_legacy -t upload --upload-port COM3
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM3 -b 115200
```

Pour la revalidation NVS propre, la flash a ensuite été effacée puis le même firmware Legacy a été reflashed :

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e ProgrammeArrosage_legacy -t erase --upload-port COM3
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e ProgrammeArrosage_legacy -t upload --upload-port COM3
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM3 -b 115200
```

## Validation matérielle du 8 août 2026

### Boot nominal

Validé sur matériel :

- démarrage AquaLook ;
- scan I2C ;
- détection du XL9535 à l'adresse `0x20` ;
- montage LittleFS ;
- initialisation TFT et splash ;
- initialisation relais Legacy ;
- initialisation planning ;
- démarrage serveur Web ;
- initialisation météo ;
- initialisation écran ;
- entrée dans la boucle principale.

### Portail captif

Validé sur matériel avec NVS sans identifiants WiFi :

```text
WiFi: pas de SSID, portail captif
WiFi: demarrage portail captif
WiFi: AP 'Arrosage-Setup' IP=192.168.4.1
```

Le point d'accès `Arrosage-Setup` a été accessible et le portail de configuration a permis de lancer un scan réseau.

### Scan WiFi et sélection SSID

Validé sur matériel :

```text
WiFi: scan reseau lance
```

Le réseau souhaité a pu être sélectionné depuis l'interface de configuration et le mot de passe saisi.

### Sauvegarde NVS

Un premier essai, avant effacement complet de la flash, a échoué avec :

```text
nvs_set_blob fail: config NOT_ENOUGH_SPACE
Config: ecriture NVS incomplete (0/4868)
```

Cet état provenait d'une NVS ayant été utilisée par des développements ultérieurs. Après effacement complet de la flash et reflash du firmware de référence, la sauvegarde a réussi :

```text
WiFi: sauvegarde identifiants via NVS
Config: sauvegarde NVS OK (4868 octets, schema 1)
```

Conclusion : la sauvegarde du bloc NVS schéma 1 du checkpoint est fonctionnelle sur une flash propre.

### Persistance après redémarrage et reconnexion WiFi

Validé sur matériel après sauvegarde NVS et reboot :

```text
WiFi: connecte IP=192.168.1.198 RSSI=-64dBm, veille desactivee
```

Le cycle complet suivant est donc validé :

1. boot sans SSID ;
2. démarrage du portail captif ;
3. scan des réseaux ;
4. sélection du SSID ;
5. saisie du mot de passe ;
6. sauvegarde NVS ;
7. redémarrage ;
8. relecture de la configuration ;
9. reconnexion automatique au réseau WiFi ;
10. obtention d'une adresse IP.

## État des sous-systèmes

| Sous-système | Statut au 8 août 2026 | Observation |
|---|---|---|
| Boot ESP32 | VALIDÉ | setup terminé et boucle démarrée |
| I2C | VALIDÉ | périphérique `0x20` détecté |
| Relais Legacy / initialisation | VALIDÉ PARTIELLEMENT | topologie initialisée ; commutation physique non retestée pendant cette reprise |
| TFT / splash | VALIDÉ | affichage initialisé |
| Planning / initialisation | VALIDÉ PARTIELLEMENT | initialisation OK ; exécution de créneaux non retestée |
| Portail captif | VALIDÉ | AP `Arrosage-Setup` opérationnel |
| Scan WiFi | VALIDÉ | scan lancé depuis le portail |
| Sauvegarde WiFi NVS | VALIDÉ | `4868 octets, schema 1` |
| Persistance WiFi après reboot | VALIDÉ | reconnexion automatique confirmée |
| Serveur Web / démarrage | VALIDÉ PARTIELLEMENT | serveur port 80 démarré ; matrice Web complète à retester |
| Carte SD | À RETESTER | montage initial observé, mais une erreur de carte retirée/illisible a été vue lors d'un essai précédent |
| LittleFS | VALIDÉ AU BOOT | montage réussi |
| NTP | À RETESTER | initialisation observée, synchro complète non revalidée dans cette campagne |
| OpenWeatherMap | À RETESTER | initialisation observée, récupération météo non revalidée dans cette campagne |
| Touch XPT2046 | À RETESTER | non revalidé explicitement pendant cette campagne |
| V4 | NON RETESTÉ | la reprise fonctionnelle actuelle utilise Legacy |

## Anomalies et risques observés

### NVS héritée des développements ultérieurs

Une NVS non nettoyée peut empêcher la réécriture du blob de configuration avec `NOT_ENOUGH_SPACE`. Pour une récupération fiable vers ce checkpoint après avoir exécuté des versions plus récentes, un effacement complet de la flash avant reflash est recommandé.

Ce comportement ne doit pas être confondu avec un défaut du portail captif : le portail, le scan et la réception des identifiants fonctionnaient avant l'échec d'écriture.

### Mot de passe WiFi journalisé en clair

Le firmware de cette époque journalise la valeur du mot de passe reçue par le formulaire WiFi. Ce comportement est considéré comme une dette de sécurité et devra être supprimé dans une correction dédiée. Aucun secret ne doit être copié dans un checkpoint ou un log versionné.

### Carte SD

Lors d'un essai avant l'effacement complet, la SD avait d'abord été montée et les ressources `/www` validées, puis le runtime a signalé que la carte était retirée ou devenue illisible. La stabilité SD doit donc être explicitement retestée avant de déclarer la nouvelle base entièrement validée.

## Invariants confirmés

- `ConfigManager` reste propriétaire de la persistance NVS et du montage LittleFS.
- Les identifiants WiFi sont chargés depuis la configuration persistante.
- Le portail captif et le mode connecté restent des états exclusifs.
- La réponse de configuration WiFi précède le redémarrage.
- Le profil matériel testé reste le backend Legacy.
- Aucun changement fonctionnel OTA n'est intégré à cette branche de récupération.

## Fichiers fonctionnels modifiés par ce checkpoint

Aucun.

Le seul ajout est le présent document :

`docs/checkpoints/CHECKPOINT_2026-08-08_RECOVERY_PRE_OTA_WIFI_NVS.md`

## Tests à réaliser avant nouveau développement

Ordre recommandé :

1. stabilité carte SD et ressources `/www` ;
2. navigation Web complète et pages principales ;
3. NTP et affichage de l'heure ;
4. météo OpenWeatherMap ;
5. touch XPT2046 et navigation TFT ;
6. démarrage/arrêt manuel d'une zone avec durée courte ;
7. validation de la logique directe/inverse si nécessaire ;
8. planning avec un créneau contrôlé ;
9. sécurité de durée maximale ;
10. seulement ensuite décider du nouveau point de départ de développement.

## Procédure de reprise

```powershell
git fetch origin
git switch recovery/pre-ota-validated-2026-07-13
git pull
git status
git log -1 --oneline
```

Pour compiler et flasher le profil de récupération :

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e ProgrammeArrosage_legacy
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e ProgrammeArrosage_legacy -t upload --upload-port COM3
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM3 -b 115200
```

Si la carte a précédemment exécuté une branche récente avec une structure ou un usage NVS différent, effectuer d'abord un effacement complet de la flash avant le reflash du checkpoint.

## Statut

**Checkpoint de récupération WiFi/NVS : VALIDÉ SUR MATÉRIEL.**

La base n'est pas encore déclarée entièrement validée pour tous les sous-systèmes. Les tests SD, Web complet, NTP, météo, touch, relais et planning restent à exécuter avant reprise du développement fonctionnel.
