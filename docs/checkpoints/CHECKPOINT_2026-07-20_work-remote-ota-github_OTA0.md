# Checkpoint AquaLook — OTA-0 — État initial et faisabilité

Date : 2026-07-20

## 1. Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/remote-ota-github`
- Point de départ de la branche : `dad5b48b622fb1c5dbc8db033acd8bdf79988302`
- Commit de définition OTA : `244f1e6b24ba60eb763c939139eec610a79cd486`
- Document d’architecture : `docs/architecture/REMOTE_OTA_GITHUB.md`

Le chantier notifications reste en pause. La mise à jour distante devient prioritaire.

## 2. Objet de RUN OTA-0

RUN OTA-0 doit déterminer si le firmware AquaLook actuel peut évoluer vers une mise à jour distante sûre sans nouvelle migration de table de partitions, puis identifier les mesures qui doivent encore être obtenues par compilation locale et sur le matériel réel.

Aucun code fonctionnel OTA n’est introduit pendant ce run.

## 3. Résultats confirmés dans le dépôt

### 3.1 Table de partitions déjà compatible avec l’OTA double image

Le fichier `aqualook_partitions.csv` contient :

| Partition | Type | Offset | Taille hexadécimale | Taille utile |
|---|---|---:|---:|---:|
| `nvs` | données NVS | `0x9000` | `0x5000` | 20 KiB |
| `otadata` | métadonnées OTA | `0xE000` | `0x2000` | 8 KiB |
| `app0` | application `ota_0` | `0x10000` | `0x1F0000` | 1 984 KiB |
| `app1` | application `ota_1` | `0x200000` | `0x1F0000` | 1 984 KiB |
| `spiffs` | LittleFS déclaré par PlatformIO | `0x3F0000` | `0x10000` | 64 KiB |

La flash de 4 MiB est donc déjà organisée avec deux partitions applicatives de même taille et une partition `otadata`.

### 3.2 Conséquence immédiate

La première installation OTA ne nécessite a priori pas de migration vers une nouvelle table de partitions, à condition que le module actuellement installé ait bien été flashé avec cette même table `aqualook_partitions.csv`.

Le dépôt est prêt structurellement pour écrire une nouvelle image dans la partition inactive.

La taille maximale théorique d’une image applicative est :

- `0x1F0000` octets ;
- 2 031 616 octets ;
- 1 984 KiB ;
- environ 1,94 MiB.

Une marge de sécurité doit rester disponible. La cible de conception retenue pour la suite est de ne pas dépasser 90 % de la partition applicative, soit environ 1 785 KiB, sauf justification et validation explicites.

### 3.3 PlatformIO utilise explicitement cette table

Les environnements principaux suivants déclarent :

```ini
board_build.partitions = aqualook_partitions.csv
board_build.filesystem = littlefs
```

Profils concernés :

- `ProgrammeArrosage` ;
- `ProgrammeArrosage_legacy` par héritage ;
- `ProgrammeArrosage_v4` par héritage ;
- `debug_boot`.

Le profil nominal reste `ProgrammeArrosage` avec le backend relais historique actif.

### 3.4 Ressources Web déjà réduites en flash

La configuration PlatformIO utilise `littlefs/` comme source minimale de secours et conserve `data/` comme source complète destinée à la carte SD.

La partition LittleFS n’occupe plus que 64 KiB. Cette organisation est cohérente avec la conservation de deux partitions applicatives de 1 984 KiB.

## 4. Point non encore mesurable depuis GitHub seul

### 4.1 Taille exacte du firmware

La taille exacte du firmware compilé n’est pas versionnée dans le dépôt et aucun artefact de compilation exploitable n’est disponible dans l’environnement d’audit.

RUN OTA-0 ne peut donc pas encore conclure quantitativement sur :

- la taille réelle de `firmware.bin` ;
- le pourcentage d’occupation de `app0`/`app1` ;
- la marge restante pour le futur code OTA ;
- la taille du firmware Legacy comparée au firmware V4.

Cette mesure doit être produite sur le poste PlatformIO utilisé pour AquaLook.

### 4.2 Commandes de mesure obligatoires

Depuis la racine du dépôt, sur la branche `work/remote-ota-github` :

```powershell
git switch work/remote-ota-github
git pull
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

Puis relever les lignes finales PlatformIO :

```text
RAM:   [....] ...% (used ... bytes from ... bytes)
Flash: [....] ...% (used ... bytes from 2031616 bytes)
```

Mesurer également les fichiers produits :

```powershell
Get-Item .pio\build\ProgrammeArrosage\firmware.bin |
  Select-Object FullName, Length

Get-Item .pio\build\ProgrammeArrosage_v4\firmware.bin |
  Select-Object FullName, Length
```

Calcul de la marge pour le profil nominal :

```powershell
$limit = 0x1F0000
$size = (Get-Item .pio\build\ProgrammeArrosage\firmware.bin).Length
[pscustomobject]@{
  FirmwareBytes = $size
  PartitionBytes = $limit
  FreeBytes = $limit - $size
  OccupationPercent = [math]::Round(($size / $limit) * 100, 2)
}
```

## 5. Vérification indispensable sur le module déjà installé

La présence de la table dans le dépôt ne garantit pas à elle seule que le module fixé au mur a été flashé avec cette version exacte.

Avant la première écriture OTA, le firmware devra journaliser au démarrage la table réellement détectée au moyen des API de partitions ESP-IDF, notamment :

- partition applicative en cours ;
- sous-type `ota_0` ou `ota_1` ;
- adresse de départ ;
- taille ;
- présence de la seconde partition OTA ;
- présence de `otadata`.

Critère : le module installé doit annoncer deux partitions applicatives d’au moins `0x1F0000` et une partition `otadata` valide.

Si le matériel installé utilise une ancienne table sans `ota_1`, une dernière intervention filaire sera nécessaire. Avec l’état actuel du dépôt, ce scénario paraît peu probable mais n’est pas encore exclu matériellement.

## 6. État mémoire et HTTPS/TLS

Le diagnostic précédent a établi :

- DNS fonctionnel ;
- TCP fonctionnel ;
- échec TLS `SSL - Memory allocation failed (-32512)` ;
- heap libre d’environ 74 KiB ;
- plus grand bloc contigu d’environ 39 KiB ;
- fragmentation mémoire comme cause probable ;
- régression tactile et portail captif lors de la tentative de libération des sprites.

Ces faits ne bloquent pas le partitionnement OTA mais bloquent encore la qualification du transport HTTPS.

Aucune stratégie de libération des sprites ne doit être réintroduite.

RUN OTA-1 devra mesurer la mémoire :

- au repos après démarrage ;
- juste avant connexion TLS ;
- après établissement TLS ;
- pendant le téléchargement ;
- après fermeture de la connexion ;
- après plusieurs heures de fonctionnement ;
- après répétition de plusieurs dizaines de connexions.

## 7. Inventaire initial des consommateurs réseau

Le runtime principal instancie simultanément :

- `WiFiManager` ;
- `NTPManager` ;
- `NotificationManager` ;
- `WeatherManager` ;
- `WebManager` ;
- `StorageManager` ;
- l’écran et le tactile ;
- les couches d’exécution Legacy et V4 en mode shadow.

Le chantier notifications étant en pause, aucune connexion ntfy ne doit être active pendant les premiers essais OTA/TLS.

L’inventaire détaillé des allocations temporaires des bibliothèques TLS et HTTP appartient à OTA-1, car leur taille réelle dépend de l’exécution et ne peut être déduite de façon fiable par inspection statique seule.

## 8. Décisions OTA-0

### Décision D-OTA0-1 — Table conservée

Conserver `aqualook_partitions.csv` sans modification pendant les premiers runs OTA.

Justification : elle contient déjà `otadata`, `ota_0` et `ota_1` avec deux images de 1 984 KiB.

### Décision D-OTA0-2 — Pas d’écriture OTA immédiate

Ne pas écrire dans `app1` avant :

1. compilation réussie des profils Legacy et V4 ;
2. mesure exacte de la taille des binaires ;
3. lecture de la table réelle sur le module installé ;
4. qualification HTTPS/TLS OTA-1 ;
5. validation d’un téléchargement en flux sans installation.

### Décision D-OTA0-3 — Seuil de taille

- inférieur ou égal à 85 % : marge confortable ;
- entre 85 % et 90 % : acceptable mais à surveiller ;
- entre 90 % et 95 % : risque élevé pour les évolutions ;
- supérieur à 95 % : OTA refusée tant qu’une réduction n’est pas obtenue.

### Décision D-OTA0-4 — Première cible technique

La première modification logicielle du run suivant sera un diagnostic de partitions et de mémoire, sans téléchargement et sans installation OTA.

## 9. Critères de clôture OTA-0

| Critère | État |
|---|---|
| Table du dépôt inspectée | VALIDÉ |
| Deux partitions OTA présentes | VALIDÉ |
| `otadata` présente | VALIDÉ |
| PlatformIO relié à la table | VALIDÉ |
| Taille limite connue | VALIDÉ — 2 031 616 octets |
| Taille exacte firmware Legacy | À MESURER LOCALEMENT |
| Taille exacte firmware V4 | À MESURER LOCALEMENT |
| Marge exacte calculée | EN ATTENTE DES BINAIRES |
| Table réelle du module installé | À INSTRUMENTER |
| Faisabilité HTTPS/TLS | REPORTÉE À OTA-1 |

## 10. Conclusion

OTA-0 confirme que l’architecture flash du dépôt est déjà favorable à une mise à jour distante à double partition. Contrairement au risque initial identifié dans la roadmap, aucune nouvelle table de partitions n’est nécessaire dans le dépôt actuel.

Deux validations restent indispensables avant de conclure définitivement pour le module en service :

1. obtenir la taille exacte des firmwares compilés ;
2. confirmer que le module installé utilise réellement cette table.

La prochaine étape recommandée est un run intermédiaire minimal `OTA-0.1` ajoutant uniquement l’observabilité de la table réelle et des métriques mémoire au démarrage, puis compilation et lecture des résultats sur le matériel. Ce run ne doit établir aucune connexion HTTPS et ne doit modifier ni l’écran, ni le tactile, ni le portail captif, ni l’autorité du moteur d’arrosage.
