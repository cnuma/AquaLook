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
│ MQTT, API, historique, notifications, supervision, OTA      │
└───────────────────────────────▲──────────────────────────────┘
                                │ MQTT/TLS et HTTPS
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

- un broker MQTT sécurisé ;
- le routage des états, événements, commandes et acquittements ;
- l’historisation des données ;
- l’authentification des utilisateurs et des modules ;
- la supervision des installations ;
- les notifications ;
- les API nécessaires à l’application et aux outils d’administration ;
- les services de gestion de flotte et de déploiement OTA.

Le cloud transporte, conserve et présente les informations. Il ne remplace pas le planificateur local ni les sécurités du module.

## 5. Trajectoire cloud validée

### 5.1 Phase de validation — HiveMQ Cloud

HiveMQ Cloud est retenu comme broker MQTT de développement pour valider rapidement :

- la connexion MQTT/TLS de l’ESP32 ;
- la publication des états et événements ;
- la réception de commandes distantes ;
- les acquittements et la corrélation requête/réponse ;
- la reconnexion après coupure ;
- la limitation de fréquence et de volume ;
- l’intégration Flutter.

Cette étape doit rester un prototype contrôlé. Les topics, formats de messages et règles de sécurité doivent être conçus pour ne pas dépendre durablement d’un fournisseur particulier.

### 5.2 Application mobile — Flutter

Après validation des échanges MQTT, une application Flutter doit être construite progressivement :

1. tableau de bord en lecture seule ;
2. affichage temps réel des états ;
3. consultation des événements et diagnostics ;
4. émission de commandes non critiques ;
5. commandes d’équipements avec acquittement explicite ;
6. notifications et gestion multi-modules ;
7. intégration contrôlée des opérations OTA autorisées.

### 5.3 Migration vers une infrastructure OVHcloud

Après validation fonctionnelle et mesure des besoins, l’infrastructure doit pouvoir migrer vers un VPS OVHcloud maîtrisé.

La première cible envisagée est :

- Mosquitto comme broker MQTT ;
- Node-RED pour les scénarios de test, diagnostics et intégrations ;
- une base de données adaptée à l’historique ;
- une API AquaLook ;
- un service de notifications ;
- supervision, sauvegardes et journalisation centralisée.

La migration ne doit pas imposer de réécriture du firmware ou de l’application. Les paramètres de connexion et certificats peuvent changer, mais les contrats MQTT et API doivent rester compatibles ou être versionnés.

## 6. Principes de communication

### 6.1 MQTT

MQTT est le transport privilégié pour :

- les états temps réel ;
- les événements ;
- les demandes de commande ;
- les acquittements ;
- la présence et la disponibilité des modules ;
- certaines notifications techniques.

MQTT ne doit pas contenir la logique métier critique. Les messages doivent être versionnés, bornés, validés et traçables.

### 6.2 HTTP/HTTPS

HTTP reste pertinent pour :

- l’interface Web locale ;
- les API locales ;
- le téléchargement OTA depuis GitHub Releases ou un relais autorisé ;
- certains échanges cloud non temps réel ;
- l’administration et la récupération.

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
- droits MQTT limités aux topics nécessaires ;
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
- le broker MQTT est inaccessible ;
- l’application mobile est fermée ;
- le VPS est en maintenance ;
- une notification ne peut pas être envoyée ;
- une synchronisation distante échoue.

Les données importantes non transmises peuvent être mises en attente dans une file locale bornée et synchronisées ultérieurement. La saturation de cette file ne doit jamais bloquer le planificateur ni la commande des équipements.

## 9. Invariants d’architecture

1. Le planificateur local et les sécurités de commande restent opérationnels sans Internet.
2. Le cloud ne commande jamais directement un relais ; il transmet une demande que l’ESP32 valide.
3. Une perte de cloud ne doit ni interrompre ni modifier silencieusement un cycle local.
4. L’application Flutter ne contient pas l’autorité métier critique.
5. MQTT transporte des messages ; il ne devient pas le moteur d’arrosage.
6. Chaque commande distante produit un acquittement explicite ou expire sans exécution.
7. Les interfaces MQTT et API sont versionnées et découplées du fournisseur cloud.
8. La migration HiveMQ vers OVHcloud doit être possible sans remise en cause du moteur local.
9. L’OTA reste indépendante de la disponibilité des notifications et du broker MQTT.
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
