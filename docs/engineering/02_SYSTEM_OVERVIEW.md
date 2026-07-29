# AquaLook — Vue technique globale

## 1. Architecture fonctionnelle

```text
Utilisateur local
   ├─ écran tactile
   └─ navigateur Web
          │
          v
Configuration / commandes manuelles
          │
          v
Moteur de décision et planification
          │
          v
Orchestration / règles de sécurité
          │
          v
Abstraction des équipements
          │
          v
Cartes relais, vannes, pompe et extensions
```

Les services distants futurs — MQTT, application Flutter, VPS, notifications et OTA — s'ajoutent autour de ce noyau sans devenir une dépendance du fonctionnement local.

## 2. Architecture logicielle embarquée

Le firmware est organisé autour de managers spécialisés initialisés depuis `main.cpp`.

- `ConfigManager` : chargement, validation et persistance de la configuration ;
- `ScheduleManager` : calcul des occurrences et suivi des cycles ;
- `RelaisManager` : abstraction des sorties et sécurité de durée ;
- `WiFiManager` : station, reconnexion et portail captif ;
- `NTPManager` : synchronisation temporelle ;
- `WeatherManager` : récupération et interprétation météo ;
- `WebManager` : serveur HTTP asynchrone, API et ressources ;
- `DisplayManager` : rendu TFT, navigation et tactile ;
- `ScreenManager` : veille et état écran ;
- `EventBus` : invalidations et signaux transverses limités ;
- `EventLog` : journal système en mémoire et extensions de persistance ;
- couches V4 : domaine équipements, orchestrateur, plans d'exécution et runtime shadow.

## 3. Séquence de démarrage

L'ordre exact dépend du checkpoint actif, mais les dépendances structurantes sont les suivantes :

1. initialisation série et diagnostics de base ;
2. initialisation des bus matériels ;
3. chargement de la configuration persistée ;
4. initialisation de l'écran et du splash ;
5. mise en sécurité et initialisation des relais ;
6. initialisation du planificateur ;
7. connexion Wi-Fi ou activation du mode de secours ;
8. synchronisation de l'heure lorsque disponible ;
9. démarrage du serveur Web ;
10. montage et vérification du stockage SD ;
11. activation des services météo, notifications et fonctions distantes selon configuration ;
12. rendu complet et activation du tactile.

La configuration doit être disponible avant les managers qui la consomment. Le chemin matériel doit être câblé avant que le planificateur puisse demander une activation.

## 4. Boucle d'exécution

Les traitements doivent rester non bloquants. La boucle ou les tâches associées assurent :

- entretien de la connexion Wi-Fi ;
- synchronisation NTP périodique ;
- mise à jour météo ;
- évaluation du planning ;
- surveillance des durées maximales ;
- traitement du serveur Web ;
- maintenance SD ;
- rendu et tactile ;
- collecte des métriques et événements.

Les appels réseau, écritures de stockage et rendus lourds ne doivent pas empêcher la surveillance des relais.

## 5. Modèle de données et persistance

### NVS

La configuration active est persistée dans le namespace `aqualook`, sous forme de structure binaire versionnée avec marqueur, taille, schéma et CRC32. Toute extension doit prévoir :

- un nouveau numéro de schéma ;
- des valeurs par défaut sûres ;
- une migration depuis les versions supportées ;
- un comportement explicite si la migration échoue.

### LittleFS et SD

LittleFS conserve les ressources minimales indispensables au démarrage, au portail captif et au secours. La SD héberge les ressources déplaçables, historiques et contenus volumineux. Le résolveur de ressources doit pouvoir retomber vers la copie minimale embarquée.

### Journaux

Le journal système doit distinguer au minimum information, avertissement, erreur et événements de sécurité. Les événements critiques doivent être conservables à travers un redémarrage sans provoquer une usure excessive de la flash.

## 6. Matériel

### Contrôleur principal

L'ESP32 CYD fournit calcul, Wi-Fi, écran, tactile et lecteur SD. L'absence de PSRAM impose une attention forte aux allocations dynamiques, aux sprites graphiques et aux tampons TLS.

### Relais

Le contrôleur XL9535 actuel utilise l'I2C avec logique directe. La cible d'architecture autorise plusieurs cartes, plusieurs contrôleurs et un mapping explicite entre zone logique et voie physique.

### Extensions

Le MCP23017 peut étendre les E/S pour relais ou signaux lents. Les débitmètres à impulsions peuvent nécessiter un microcontrôleur secondaire, par exemple un Lolin S2 Mini, afin de garantir le comptage sans surcharger le contrôleur principal.

## 7. Interfaces

### Web local

L'interface Web fournit consultation, configuration, commandes manuelles, diagnostics et récupération. Les identifiants HTML et contrats JSON sont des interfaces stables. Le verrouillage visuel historique ne constitue pas une authentification forte.

### MQTT futur

Le contrat MQTT devra séparer télémétrie, état désiré, état confirmé, événements et commandes. Chaque appareil doit disposer d'une identité distincte et d'ACL limitées à ses topics.

### OTA

Le module télécharge un manifeste et un firmware, vérifie compatibilité, version, intégrité et signature, puis utilise les partitions doubles. Un démarrage non confirmé doit permettre le rollback.

### Application Flutter

L'application mobile consomme l'état publié et émet des demandes. Elle ne doit jamais présenter une commande comme exécutée avant confirmation du module.

## 8. Sécurité fonctionnelle

- état sûr au démarrage : sorties désactivées ;
- durée maximale indépendante du planning ;
- validation du mapping avant toute commande ;
- refus sûr en cas de configuration invalide ;
- aucune activation sur perte de communication ;
- reprise après redémarrage définie explicitement ;
- journalisation des commandes locales et distantes.

## 9. Cybersécurité

Les surfaces d'attaque comprennent le point d'accès, le serveur Web, MQTT, OTA, GitHub, le VPS, l'application mobile, la SD et la chaîne de développement. Les contrôles prioritaires sont : authentification, autorisation, TLS, secrets hors Git, signature des firmwares, limitation des tentatives, journaux de sécurité et révocation des identités.

## 10. Observabilité

Les métriques recommandées sont : heap libre, plus grand bloc contigu, fragmentation, uptime, RSSI, latence, erreurs réseau, disponibilité SD, taille des journaux, cycles exécutés, commandes refusées et resets. Les diagnostics doivent distinguer problème réseau, problème TLS, saturation mémoire, défaut matériel et configuration invalide.

## 11. Validation

Une évolution n'est pas validée par la compilation seule. Elle doit démontrer la chaîne complète : inclusion, instanciation, appel, déclencheur, conditions, effet observable et comportement de repli. Les modifications matérielles exigent un test sur banc avec une seule zone et une durée courte avant généralisation.
