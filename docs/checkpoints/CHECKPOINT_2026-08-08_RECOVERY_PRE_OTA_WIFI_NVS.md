# Checkpoint AquaLook — Récupération pré-OTA / base matérielle revalidée

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
- initialisation TFT ;
- initialisation relais Legacy ;
- initialisation planning ;
- démarrage serveur Web ;
- initialisation météo ;
- initialisation écran ;
- entrée dans la boucle principale.

### Portail captif, scan WiFi et persistance

Validé sur matériel avec NVS sans identifiants WiFi :

```text
WiFi: pas de SSID, portail captif
WiFi: demarrage portail captif
WiFi: AP 'Arrosage-Setup' IP=192.168.4.1
WiFi: scan reseau lance
```

Le point d'accès `Arrosage-Setup` a été accessible, le scan réseau a fonctionné et le SSID souhaité a pu être sélectionné.

Un premier essai, avant effacement complet de la flash, a échoué avec :

```text
nvs_set_blob fail: config NOT_ENOUGH_SPACE
Config: ecriture NVS incomplete (0/4868)
```

Après effacement complet de la flash et reflash du firmware de référence, la sauvegarde a réussi :

```text
WiFi: sauvegarde identifiants via NVS
Config: sauvegarde NVS OK (4868 octets, schema 1)
```

Après redémarrage, la configuration a été relue et la reconnexion automatique confirmée :

```text
WiFi: connecte IP=192.168.1.198 RSSI=-64dBm, veille desactivee
```

Le cycle complet suivant est validé :

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

### Web et ressources SD

Validé sur matériel en fonctionnement nominal avec carte SD insérée :

- montage de la SD ;
- validation des ressources Web dans `/www` ;
- page principale accessible ;
- présentation/CSS normale ;
- navigation Web fonctionnelle ;
- affichage des zones ;
- page planning ;
- page paramètres ;
- page logs fonctionnelle.

### NTP et météo

Validé sur matériel :

```text
NTP: synchronise 08/08/2026 21:31:07
Meteo: HTTP 200 annonce=16664 lecture=stream type=inconnu
Meteo: fetch termine taille=16664 pluie=0.0mm temp=26.4C statut=ok
```

La reconfiguration NTP et un nouveau fetch météo après modification de configuration ont également été observés avec succès.

### Relais Legacy

Validé sur matériel pour les deux zones actives :

- zone 1 : activation puis arrêt manuel ;
- zone 2 : activation puis arrêt manuel ;
- commandes physiques envoyées au XL9535 `0x20` ;
- logique directe ;
- retours `ON` puis `OFF` observés dans les logs.

Les essais ont été effectués sans charge réelle raccordée aux relais.

### LCD, tactile et configuration persistée

Validé par contrôle utilisateur sur matériel :

- affichage LCD normal ;
- réveil et navigation écran ;
- tactile XPT2046 fonctionnel ;
- navigation entre écrans fonctionnelle ;
- modification d'un paramètre depuis le Web ;
- prise en compte de la modification ;
- redémarrage ;
- persistance de la modification après reboot.

### Planning

Le planning s'initialise correctement avec 2 zones actives. Les paramètres de planning et de configuration sont accessibles et persistants. Un déclenchement automatique sur créneau horaire dédié n'a pas été rejoué pendant cette campagne, mais la chaîne manuelle ScheduleManager -> backend physique a été validée pour les deux zones.

## État des sous-systèmes

| Sous-système | Statut au 8 août 2026 | Observation |
|---|---|---|
| Boot ESP32 | VALIDÉ | setup terminé et boucle démarrée |
| I2C | VALIDÉ | XL9535 `0x20` détecté |
| Relais Legacy | VALIDÉ | zones 1 et 2 ON/OFF manuellement |
| TFT / LCD | VALIDÉ | affichage et navigation normaux |
| Touch XPT2046 | VALIDÉ | navigation tactile confirmée |
| Planning / configuration | VALIDÉ FONCTIONNELLEMENT | initialisation, configuration et persistance OK ; créneau automatique non rejoué |
| Portail captif | VALIDÉ | AP `Arrosage-Setup` opérationnel |
| Scan WiFi | VALIDÉ | sélection SSID fonctionnelle |
| Sauvegarde WiFi NVS | VALIDÉ | `4868 octets, schema 1` |
| Persistance après reboot | VALIDÉ | WiFi et paramètres conservés |
| Serveur Web | VALIDÉ | pages principales et logs accessibles |
| Carte SD en fonctionnement nominal | VALIDÉ | montage et ressources `/www` opérationnels |
| Retrait SD à chaud | ANOMALIE CONNUE / DIFFÉRÉE | peut provoquer un panic après détection du retrait |
| LittleFS | VALIDÉ AU BOOT | montage réussi ; certains assets de secours absents |
| NTP | VALIDÉ | synchronisation et reconfiguration observées |
| OpenWeatherMap | VALIDÉ | HTTP 200, parsing en flux, statut OK |
| V4 | NON RETESTÉ | la nouvelle base fonctionnelle est Legacy |

## Anomalies et risques observés

### NVS héritée des développements ultérieurs

Une NVS non nettoyée peut empêcher la réécriture du blob de configuration avec `NOT_ENOUGH_SPACE`. Pour une récupération fiable vers ce checkpoint après avoir exécuté des versions plus récentes, un effacement complet de la flash avant reflash est recommandé.

Ce comportement ne doit pas être confondu avec un défaut du portail captif : le portail, le scan et la réception des identifiants fonctionnaient avant l'échec d'écriture.

### Mot de passe WiFi journalisé en clair

Le firmware de cette époque journalise la valeur du mot de passe reçue par le formulaire WiFi. Ce comportement est considéré comme une dette de sécurité et devra être supprimé dans une correction dédiée. Aucun secret ne doit être copié dans un checkpoint ou un log versionné.

### Retrait de la carte SD à chaud

Le retrait à chaud de la carte est détecté par le runtime, mais un essai a ensuite provoqué un panic `LoadProhibited` et un redémarrage de l'ESP32. Cette anomalie est volontairement laissée de côté pour le nouveau départ et devra faire l'objet d'une correction dédiée ultérieure.

La présence et l'utilisation nominale de la SD avec les ressources `/www` sont validées. Seul le scénario de retrait/remise en place à chaud reste non fiable.

### Ressources LittleFS de secours

Après effacement complet de la flash et reflash du firmware seul, certains fichiers LittleFS de secours tels que `splash.jpg`, `logo.png`, `favicon.ico` ou certaines routes non statiques peuvent être absents. Le fonctionnement Web nominal depuis la SD reste validé. Une future campagne pourra vérifier séparément l'image LittleFS de secours et son `uploadfs`.

## Invariants confirmés

- `ConfigManager` reste propriétaire de la persistance NVS et du montage LittleFS.
- Les identifiants WiFi sont chargés depuis la configuration persistante.
- Le portail captif et le mode connecté restent des états exclusifs.
- La réponse de configuration WiFi précède le redémarrage.
- Le profil matériel validé reste le backend Legacy.
- Le pilotage relais passe par la chaîne applicative attendue jusqu'au backend physique.
- La configuration reste persistante après reboot.
- Aucun changement fonctionnel OTA n'est intégré à cette branche de récupération.

## Fichiers fonctionnels modifiés par ce checkpoint

Aucun.

Le seul fichier documentaire modifié est :

`docs/checkpoints/CHECKPOINT_2026-08-08_RECOVERY_PRE_OTA_WIFI_NVS.md`

## Points restant volontairement hors validation complète

1. retrait/remise en place de la SD à chaud ;
2. image LittleFS de secours complète après `uploadfs` ;
3. déclenchement automatique réel d'un créneau de planning ;
4. sécurité de durée maximale sur un cycle volontairement prolongé ;
5. profil V4.

Ces points ne bloquent pas l'utilisation du firmware Legacy comme nouvelle base fonctionnelle de développement, mais doivent être conservés dans la dette de validation.

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

**BASE DE RÉCUPÉRATION LEGACY : VALIDÉE FONCTIONNELLEMENT SUR MATÉRIEL LE 8 AOÛT 2026.**

Cette branche devient le nouveau point de départ de référence pour les développements AquaLook qui doivent repartir de l'état fonctionnel pré-OTA. Toute évolution doit préserver les comportements revalidés ci-dessus et être développée par petits incréments avec test matériel.
