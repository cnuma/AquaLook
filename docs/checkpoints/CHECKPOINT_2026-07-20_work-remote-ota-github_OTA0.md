# Checkpoint AquaLook — OTA-0 — État initial et faisabilité

Date initiale : 2026-07-20  
Mesures de compilation ajoutées : 2026-07-21

## 1. Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/remote-ota-github`
- Point de départ de la branche : `dad5b48b622fb1c5dbc8db033acd8bdf79988302`
- Commit de définition OTA : `244f1e6b24ba60eb763c939139eec610a79cd486`
- Document d’architecture : `docs/architecture/REMOTE_OTA_GITHUB.md`

Le chantier notifications reste en pause. La mise à jour distante devient prioritaire.

## 2. Objet de RUN OTA-0

RUN OTA-0 détermine si le firmware AquaLook actuel peut évoluer vers une mise à jour distante sûre sans nouvelle migration de table de partitions. Il mesure également la place réellement disponible pour introduire la couche OTA.

Aucun code fonctionnel OTA n’est introduit pendant ce run.

## 3. Table de partitions confirmée

Le fichier `aqualook_partitions.csv` contient :

| Partition | Type | Offset | Taille hexadécimale | Taille utile |
|---|---|---:|---:|---:|
| `nvs` | données NVS | `0x9000` | `0x5000` | 20 KiB |
| `otadata` | métadonnées OTA | `0xE000` | `0x2000` | 8 KiB |
| `app0` | application `ota_0` | `0x10000` | `0x1F0000` | 1 984 KiB |
| `app1` | application `ota_1` | `0x200000` | `0x1F0000` | 1 984 KiB |
| `spiffs` | LittleFS | `0x3F0000` | `0x10000` | 64 KiB |

La flash de 4 MiB est déjà organisée avec deux partitions applicatives de même taille et une partition `otadata`.

La taille maximale d’une image applicative est :

- `0x1F0000` octets ;
- 2 031 616 octets ;
- 1 984 KiB ;
- environ 1,94 MiB.

La première installation OTA ne nécessite donc a priori pas de migration vers une nouvelle table de partitions, à condition que le module actuellement installé ait bien été flashé avec cette même table.

## 4. Configuration PlatformIO confirmée

Les environnements principaux déclarent :

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

La configuration utilise `littlefs/` comme source minimale de secours et conserve `data/` comme source complète destinée à la carte SD. Cette organisation est cohérente avec les deux partitions OTA de 1 984 KiB.

## 5. Mesures réelles des firmwares

Les binaires ont été compilés localement sur la branche `work/remote-ota-github` puis mesurés avec PowerShell.

### 5.1 Profil nominal Legacy

- environnement : `ProgrammeArrosage` ;
- fichier : `.pio/build/ProgrammeArrosage/firmware.bin` ;
- taille : **1 327 152 octets** ;
- taille : **1 296,05 KiB** ;
- occupation de la partition : **65,32 %** ;
- espace restant : **704 464 octets** ;
- espace restant : **687,95 KiB**.

### 5.2 Profil V4

- environnement : `ProgrammeArrosage_v4` ;
- fichier : `.pio/build/ProgrammeArrosage_v4/firmware.bin` ;
- taille : **1 331 472 octets** ;
- taille : **1 300,27 KiB** ;
- occupation de la partition : **65,54 %** ;
- espace restant : **700 144 octets** ;
- espace restant : **683,73 KiB**.

### 5.3 Comparaison

Le profil V4 est plus volumineux que le profil Legacy de :

- **4 320 octets** ;
- **4,22 KiB** ;
- environ **0,22 point de pourcentage** de la partition.

Les deux profils restent très en dessous du seuil de vigilance de 85 %.

## 6. Conclusion sur la marge flash

Les mesures classent les deux firmwares dans la zone **marge confortable** définie par OTA-0 :

- inférieur ou égal à 85 % : marge confortable ;
- entre 85 % et 90 % : acceptable mais à surveiller ;
- entre 90 % et 95 % : risque élevé ;
- supérieur à 95 % : OTA refusée jusqu’à réduction.

Avec environ 684 à 688 KiB encore disponibles, la taille du firmware ne constitue pas actuellement un obstacle à l’ajout de la couche OTA, du manifeste, de la validation SHA-256, de l’instrumentation et du mécanisme de rollback.

Cette marge ne doit toutefois pas être consommée sans suivi. La taille des binaires devra être enregistrée à chaque run OTA et contrôlée automatiquement dans la future chaîne de publication.

## 7. Vérification indispensable sur le module installé

La présence de la table dans le dépôt ne garantit pas à elle seule que le module fixé au mur a été flashé avec cette version exacte.

Avant la première écriture OTA, le firmware devra journaliser au démarrage :

- la partition applicative active ;
- son sous-type `ota_0` ou `ota_1` ;
- son adresse de départ ;
- sa taille ;
- la présence de la seconde partition OTA ;
- la présence de `otadata` ;
- la prochaine partition de mise à jour sélectionnée par ESP-IDF.

Critère : le module installé doit annoncer deux partitions applicatives d’au moins `0x1F0000` et une partition `otadata` valide.

Si le matériel installé utilise une ancienne table sans `ota_1`, une dernière intervention filaire sera nécessaire. Avec l’état actuel du dépôt, ce scénario paraît peu probable mais n’est pas encore exclu matériellement.

## 8. État mémoire et HTTPS/TLS

Le diagnostic précédent a établi :

- DNS fonctionnel ;
- TCP fonctionnel ;
- échec TLS `SSL - Memory allocation failed (-32512)` ;
- heap libre d’environ 74 KiB ;
- plus grand bloc contigu d’environ 39 KiB ;
- fragmentation mémoire comme cause probable ;
- régression tactile et portail captif lors de la tentative de libération des sprites.

Ces faits ne bloquent pas le partitionnement OTA ni la taille du firmware, mais bloquent encore la qualification du transport HTTPS.

Aucune stratégie de libération des sprites ne doit être réintroduite.

RUN OTA-1 devra mesurer la mémoire :

- au repos après démarrage ;
- juste avant connexion TLS ;
- après établissement TLS ;
- pendant le téléchargement ;
- après fermeture de la connexion ;
- après plusieurs heures de fonctionnement ;
- après plusieurs dizaines de connexions.

## 9. Décisions OTA-0

### D-OTA0-1 — Table conservée

Conserver `aqualook_partitions.csv` sans modification pendant les premiers runs OTA.

### D-OTA0-2 — Capacité flash validée

Les profils Legacy et V4 occupent environ 65,5 % de leur partition. La capacité flash est validée pour poursuivre le chantier OTA.

### D-OTA0-3 — Pas d’écriture OTA immédiate

Ne pas écrire dans la partition inactive avant :

1. lecture de la table réelle sur le module installé ;
2. qualification HTTPS/TLS OTA-1 ;
3. validation d’un téléchargement en flux sans installation ;
4. définition du boot sain et du rollback.

### D-OTA0-4 — Première cible logicielle

La première modification sera un diagnostic de partitions et de mémoire, sans téléchargement et sans installation OTA.

### D-OTA0-5 — Contrôle continu de taille

La future chaîne de publication devra refuser automatiquement un firmware dépassant le seuil autorisé et publier sa taille, son taux d’occupation et sa marge dans les artefacts de release.

## 10. Critères de clôture OTA-0

| Critère | État |
|---|---|
| Table du dépôt inspectée | VALIDÉ |
| Deux partitions OTA présentes | VALIDÉ |
| `otadata` présente | VALIDÉ |
| PlatformIO relié à la table | VALIDÉ |
| Taille limite connue | VALIDÉ — 2 031 616 octets |
| Taille firmware Legacy | VALIDÉ — 1 327 152 octets, 65,32 % |
| Taille firmware V4 | VALIDÉ — 1 331 472 octets, 65,54 % |
| Marge Legacy | VALIDÉ — 704 464 octets |
| Marge V4 | VALIDÉ — 700 144 octets |
| Capacité flash pour poursuivre l’OTA | VALIDÉ |
| Table réelle du module installé | À INSTRUMENTER |
| Faisabilité HTTPS/TLS | REPORTÉE À OTA-1 |

## 11. Conclusion

OTA-0 confirme que l’architecture flash du dépôt est déjà favorable à une mise à jour distante à double partition.

Les deux firmwares occupent seulement environ 65,5 % des partitions applicatives et laissent plus de 680 KiB libres. La taille du firmware ne constitue donc pas un verrou pour l’OTA.

Le seul point matériel restant pour clôturer complètement la faisabilité initiale est de confirmer la table réellement installée sur le programmateur en service.

La prochaine étape est `OTA-0.1` : ajouter uniquement l’observabilité de la table réelle et des métriques mémoire au démarrage. Ce run ne doit établir aucune connexion HTTPS et ne doit modifier ni l’écran, ni le tactile, ni le portail captif, ni l’autorité du moteur d’arrosage.