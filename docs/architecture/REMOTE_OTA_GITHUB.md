# Mise à jour distante AquaLook par GitHub Releases

## 1. Objet du document

Ce document définit le cadre d’architecture, de qualification et de réalisation de la mise à jour distante du firmware AquaLook.

L’objectif opérationnel est de pouvoir mettre à jour un programmateur déjà installé, sans démontage du boîtier, sans présence physique à proximité et sans connexion au même réseau local.

Cette fonctionnalité devient prioritaire par rapport au chantier de notifications, qui est mis en pause. Les notifications restent prévues comme fonction complémentaire, mais ne doivent ni retarder ni conditionner la mise en service de l’OTA.

## 2. Source de vérité et branche de travail

- Dépôt : `cnuma/AquaLook`
- Branche dédiée : `work/remote-ota-github`
- Point de départ : commit `dad5b48b622fb1c5dbc8db033acd8bdf79988302`
- Source fonctionnelle initiale : section « Mise à jour distante du firmware par GitHub Releases » de `ROADMAP.md`

Toute évolution OTA devra être développée, compilée et testée sur cette branche avant intégration dans une branche plus large.

## 3. Périmètre retenu

La première version doit permettre :

- de vérifier à distance la disponibilité d’un nouveau firmware ;
- de télécharger un manifeste publié avec une GitHub Release ;
- de vérifier la compatibilité du firmware avec le matériel et le schéma de partitions ;
- de télécharger le firmware par HTTPS sans le charger intégralement en RAM ;
- de vérifier son intégrité et, à terme, son authenticité ;
- de l’écrire dans la partition OTA inactive ;
- de redémarrer sur la nouvelle version ;
- de confirmer le bon démarrage ;
- de revenir automatiquement à la version précédente en cas d’échec ;
- de conserver la configuration NVS, les programmes, les journaux et les données présentes sur la carte SD.

La première version ne doit pas encore inclure :

- une gestion complète de parc ;
- un serveur cloud AquaLook ;
- un déploiement simultané sur plusieurs appareils ;
- une dépendance obligatoire à ntfy ou à un autre service de notification ;
- une installation automatique non supervisée pendant un cycle d’arrosage.

## 4. Architecture cible

### 4.1 Chaîne de publication

La chaîne de publication envisagée est la suivante :

1. compilation d’un firmware validé ;
2. calcul de son empreinte SHA-256 ;
3. génération d’un manifeste OTA ;
4. publication du firmware et du manifeste dans une GitHub Release ;
5. marquage explicite de la release comme stable et déployable ;
6. vérification par AquaLook de la version et de la compatibilité ;
7. téléchargement puis installation contrôlée.

GitHub Releases constitue la source officielle des firmwares publiés. Le dépôt source peut rester privé, mais il faut éviter d’embarquer dans le module un jeton GitHub personnel ou tout secret disposant de droits d’écriture.

### 4.2 Composants logiciels prévus

La fonction devra être isolée dans une couche dédiée, par exemple :

- `FirmwareUpdateManager` : orchestration générale ;
- `OtaManifest` : représentation et validation du manifeste ;
- `OtaTransport` : téléchargement HTTP/HTTPS en flux ;
- `OtaInstaller` : écriture dans la partition inactive ;
- `OtaBootGuard` : confirmation de démarrage et rollback ;
- `OtaJournal` : événements persistés et diagnostic ;
- `OtaPolicy` : règles d’autorisation, fenêtre de maintenance et compatibilité.

Le nom exact des classes reste à confirmer après inspection de l’architecture existante. Le moteur d’arrosage, le serveur Web, l’écran et le stockage ne doivent pas contenir directement la logique OTA.

### 4.3 Flux fonctionnel

Le flux nominal est :

1. déclenchement manuel ou vérification périodique ;
2. contrôle de l’état du module ;
3. refus ou report si un cycle est actif ;
4. téléchargement du manifeste ;
5. validation du format, de la version et de la compatibilité ;
6. contrôle de la mémoire et de l’espace OTA ;
7. téléchargement du firmware en flux ;
8. calcul de l’empreinte pendant le téléchargement ;
9. comparaison de l’empreinte attendue ;
10. finalisation de la partition OTA ;
11. enregistrement de l’état « mise à jour en attente de validation » ;
12. redémarrage ;
13. autodiagnostic minimal ;
14. confirmation de la nouvelle version ou rollback.

## 5. Qualification HTTPS/TLS préalable

Le diagnostic ntfy a montré :

- DNS fonctionnel ;
- connexion TCP fonctionnelle ;
- échec TLS avec `SSL - Memory allocation failed (-32512)` ;
- heap libre d’environ 74 Ko ;
- plus grand bloc contigu d’environ 39 Ko ;
- fragmentation mémoire comme cause probable.

L’OTA dépend elle aussi d’une connexion HTTPS. La qualification TLS est donc un verrou préalable obligatoire.

Avant toute écriture OTA, il faudra réaliser un banc progressif :

1. mesure de la heap libre et du plus grand bloc contigu au repos ;
2. connexion HTTPS simple vers un endpoint contrôlé ;
3. téléchargement d’un petit manifeste ;
4. répétition de la connexion plusieurs dizaines de fois ;
5. téléchargement d’un fichier de taille proche du firmware sans installation ;
6. mesure avant, pendant et après chaque étape ;
7. validation de l’absence de fuite mémoire ;
8. test après plusieurs heures de fonctionnement réel du programmateur.

Critère de passage : les connexions HTTPS doivent réussir de manière répétable sans désactiver le tactile, le portail captif, le planificateur ou les fonctions Web essentielles.

Il est explicitement interdit de reprendre la stratégie de libération des sprites graphiques ayant provoqué une régression tactile et portail captif.

## 6. Manifeste OTA proposé

Format initial proposé :

```json
{
  "product": "AquaLook",
  "channel": "stable",
  "version": "x.y.z",
  "build": 1,
  "hardware": ["ESP32-2432S028"],
  "partitionScheme": "aqualook-ota-v1",
  "minimumVersion": "x.y.z",
  "firmwareUrl": "https://.../firmware.bin",
  "firmwareSize": 0,
  "sha256": "...",
  "releaseDate": "YYYY-MM-DDTHH:MM:SSZ",
  "mandatory": false,
  "notes": "..."
}
```

Champs obligatoires pour la première version :

- produit ;
- canal ;
- version ;
- matériel compatible ;
- schéma de partitions ;
- URL du firmware ;
- taille ;
- SHA-256 ;
- date de publication.

Une signature numérique du manifeste ou du firmware doit être étudiée. Le SHA-256 seul protège contre une corruption accidentelle, mais ne constitue pas une preuve d’origine si le manifeste lui-même est compromis.

## 7. Table de partitions

L’OTA exige deux emplacements applicatifs :

- partition active ;
- partition OTA inactive.

La table devra également préserver :

- NVS ;
- données système nécessaires au bootloader ;
- espace résiduel LittleFS strictement nécessaire ;
- compatibilité avec les ressources Web désormais hébergées sur SD.

Aucune modification de partition ne doit être appliquée au matériel en service sans procédure de migration et de récupération clairement testée. Une première migration vers une table OTA peut nécessiter une dernière intervention filaire ; ce point devra être confirmé selon la table actuellement déployée.

## 8. Politique de déclenchement

Pour la première version :

- vérification manuelle depuis l’interface locale ;
- possibilité de vérification périodique désactivable ;
- installation explicitement confirmée par l’utilisateur ;
- interdiction de lancer une installation pendant un cycle d’arrosage ;
- report automatique si une zone est active ;
- journalisation de la demande et du motif de report ;
- aucune installation silencieuse au premier démarrage après publication.

Le déclenchement distant depuis Internet sera ajouté seulement après validation du téléchargement, du rollback et de la sécurité d’authentification.

## 9. Démarrage sain et rollback

La nouvelle version ne doit être confirmée qu’après validation d’un état de démarrage minimal comprenant au moins :

- initialisation du stockage de configuration ;
- montage ou diagnostic contrôlé de la carte SD ;
- initialisation de l’écran et du tactile ;
- initialisation du gestionnaire de relais ;
- absence de boucle de redémarrage ;
- démarrage du planificateur ;
- disponibilité du serveur Web minimal ;
- durée minimale de fonctionnement stable à définir.

En cas d’échec répété, le bootloader doit revenir à la version précédente. L’échec et son motif doivent être conservés localement.

## 10. Journalisation

Événements OTA minimum :

- vérification demandée ;
- manifeste téléchargé ;
- aucune mise à jour disponible ;
- version incompatible ;
- espace insuffisant ;
- mémoire TLS insuffisante ;
- téléchargement démarré ;
- progression par paliers ;
- téléchargement interrompu ;
- hash invalide ;
- installation finalisée ;
- redémarrage demandé ;
- nouvelle version confirmée ;
- rollback exécuté ;
- notification non envoyée.

Les journaux doivent rester disponibles après redémarrage et ne pas dépendre de la carte SD seule.

## 11. Notifications

Le chantier notifications est mis en pause.

L’architecture OTA doit néanmoins exposer des événements permettant ultérieurement de notifier :

- une version disponible ;
- le début d’une installation ;
- le succès ;
- l’échec ;
- le rollback.

La méthode reste à valider : ntfy direct, passerelle ESP32-S2, relais local, MQTT, Home Assistant ou service intermédiaire.

Aucune notification ne doit conditionner le téléchargement, l’installation, la confirmation ou le rollback.

## 12. Risques principaux

- impossibilité d’établir durablement TLS en raison de la fragmentation mémoire ;
- firmware trop volumineux pour un schéma à double partition ;
- migration initiale de table de partitions nécessitant une intervention physique ;
- certificat racine expiré ou incompatible ;
- coupure d’alimentation pendant l’écriture ;
- image compatible en apparence mais incompatible avec les données NVS ;
- boucle de redémarrage avant confirmation ;
- déclenchement pendant un arrosage ;
- indisponibilité temporaire de GitHub ;
- exposition d’un secret GitHub dans le firmware ;
- régression tactile ou portail captif provoquée par une tentative de libération mémoire.

## 13. Découpage de réalisation

### RUN OTA-0 — État initial et faisabilité

- inspecter la table de partitions actuelle ;
- mesurer la taille exacte du firmware ;
- mesurer les marges disponibles ;
- confirmer si une migration OTA est possible sans reflash filaire ;
- inventorier les allocations permanentes et temporaires liées au réseau.

### RUN OTA-1 — Banc HTTPS/TLS

- connexion répétée à un endpoint simple ;
- téléchargement d’un manifeste ;
- instrumentation mémoire ;
- test après fonctionnement prolongé ;
- aucun changement du mécanisme graphique ayant déjà régressé.

### RUN OTA-2 — Manifeste et politique de version

- parser le manifeste ;
- comparer les versions ;
- valider matériel et partitions ;
- afficher le résultat sans télécharger le firmware.

### RUN OTA-3 — Téléchargement en flux

- télécharger un binaire volumineux ;
- calculer SHA-256 ;
- mesurer mémoire, stabilité et débit ;
- ne pas écrire dans une partition de boot à ce stade.

### RUN OTA-4 — Installation contrôlée

- écrire dans la partition inactive ;
- traiter les erreurs et coupures ;
- redémarrer uniquement après validation complète.

### RUN OTA-5 — Boot sain et rollback

- définir les critères de santé ;
- confirmer la nouvelle version ;
- provoquer volontairement des échecs ;
- valider le retour automatique.

### RUN OTA-6 — Interface et exploitation

- version installée et disponible ;
- bouton de vérification ;
- bouton d’installation ;
- historique de la dernière tentative ;
- blocage pendant un cycle actif.

### RUN OTA-7 — Publication GitHub Releases

- procédure reproductible de génération ;
- manifeste automatique ;
- SHA-256 ;
- règles stable/bêta ;
- documentation de publication et de retrait d’une version.

### RUN OTA-8 — Déclenchement distant sécurisé

- à réaliser uniquement après validation complète des étapes précédentes ;
- méthode d’authentification à définir ;
- aucune ouverture de port entrant sur le réseau du module ;
- commande distante sortante ou interrogée périodiquement.

## 14. Critères d’acceptation globaux

La fonction sera considérée comme exploitable lorsque :

- le module détecte une nouvelle version publiée sur GitHub ;
- le téléchargement HTTPS réussit après plusieurs heures de fonctionnement ;
- le firmware est vérifié avant installation ;
- une coupure réseau ne rend pas le module inutilisable ;
- une coupure électrique pendant le processus ne détruit pas la version active ;
- la configuration et les programmes sont conservés ;
- un firmware volontairement défectueux provoque un rollback ;
- aucun relais n’est activé intempestivement ;
- aucun cycle actif n’est interrompu silencieusement ;
- le tactile et le portail captif restent fonctionnels ;
- le résultat est visible localement même sans notification.

## 15. Première action technique recommandée

La première action n’est pas l’implémentation de l’installation OTA.

Elle consiste à inspecter le dépôt réel et le firmware actuel afin de produire un état factuel :

- table de partitions active ;
- taille du binaire ;
- occupation flash ;
- version du framework ESP32 ;
- configuration TLS ;
- points d’allocation mémoire ;
- plus grand bloc contigu avant et après initialisation complète ;
- possibilité réelle d’un schéma OTA double partition.

Ce diagnostic constituera le checkpoint `OTA-0` et décidera si l’OTA directe depuis GitHub est viable sur la carte principale ou si un relais intermédiaire est nécessaire.
