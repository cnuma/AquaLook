# Architecture système AquaLook

## 1. Objet du document

Ce document formalise la vision d’architecture globale d’AquaLook. Il décrit les responsabilités des composants, leurs interactions, les invariants à préserver et la trajectoire d’évolution du produit.

Il complète les documents d’architecture spécialisés existants. En cas de divergence, les invariants de sécurité et d’autonomie locale définis ici doivent être préservés, puis la divergence doit être explicitement arbitrée et documentée.

## 2. Vision produit

AquaLook n’est plus seulement un programmateur d’arrosage isolé. Le projet évolue vers une plateforme de pilotage d’équipements et de supervision IoT, dont l’arrosage constitue la première spécialisation.

La plateforme doit pouvoir intégrer progressivement :

- électrovannes et pompes ;
- extensions de relais ;
- débitmètres et mesure de consommation d’eau ;
- sondes d’humidité du sol ;
- capteurs météo et niveaux de cuve ;
- équipements auxiliaires, éclairages et ouvrants de serre ;
- application mobile ;
- notifications ;
- mise à jour distante OTA ;
- supervision multi-modules et multi-sites.

Cette évolution ne doit pas transformer le cloud, l’application mobile ou Internet en dépendance critique du moteur local.

## 3. Architecture cible à trois couches

```text
┌──────────────────────────────────────────────────────────────┐
│ Couche services distants                                    │
│ API HTTP/HTTPS, historique, notifications, supervision, OTA │
│ (MQTT en extension differee, voir §5.0)                      │
└───────────────────────────────▲──────────────────────────────┘
                                │ HTTPS (jeton porteur), et MQTT/TLS si reconsidere
┌───────────────────────────────┴──────────────────────────────┐
│ Couche applications                                         │
│ Flutter iOS/Android, interface Web et outils d’administration│
└───────────────────────────────▲──────────────────────────────┘
                                │ commandes et consultations
┌───────────────────────────────┴──────────────────────────────┐
│ Couche terrain autonome                                     │
│ ESP32 AquaLook, écran local, planificateur, relais, capteurs │
└──────────────────────────────────────────────────────────────┘
```

Les trois couches doivent pouvoir évoluer indépendamment grâce à des contrats d’interface stables, versionnés et documentés.

## 4. Responsabilités par couche

### 4.1 ESP32 AquaLook — autorité locale et temps réel

Le module AquaLook reste l’autorité opérationnelle sur le terrain. Il assure notamment :

- le planificateur et les décisions d’arrosage locales ;
- la commande sûre des relais et équipements ;
- la gestion des cycles en cours ;
- les protections hydrauliques et matérielles ;
- le fonctionnement de l’écran et de l’interface locale ;
- la persistance de la configuration nécessaire au fonctionnement autonome ;
- le mode dégradé sans Internet, sans cloud et, dans les limites définies, sans carte SD ;
- la validation de toute commande reçue à distance ;
- la publication d’états, d’événements et d’acquittements ;
- le téléchargement et l’installation OTA selon une procédure contrôlée et réversible.

Le module ne doit jamais exécuter aveuglément une commande reçue du cloud ou de l’application. Toute commande distante traverse les mêmes règles de sécurité et d’autorité que les commandes locales.

### 4.2 Application Flutter — expérience utilisateur mobile

L’application mobile AquaLook doit être développée avec Flutter afin de partager une base de code entre iOS et Android.

Elle assure principalement :

- l’affichage des états reçus par MQTT ou API ;
- la consultation des zones, programmes, événements et diagnostics ;
- l’émission de demandes de commande vers AquaLook ;
- l’affichage des acquittements, refus et erreurs ;
- la réception et la présentation des notifications ;
- la gestion de plusieurs modules ou sites lorsque cette fonction sera introduite ;
- l’accès aux services distants sans reproduire le moteur métier critique de l’ESP32.

L’application ne constitue pas l’autorité d’exécution. Elle demande une action ; le module décide si cette action est autorisée et réalisable.

### 4.3 Services distants — communication, supervision et historique

La couche distante fournit progressivement :

- une API HTTP/HTTPS sécurisée, à jeton porteur (un broker MQTT peut s'y ajouter plus tard, voir §5.0) ;
- le routage des états, événements, commandes et acquittements ;
- l’historisation des données ;
- l’authentification des utilisateurs et des modules ;
- la supervision des installations ;
- les notifications ;
- les API nécessaires à l’application et aux outils d’administration ;
- les services de gestion de flotte et de déploiement OTA.

Le cloud transporte, conserve et présente les informations. Il ne remplace pas le planificateur local ni les sécurités du module.

## 5. Trajectoire cloud validée

### 5.0 Arbitrage du 18 août 2026 — HTTP/HTTPS d'abord, MQTT différé

Divergence explicitement arbitrée et documentée, conformément à la règle du §1.

**Constat matériel** : les mesures du 16 et du 18 août 2026 (voir `docs/engineering/38_MEMORY_MANAGEMENT.md` et `docs/architecture/CLOUD_ENVIRONMENT_EVALUATION.md`) montrent qu'une connexion TLS même **ponctuelle** n'a aucune taille d'arène mémoire dédiée qui tienne sur l'ESP32 actuel (sans PSRAM) sans dégrader le démarrage WiFi. MQTT exige une connexion TLS **permanente** : structurellement plus coûteuse, et non couverte par les atténuations déjà en place (libération temporaire des tampons d'écran, inapplicable à une connexion tenue en continu).

**Constat d'usage** : le besoin exprimé (télémétrie de supervision prédictive, réglages poussés à distance) ne requiert pas de poussée temps réel. Un module unique, pas encore une flotte, réduit aussi l'intérêt immédiat du pub/sub MQTT.

**Décision** : le transport par défaut pour la couche services distants devient **HTTP/HTTPS avec jeton porteur**, à connexions courtes (connecte → échange → ferme), sur le modèle déjà éprouvé en production par l'OTA et les notifications. Le module reste toujours l'initiateur (aucune connexion entrante acceptée), par sondage périodique — voir §6.1 révisé.

**MQTT n'est pas abandonné, il est différé** : à reconsidérer si (a) une carte à PSRAM change le budget mémoire disponible, et/ou (b) un besoin réel de commande temps réel ou de flotte multi-modules apparaît. La section 5.1 ci-dessous reste documentée à ce titre, comme hypothèse ouverte plutôt que comme trajectoire validée.

Conséquence directe : la couche services distants **peut** démarrer sur un hébergement Web simple (un script exécuté par requête suffit), sans processus permanent à faire tourner — voir `docs/architecture/CLOUD_ENVIRONMENT_EVALUATION.md` pour le détail. Le mini PC / VPS restent pertinents pour la suite (historisation, tableaux de bord), mais ne sont plus un préalable obligé.

### 5.1 Piste différée — HiveMQ Cloud et MQTT

Conservé comme hypothèse ouverte, non comme trajectoire validée (voir §5.0). Si reconsidéré : HiveMQ Cloud comme broker MQTT de développement, pour valider la connexion MQTT/TLS, la publication d'états/événements, la réception de commandes, les acquittements, la reconnexion après coupure, la limitation de fréquence et de volume, l'intégration Flutter. Resterait un prototype contrôlé, topics et formats conçus pour ne pas dépendre durablement d'un fournisseur particulier.

### 5.2 Application mobile — Flutter

Une application Flutter doit être construite progressivement, sur la base des échanges HTTP/HTTPS validés (ou MQTT si cette piste est reconsidérée) :

1. tableau de bord en lecture seule ;
2. affichage des états (sondage périodique) ;
3. consultation des événements et diagnostics ;
4. émission de commandes non critiques ;
5. commandes d’équipements avec acquittement explicite ;
6. notifications et gestion multi-modules ;
7. intégration contrôlée des opérations OTA autorisées.

### 5.3 Migration vers une infrastructure dédiée

Après validation fonctionnelle et mesure des besoins, l’infrastructure doit pouvoir migrer vers un hébergement dédié maîtrisé (VPS ou équivalent).

La première cible envisagée est :

- une API AquaLook en HTTP/HTTPS (un broker MQTT peut s'y ajouter plus tard si §5.1 est reconsidérée) ;
- Node-RED pour les scénarios de test, diagnostics et intégrations ;
- une base de données adaptée à l’historique ;
- un service de notifications ;
- supervision, sauvegardes et journalisation centralisée.

La migration ne doit pas imposer de réécriture du firmware ou de l’application. Les paramètres de connexion et secrets peuvent changer, mais les contrats d'API doivent rester compatibles ou être versionnés.

## 6. Principes de communication

### 6.1 HTTP/HTTPS — transport distant privilégié (depuis le 18 août 2026, voir §5.0)

HTTP/HTTPS à jeton porteur, en connexions courtes initiées par le module (jamais de connexion entrante acceptée), est le transport privilégié pour :

- les états et événements, remontés par sondage périodique ou en fin de cycle ;
- les demandes de commande et réglages, déposés côté serveur puis récupérés au sondage suivant du module ;
- les acquittements ;
- certaines notifications techniques ;
- l’interface Web locale, les API locales ;
- le téléchargement OTA depuis GitHub Releases ou un relais autorisé ;
- l’administration et la récupération.

Aucune poussée temps réel : la fraîcheur des commandes distantes dépend de l'intervalle de sondage, ajustable indépendamment du reste (plus resserré pour une commande, plus large pour la télémétrie). Le contrôle local (WiFi domestique) reste instantané, inchangé par cette décision.

Les messages doivent être versionnés, bornés, validés et traçables — même exigence qu'aurait imposée MQTT.

### 6.2 MQTT — extension différée

Non retenu dans la trajectoire actuelle (voir §5.0 pour l'arbitrage et ses raisons). Resterait pertinent si un besoin réel de commande temps réel ou de supervision multi-modules à grande échelle apparaissait, et si une évolution matérielle (PSRAM) lève la contrainte mémoire qui a motivé ce report.

MQTT, si reconsidéré, ne devra pas contenir la logique métier critique. Les messages devront être versionnés, bornés, validés et traçables.

### 6.3 Contrats stables

Chaque interface distante doit définir au minimum :

- version du protocole ;
- identifiant unique du module ;
- identifiant de corrélation des commandes ;
- horodatage ;
- type de message ;
- charge utile bornée ;
- état d’acceptation, de refus ou d’échec ;
- règles de compatibilité ascendante et descendante.

## 7. Sécurité et autorité

Les exigences minimales sont :

- chiffrement TLS ;
- identifiants propres à chaque module ;
- révocation et renouvellement des secrets ;
- droits d’accès limités aux ressources nécessaires (routes API, ou topics si MQTT est reconsidéré) ;
- absence de secret administrateur global dans le firmware ;
- validation stricte de chaque message ;
- protection contre le rejeu de commandes ;
- traçabilité de l’émetteur et du résultat ;
- limitation de fréquence ;
- refus sûr en cas de message incomplet, invalide ou incompatible.

Les commandes critiques doivent être explicites, acquittées et, lorsque nécessaire, soumises à des conditions supplémentaires : absence de cycle incompatible, mode d’autorité autorisé, durée maximale, état matériel sain et utilisateur habilité.

## 8. Résilience et fonctionnement hors ligne

AquaLook doit continuer à fonctionner normalement lorsque :

- Internet est indisponible ;
- l’API distante (ou le broker MQTT, si reconsidéré) est inaccessible ;
- l’application mobile est fermée ;
- l’hébergement distant est en maintenance ;
- une notification ne peut pas être envoyée ;
- une synchronisation distante échoue.

Les données importantes non transmises peuvent être mises en attente dans une file locale bornée et synchronisées ultérieurement. La saturation de cette file ne doit jamais bloquer le planificateur ni la commande des équipements.

## 9. Invariants d’architecture

1. Le planificateur local et les sécurités de commande restent opérationnels sans Internet.
2. Le cloud ne commande jamais directement un relais ; il transmet une demande que l’ESP32 valide.
3. Une perte de cloud ne doit ni interrompre ni modifier silencieusement un cycle local.
4. L’application Flutter ne contient pas l’autorité métier critique.
5. Le transport distant (HTTP/HTTPS aujourd’hui, MQTT si reconsidéré — voir §5.0) transporte des messages ; il ne devient pas le moteur d’arrosage.
6. Chaque commande distante produit un acquittement explicite ou expire sans exécution.
7. Les interfaces distantes (API HTTP, et MQTT le cas échéant) sont versionnées et découplées du fournisseur cloud.
8. Toute migration d’hébergement doit être possible sans remise en cause du moteur local.
9. L’OTA reste indépendante de la disponibilité des notifications et de tout service cloud.
10. Aucun nouveau service distant ne doit créer un point de défaillance unique pour le fonctionnement local.

## 10. Documents spécialisés à maintenir

La vision globale doit être complétée progressivement par des documents spécialisés, sans dupliquer inutilement les informations :

- architecture et protocole MQTT ;
- architecture de l’application mobile Flutter ;
- architecture cloud et exploitation du VPS ;
- architecture de sécurité ;
- architecture OTA ;
- contrats API et schémas de messages ;
- stratégie d’observabilité et d’historisation.

Ces documents devront distinguer clairement les choix validés, les hypothèses, les points à expérimenter et les décisions encore ouvertes.
