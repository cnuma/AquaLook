# Roadmap AquaLook

Ce document regroupe les évolutions envisagées pour AquaLook. Il ne constitue pas un engagement de développement immédiat ; chaque point devra être détaillé, priorisé et traité sur une branche dédiée.

## Évolutions futures

### Migration des ressources Web vers la carte SD — préalable à l’OTA

**Statut : réalisé et validé.** Les ressources Web sont servies depuis la carte SD par `SdStaticHandler`, avec repli LittleFS pour les ressources indispensables au démarrage et à la récupération. Validé sur matériel le 8 août 2026 (`docs/checkpoints/CHECKPOINT_2026-08-08_RECOVERY_PRE_OTA_WIFI_NVS.md`) et reconfirmé le 13 août 2026 pendant la campagne de validation OTA (`Stockage: ressources Web SD validees dans /www` observé en boot normal comme après bascule OTA). Le contenu ci-dessous reste la référence des objectifs et contraintes qui ont guidé cette réalisation.

Utiliser la carte SD pour stocker les pages et ressources Web qui se trouvent actuellement en flash ou dans LittleFS et qui peuvent être déplacées sans compromettre le démarrage, la configuration initiale ou la récupération du module.

Cette activité doit être réalisée et validée avant la mise en place de la mise à jour OTA.

Objectifs fonctionnels :

- réduire l’occupation de la flash et de la partition LittleFS ;
- déplacer vers la carte SD les pages HTML, feuilles de style, scripts, images et autres ressources non indispensables au démarrage minimal ;
- conserver en flash ou dans LittleFS les ressources nécessaires à la première configuration, au portail captif, au diagnostic minimal et aux fonctions de récupération ;
- permettre au serveur Web de servir les ressources depuis la carte SD de manière transparente ;
- disposer d’un mode dégradé clair lorsque la carte SD est absente, non montée ou illisible ;
- préparer une architecture de stockage stable avant de définir le périmètre et les partitions nécessaires à l’OTA.

Ressources à conserver impérativement en flash ou dans LittleFS :

- portail captif et pages de première configuration Wi-Fi ;
- page ou interface minimale de diagnostic et de récupération ;
- ressources nécessaires au démarrage du serveur Web en mode secours ;
- pages permettant de détecter, signaler ou corriger une carte SD absente ou défaillante ;
- toute ressource indispensable à une future procédure OTA ou de restauration ;
- fichiers dont l’absence empêcherait l’accès administratif minimal au module.

Ressources candidates à la migration vers la carte SD :

- pages d’état et de supervision détaillées ;
- pages de programmation et de paramétrage non nécessaires à la première mise en service ;
- feuilles CSS et scripts JavaScript associés à ces pages ;
- images, icônes, historiques, aides et contenus volumineux ;
- ressources futures liées aux graphiques, consommations, sondes et statistiques.

Points d’architecture à étudier :

- inventaire précis des fichiers actuellement embarqués dans LittleFS et de leurs dépendances ;
- classement de chaque ressource selon trois niveaux : indispensable au démarrage, nécessaire au secours, déplaçable sur SD ;
- gestion centralisée du montage, de la disponibilité et des erreurs de la carte SD ;
- résolution des chemins et priorité de recherche entre flash, LittleFS et carte SD ;
- stratégie de repli lorsqu’un fichier attendu sur SD est absent ou corrompu ;
- vérification de l’intégrité et de la version des ressources présentes sur la carte ;
- mécanisme de déploiement et de mise à jour du contenu de la carte SD ;
- compatibilité avec le bus matériel existant, notamment l’écran tactile et les autres périphériques SPI ;
- performances de lecture, concurrence d’accès et absence de blocage du planificateur ;
- impacts sur le partitionnement flash avant l’introduction de l’OTA ;
- définition du comportement lors d’un retrait ou d’une défaillance de la carte pendant le fonctionnement.

Ordre de réalisation imposé :

1. inventorier et classifier les ressources actuelles ;
2. mettre en place l’accès SD et le mode de repli ;
3. migrer progressivement les ressources éligibles ;
4. valider le démarrage, le portail captif et le mode secours sans carte SD ;
5. mesurer l’espace flash et LittleFS libéré ;
6. seulement ensuite concevoir et intégrer la mise à jour OTA.

Invariant impératif : l’absence, le retrait ou la corruption de la carte SD ne doit jamais empêcher le démarrage du programmateur, l’exécution locale des cycles, l’accès à la première configuration ni l’utilisation d’une interface minimale de diagnostic et de récupération.

### Mise à jour distante du firmware par GitHub Releases

**Statut : mécanique cœur réalisée et validée sur matériel le 13 août 2026.** Cycle complet observé de bout en bout sur `esp32-2432S028` : vérification de version depuis le manifeste GitHub Releases, téléchargement HTTPS avec vérification SHA-256, écriture en partition OTA inactive, activation (`esp_ota_set_boot_partition`) et redémarrage, garde de retour arrière applicative (validation après 45 s de fonctionnement stable, retour automatique après 3 tentatives sans validation), configuration NVS préservée à travers la bascule. Interface locale (`/ota`) et journalisation de chaque étape en place.

Restent non réalisés, dans cette section : signature numérique du firmware au-delà du seul SHA-256, notifications de mise à jour (ntfy ou autre), déclenchement distant autorisé, limitation de fréquence des vérifications, et tests de coupure réseau/alimentation pendant un cycle OTA en cours. Le contenu ci-dessous reste la référence d'architecture pour ces points restants.

Permettre la mise à jour d’un module AquaLook à distance, sans présence physique à proximité du programmateur et sans connexion au même réseau local, en utilisant GitHub Releases comme source officielle des firmwares OTA.

Objectifs fonctionnels :

- publier les firmwares validés sous forme de fichiers binaires dans une GitHub Release ;
- publier avec chaque version un manifeste décrivant au minimum la version, la compatibilité matérielle, la taille, l’URL de téléchargement et l’empreinte SHA-256 du firmware ;
- permettre au module de vérifier manuellement ou périodiquement si une version plus récente est disponible ;
- télécharger la mise à jour par une connexion HTTPS sortante initiée par le module ;
- vérifier l’intégrité et l’authenticité du firmware avant toute installation ;
- installer le firmware dans la partition OTA inactive puis redémarrer sur la nouvelle version ;
- conserver une possibilité de retour automatique à la version précédente lorsque le nouveau firmware ne confirme pas un démarrage sain ;
- afficher dans l’interface la version installée, la version disponible, la date de la dernière vérification et le résultat de la dernière tentative ;
- permettre une vérification manuelle et, après validation de la stratégie de sécurité, une installation déclenchée depuis l’interface locale ou un mécanisme distant autorisé ;
- journaliser toutes les étapes : découverte, téléchargement, validation, installation, redémarrage, confirmation ou retour arrière.

GitHub comme infrastructure OTA :

- GitHub Releases constitue la source officielle des versions publiées ;
- une release ne doit être utilisée par les modules que lorsqu’elle est explicitement marquée comme compatible et déployable ;
- les versions de développement, préversions et binaires non validés doivent être ignorés par défaut ;
- le manifeste OTA doit permettre de distinguer les variantes matérielles, les schémas de partitions et les versions minimales compatibles ;
- aucune clé GitHub disposant de droits d’écriture ne doit être stockée dans le module ;
- l’accès à un dépôt privé, s’il est retenu, devra utiliser un mécanisme dédié qui n’expose pas durablement un jeton personnel dans le firmware ;
- la stratégie finale devra décider entre dépôt public pour les seuls binaires publiés, dépôt privé avec relais sécurisé, ou serveur intermédiaire alimenté depuis GitHub Releases.

Notifications associées — méthode à valider :

- prévoir une abstraction de notification indépendante du fournisseur retenu ;
- notifier au minimum la disponibilité d’une nouvelle version, le début de la mise à jour, la réussite, l’échec et un éventuel retour arrière ;
- étudier ntfy comme première solution, en tenant compte du diagnostic TLS et de la fragmentation mémoire déjà observés sur la carte principale ;
- comparer ntfy direct depuis le module, passerelle ESP32-S2, relais local, MQTT, Home Assistant ou service cloud intermédiaire ;
- ne pas rendre la réussite de la mise à jour dépendante de la disponibilité du service de notification ;
- conserver localement le résultat et le motif détaillé lorsque la notification ne peut pas être envoyée.

Points d’architecture à étudier :

- table de partitions compatible avec deux emplacements OTA, NVS et les besoins résiduels de LittleFS ;
- taille maximale du firmware après migration des ressources Web vers la carte SD ;
- capacité réelle de la carte à effectuer durablement les connexions HTTPS nécessaires avec une mémoire fragmentée ;
- validation cryptographique du manifeste et du firmware, avec signature numérique à privilégier au-delà du seul contrôle SHA-256 ;
- gestion des certificats racine, de leur expiration et de leur renouvellement ;
- stratégie de déploiement progressif, de canal stable ou bêta et de blocage d’une version défectueuse ;
- définition d’un état de démarrage sain avant confirmation définitive du nouveau firmware ;
- conservation des configurations NVS, des programmes, des journaux et des données présentes sur la carte SD ;
- compatibilité ascendante et migration contrôlée des structures de données persistantes ;
- comportement lorsque la carte SD est absente, le réseau instable, le téléchargement interrompu ou l’alimentation coupée ;
- politique concernant les cycles d’arrosage en cours : interdiction, report ou fenêtre de maintenance explicite ;
- mécanisme de récupération local lorsque plusieurs démarrages du nouveau firmware échouent ;
- limitation de fréquence des vérifications afin de ne pas perturber le planificateur ni surcharger GitHub.

Ordre de réalisation proposé :

1. valider l’occupation flash après migration des ressources Web vers la carte SD ;
2. définir et tester une table de partitions OTA compatible avec le matériel ;
3. définir le format du manifeste et la chaîne de publication GitHub Releases ;
4. implémenter la vérification de version et le téléchargement sans installation ;
5. ajouter les contrôles d’intégrité, de signature et de compatibilité ;
6. intégrer l’installation OTA, la confirmation de démarrage et le rollback ;
7. ajouter l’interface locale, les journaux et les commandes autorisées ;
8. valider la méthode de notification puis l’intégrer sans couplage fort ;
9. tester les coupures réseau et électriques, les images invalides et les retours arrière ;
10. seulement après validation, autoriser un déclenchement distant contrôlé.

Invariant impératif : aucune mise à jour ne doit pouvoir activer ou désactiver une voie de manière intempestive, interrompre silencieusement un cycle en cours, effacer la configuration ou rendre le module irrécupérable. Le module doit rester fonctionnel sur la version précédente tant que la nouvelle version n’a pas été téléchargée, vérifiée, démarrée et explicitement confirmée comme saine. Les notifications restent informatives et ne doivent jamais constituer une dépendance critique du processus OTA.

### Mise à jour distante des ressources Web — canal séparé de l’OTA firmware

**Statut : bloquant avant mise en production, chantier démarré le 16 août 2026.** À ce jour, `data/` (pages HTML, `app.js`, CSS) n’est déployé sur la carte SD que par synchronisation locale (`tools/sync-sd-assets.ps1`), qui suppose un accès physique à la carte SD (retrait, lecteur sur un PC ayant le dépôt, réinsertion). Cette manipulation est tenable pendant le développement, avec le module à portée de main ; elle ne l’est plus une fois le module installé en usage réel, hors d’atteinte physique facile. C’est donc un prérequis explicite avant tout déploiement en production, pas une simple amélioration de confort. L’OTA firmware ne couvre pas ce besoin : une mise à jour OTA installée ne change jamais ce qui est affiché sur `/`. Un horodatage de synchro (`assets-version.json`, généré par le script local, affiché en pied de page de `/`) permet seulement de constater visuellement quelle version est déployée — pas encore de la déployer à distance.

Contrainte d’architecture centrale, à respecter dans toute conception : le mode maintenance OTA (`MaintenanceSetupWrapper.cpp`, `-Wl,--wrap=_Z5setupv`) exécute les opérations de flash firmware dans une tâche FreeRTOS isolée et volontairement minimale — sans SD, sans écran, sans serveur Web initialisés. C’est un choix délibéré d’isolation pendant une opération risquée (déjà la raison pour laquelle `MaintenanceResultStore` a été conservé en NVS plutôt que sur SD). Un déploiement de ressources Web a nécessairement besoin d’écrire sur la SD : il ne peut donc **pas** être greffé dans ce flux de maintenance existant sans revoir cette isolation. La conception doit passer par un second canal, indépendant, qui s’exécute en fonctionnement normal (WiFi, SD et le reste déjà disponibles comme pour le reste de l’application).

Objectifs fonctionnels :

- publier avec chaque version un manifeste décrivant les ressources Web disponibles : liste des fichiers avec URL de téléchargement et empreinte SHA-256 de chacun (pas d’archive compressée — l’ESP32 n’a pas de décompresseur zip/tar embarqué, et il s’agit d’un nombre restreint de petits fichiers texte, aussi simples à télécharger individuellement) ;
- permettre au module de comparer sa version déployée (voir `assets-version.json`, à faire écrire par le module lui-même après un déploiement réussi plutôt que par le script local) à la version publiée dans le manifeste ;
- télécharger, vérifier le SHA-256 puis écrire chaque fichier modifié sur la carte SD, en réutilisant l’infrastructure TLS/HTTPS déjà validée pour l’OTA firmware ;
- exposer un déclenchement manuel (bouton dédié sur `/ota`, à côté de ceux du firmware) puis, une fois éprouvé, une vérification périodique ;
- journaliser chaque étape comme le fait déjà l’OTA firmware (découverte, téléchargement, écriture, résultat).

Couplage avec l’OTA firmware — décision à confirmer, orientation actuelle : **canal découplé plutôt que fusionné.** L’expérience du 16 août 2026 (nombreux correctifs Web ponctuels dans la même session, aucun n’ayant nécessité de nouveau firmware) va dans le sens d’un cycle de mise à jour des pages Web plus fréquent que celui du firmware. Le tag Git peut rester la référence commune de version (une version = firmware et pages cohérents ensemble), mais les deux installations restent déclenchables indépendamment l’une de l’autre.

Travaux préalables identifiés :

- `StorageManager` n’expose aujourd’hui que de la lecture (`openRead`/`readChunk`/`existsOnSd`) ; une capacité d’écriture symétrique (`openWrite`/`writeChunk`/suppression), protégée par le même mutex que le reste des accès SD, est un prérequis ;
- étendre `tools/generate_ota_manifest.py` (ou créer un script dédié) et `.github/workflows/ota-release.yml` pour publier ce second manifeste avec chaque release ;
- décider du comportement si l’écriture est interrompue en cours de déploiement (coupure réseau ou alimentation) : le module ne doit jamais se retrouver avec un mélange incohérent d’anciens et de nouveaux fichiers qui casserait le site servi.

Ordre de réalisation proposé :

1. capacité d’écriture SD dans `StorageManager`, mutex-protégée comme la lecture existante — **fait le 16 août 2026** ;
2. validation isolée de cette écriture (route de test ou équivalent) avant tout branchement réseau — **fait le 16 août 2026**, via `/api/debug/deploy-file` et un déploiement réel de `logs.html` vérifié octet pour octet (SHA-256 identique) ;
3. format du manifeste des ressources Web et extension de la chaîne de publication GitHub Releases — **fait le 16 août 2026** : `tools/generate_web_manifest.py` (schéma `aqualook-web-manifest-v1`, un fichier par entrée avec URL/taille/SHA-256, même convention que `generate_ota_manifest.py`) et `.github/workflows/ota-release.yml` étendu pour publier chaque fichier de `data/` comme asset de la release et générer `aqualook-web-manifest.json` à côté du manifeste firmware — validé localement (manifeste généré sur les 10 fichiers de `data/`, ~2 Ko, sous le budget de 8 Ko) ; reste à valider en conditions réelles au prochain tag de release ;
4. téléchargement + vérification SHA-256 d’un fichier, sans écriture (symétrique à l’étape « vérification sans installation » de l’OTA firmware) — **code fait le 16 août 2026** (`WebAssetsUpdater::verifyOnly`, `src/WebAssetsUpdater.h/.cpp`, route de test `/api/debug/verify-web-asset`), **mais bloquée en pratique par un constat matériel** : voir ci-dessous ;

**Constat du 16 août 2026 — mémoire contiguë insuffisante pour une poignée de main TLS en fonctionnement normal.** Test sur le module reel : `WebAssetsUpdater::verifyOnly()` appelée en fonctionnement normal (Display + SD + serveur Web deja actifs) contre une vraie URL de release GitHub (`aqualook-manifest.json` de la release v5.9.4, 867 octets) echoue systematiquement a la connexion TLS — `ssl_client.cpp: start_ssl_client(): (-32512) SSL - Memory allocation failed`. `/api/diagnostics` confirme : `heapFree` ~71 Ko mais `heapLargestBlock` seulement ~21,5 Ko, stable sur plusieurs mesures a plusieurs dizaines de secondes d'intervalle (donc pas un pic transitoire lie a la requete HTTP de test elle-meme) — le plus grand bloc contigu disponible ne suffit pas a l'allocation cumulee des tampons mbedTLS (tampons in/out + contexte crypto) pendant la poignee de main. Aucun crash, aucun redemarrage : l'appel echoue proprement et retourne une erreur exploitable, donc rien a corriger en urgence — mais tel quel, le telechargement HTTPS d'un fichier ne peut pas aboutir en fonctionnement normal avec ce framework.

Precisions utiles pour la suite : c'est la toute premiere tentative de connexion HTTPS en fonctionnement normal dans ce projet — `WeatherManager` n'utilise que du HTTP simple (`api.openweathermap.org` en clair), et la seule autre utilisation de `WiFiClientSecure`/`OtaTlsTrust` est `OtaDownloadTest`, qui tourne dans la tache FreeRTOS isolee du mode maintenance (sans SD/Display/Web charges), avec donc beaucoup plus de memoire contigue disponible. `WiFiClientSecure` de ce framework (arduino-esp32 tel que fourni par PlatformIO) n'expose pas de `setBufferSizes()` pour reduire les tampons TLS ; mbedTLS est fourni precompile (`libmbedtls.a`), donc `MBEDTLS_SSL_IN/OUT_CONTENT_LEN` n'est pas ajustable sans reconstruire le framework — hors de portee raisonnable pour ce projet.

Pistes a arbitrer avant de poursuivre l'étape 5 (ecriture) :
- réduire la fragmentation en fonctionnement normal (identifier quels sous-systèmes gardent des gros blocs alloués — Display, AsyncWebServer, SD — et voir si l'un peut libérer temporairement de la mémoire pendant la fenêtre de téléchargement) ;
- isoler temporairement le téléchargement (suspendre le rendu écran et/ou les nouvelles requêtes Web entrantes pendant les quelques secondes de la vérification/téléchargement, sans aller jusqu'à la isolation complète du mode maintenance) ;
- chercher un chemin HTTPS alternatif plus léger en mémoire (à confirmer si `esp_http_client`/`esp_tls` du SDK, potentiellement plus économe que le `ssl_client.cpp` d'Arduino, est accessible sans sortir du framework Arduino fourni).

Aucune de ces pistes n'a été tentée — décision à prendre avant de continuer, plutôt qu'une correction rapide non vérifiée.
5. écriture effective sur SD, avec stratégie explicite de cohérence en cas d’interruption ;
6. interface locale (`/ota`) et journalisation de chaque étape ;
7. vérification périodique, une fois la chaîne manuelle éprouvée sur le terrain.

#### Expérience utilisateur du déclenchement — spécification du 16 août 2026

À ce jour (test du 16 août 2026), le déclenchement d’un déploiement se fait directement depuis ce poste de travail (route de test `/api/debug/deploy-file`, appelée manuellement) : cela valide le mécanisme d’écriture, mais ce n’est pas utilisable par l’utilisateur final. Une fois la chaîne manifeste + téléchargement + vérification en place (étapes 3 à 5 ci-dessus), le déclenchement et la visibilité doivent devenir entièrement pilotables par l’utilisateur, pour les deux canaux (OTA firmware et ressources Web/SD) :

- **notification qu’une mise à jour est en attente**, dès que le module détecte que sa version diffère de celle publiée (comparaison de manifeste), via le canal de notification déjà en service (ntfy) — informative, avant toute action ;
- **LED** : clignotement violet dédié à l’état « mise à jour en attente », distinct des trois autres états déjà fixés (bleu = arrosage en cours, ambre = recherche réseau, rouge clignotant = panne réservée à `FaultManager`) — le violet a déjà été réservé à cet usage lors de la réorganisation des couleurs LED/LCD du 16 août 2026 ;
- **LCD** : icône dédiée reprenant le même violet, sur le même principe que l’icône signal WiFi (`renderSignalSprite()`) déjà utilisée pour l’état « recherche réseau » ;
- **page `/`** : icône ou badge dans la barre du haut signalant une mise à jour en attente, avec lien direct vers `/ota` ;
- **page `/ota`** : lien mis en avant pour accéder facilement à la mise à jour en attente ; bouton de déclenchement manuel du processus complet (téléchargement, vérification, installation) pour le canal concerné (firmware ou SD/Web) ;
- **confirmation de fin de processus** : notification (même canal ntfy) une fois le déploiement terminé, en cas de succès comme d’échec — l’utilisateur ne doit pas avoir à revenir consulter la page pour savoir si ça a fonctionné.

Ce comportement s’applique uniformément aux deux canaux de mise à jour (OTA firmware et SD/Web), avec un vocabulaire visuel et de notification commun, même si le déclenchement et le contenu déployé restent indépendants l’un de l’autre (cf. décision de canal découplé ci-dessus). Prérequis : ce point dépend de l’étape 3 (manifeste) pour savoir qu’une mise à jour existe — tant que cette détection n’existe pas, il n’y a rien à signaler ni à déclencher depuis l’interface.

### Mode autonome sans Internet avec point d’accès Wi-Fi

Permettre au module AquaLook de fonctionner et d’être administré sans box, routeur ni accès Internet en créant son propre point d’accès Wi-Fi auquel l’utilisateur peut se connecter directement.

Objectifs fonctionnels :

- permettre une utilisation complète du programmateur dans un site dépourvu d’accès Internet ou de réseau local ;
- activer un point d’accès Wi-Fi propre au module ;
- permettre à l’utilisateur de se connecter au réseau Wi-Fi AquaLook depuis un téléphone, une tablette ou un ordinateur ;
- ouvrir une interface Web locale pour consulter l’état du module, configurer les zones et les programmes, lancer ou arrêter un arrosage et modifier les paramètres autorisés ;
- conserver les programmes, l’heure locale et les paramètres nécessaires au fonctionnement autonome ;
- rendre le mode autonome utilisable aussi bien lors de la première mise en service qu’après une perte durable du réseau configuré ;
- permettre le retour vers un fonctionnement connecté sans réinitialisation complète du module.

Modes de fonctionnement à prévoir :

- mode station normal : le module rejoint le réseau Wi-Fi configuré ;
- mode point d’accès de secours : le module crée automatiquement son réseau après un nombre défini d’échecs de connexion ;
- mode point d’accès forcé : activation manuelle depuis l’écran, un bouton matériel ou une commande locale ;
- mode simultané AP + station à étudier pour conserver l’accès local direct tout en restant connecté au réseau existant.

Fonctions disponibles en mode autonome :

- consultation de l’état des voies et des cycles ;
- création, modification, activation et suppression des programmes ;
- commande manuelle des zones dans les limites de sécurité existantes ;
- consultation des diagnostics essentiels ;
- configuration du Wi-Fi principal pour préparer un retour en mode connecté ;
- accès aux pages Web stockées en flash, LittleFS ou sur la carte SD selon leur disponibilité ;
- conservation du portail captif ou d’un mécanisme équivalent pour faciliter l’ouverture de l’interface locale.

Points d’architecture à étudier :

- définition du SSID, du mot de passe et de la méthode de génération d’identifiants propres à chaque module ;
- protection contre un point d’accès ouvert ou conservant un mot de passe par défaut connu ;
- déclenchement automatique du mode AP et délai avant bascule ;
- signalisation claire du mode actif sur l’écran et dans l’interface Web ;
- maintien fiable de l’horloge sans NTP, avec RTC éventuelle, dérive acceptable et méthode de remise à l’heure depuis le navigateur ;
- comportement des fonctions météo, cloud et autres services Internet lorsqu’ils sont indisponibles ;
- résolution DNS locale et portail captif pour diriger l’utilisateur vers l’interface AquaLook ;
- coexistence entre serveur Web, DNS captif, planificateur, écran et acquisition des capteurs ;
- procédure sécurisée pour quitter le mode AP et tester une nouvelle configuration Wi-Fi sans perdre l’accès de secours ;
- temporisation ou maintien permanent du point d’accès selon le mode choisi ;
- limites du nombre de clients simultanés et prévention des commandes concurrentes ;
- compatibilité avec le mode dégradé sans carte SD.

Invariant impératif : l’absence d’Internet ou de réseau Wi-Fi externe ne doit jamais empêcher l’exécution des programmes déjà enregistrés ni l’accès local aux fonctions essentielles. Le passage en point d’accès ne doit provoquer ni redémarrages répétés, ni perte de configuration, ni interruption intempestive d’un cycle d’arrosage en cours.

### Écosystème AquaLook — cloud, MQTT et application mobile

Faire évoluer AquaLook vers une architecture à trois couches : module ESP32 autonome, applications clientes et services distants. Le document de référence `docs/architecture/SYSTEM_ARCHITECTURE.md` formalise les responsabilités et invariants de cette architecture.

#### Phase A — validation MQTT avec HiveMQ Cloud

- utiliser HiveMQ Cloud comme broker MQTT de développement ;
- établir une connexion MQTT/TLS sortante depuis AquaLook ;
- définir et versionner l’arborescence des topics ;
- publier les états, événements et diagnostics utiles ;
- recevoir des demandes de commande distantes ;
- ajouter un identifiant de corrélation et un acquittement explicite pour chaque commande ;
- tester la reconnexion, les coupures réseau et la reprise après indisponibilité du broker ;
- limiter le volume, la fréquence et la taille des messages afin de préserver la stabilité de l’ESP32 ;
- rendre les contrats de messages indépendants du fournisseur de broker.

#### Phase B — application mobile Flutter

- développer une base de code unique pour iOS et Android avec Flutter ;
- afficher les états AquaLook en temps réel à partir de MQTT ;
- consulter les zones, programmes, événements et diagnostics ;
- envoyer des demandes de commande vers AquaLook via MQTT ;
- afficher les commandes acceptées, refusées, expirées ou en erreur ;
- recevoir et présenter les notifications ;
- préparer la gestion multi-modules et multi-sites ;
- préparer une intégration OTA contrôlée sans donner à l’application l’autorité directe sur le moteur local.

#### Phase C — migration vers un VPS OVHcloud

Après validation du fonctionnement avec HiveMQ Cloud et mesure des besoins :

- migrer le broker vers un VPS OVHcloud maîtrisé ;
- déployer Mosquitto comme broker MQTT ;
- utiliser Node-RED pour les scénarios de test, les diagnostics et certaines intégrations ;
- ajouter une base de données pour l’historique ;
- préparer une API AquaLook et les services de notifications ;
- mettre en place supervision, sauvegardes, mises à jour de sécurité et journalisation ;
- conserver les mêmes contrats MQTT ou gérer leur évolution par version ;
- éviter toute dépendance du firmware à une adresse, un certificat ou un fournisseur figé sans mécanisme de renouvellement.

#### Phase D — plateforme AquaLook

- gestion multi-utilisateurs ;
- gestion multi-sites ;
- association de plusieurs modules à une installation ;
- supervision centralisée ;
- historique et statistiques ;
- tableaux de bord ;
- notifications avancées ;
- administration Web ;
- gestion de flotte et préparation des déploiements OTA contrôlés ;
- ouverture vers les consommations d’eau, sondes, météo et recommandations d’arrosage.

Principes d’architecture :

- l’ESP32 reste l’autorité locale et temps réel ;
- le cloud transporte, historise et supervise mais ne pilote jamais directement un relais ;
- Flutter présente les données et émet des demandes, mais n’embarque pas la logique métier critique ;
- MQTT transporte les messages et ne devient pas le moteur d’arrosage ;
- toute commande distante est validée localement, bornée, tracée et acquittée ;
- la perte d’Internet, du broker, du VPS ou de l’application ne doit pas empêcher les cycles locaux ;
- HiveMQ sert à valider le concept, puis la migration vers OVHcloud doit rester possible sans réécriture du moteur local.

Stratégie de persistance de la configuration :

À terme, les données de configuration et d'historique doivent pouvoir exister à la fois sur le module et dans le cloud, sans que l'un ne devienne une dépendance obligatoire de l'autre.

- les données indispensables au fonctionnement autonome et à la sécurité (identifiants WiFi, planning des zones, seuils de pluie, paramètres système et d'affichage, garde de retour arrière OTA) restent toujours stockées localement sur le module, en NVS, et ne migrent jamais uniquement vers la carte SD ou le cloud ;
- les données volumineuses ou non critiques pour l'autonomie (historique des opérations de mise à jour, journaux détaillés, statistiques, configurations avancées) peuvent être déplacées vers la carte SD dès aujourd'hui, puis vers le cloud à terme, à condition que leur absence dégrade une fonctionnalité annexe sans jamais interrompre l'arrosage ;
- l'application native embarquée sur le module (interface Web locale actuelle) reste systématiquement disponible et pleinement fonctionnelle sans connexion au cloud ; l'application mobile et le cloud constituent une seconde façon d'accéder au module et de le configurer, pas la seule ;
- toute donnée dupliquée entre le module et le cloud doit avoir une source de vérité explicite et une stratégie de synchronisation et de résolution de conflit documentée avant mise en œuvre.

Ordre de réalisation proposé :

1. documenter les topics et les schémas de messages ;
2. connecter un prototype AquaLook à HiveMQ Cloud en lecture seule ;
3. valider publication, reconnexion, charge mémoire et stabilité ;
4. ajouter des commandes non critiques avec acquittement ;
5. réaliser un premier tableau de bord Flutter ;
6. valider les commandes d’équipements avec les règles d’autorité locales ;
7. ajouter notifications, historique et gestion multi-modules ;
8. mesurer les besoins réels d’exploitation ;
9. migrer vers un VPS OVHcloud avec Mosquitto et Node-RED ;
10. développer progressivement les services de plateforme.

Invariant impératif : le cloud et l’application mobile restent des extensions du système. Le planificateur, la sécurité des relais, les cycles d’arrosage et la validation des commandes restent locaux. Une indisponibilité du broker MQTT, de l’application, d’Internet ou du VPS ne doit jamais provoquer l’arrêt du système, une commande intempestive ou une modification silencieuse d’un programme.

### Mesure de la consommation d’eau par voie

Ajouter la possibilité de mesurer et d’historiser la quantité d’eau réellement consommée par chaque voie d’arrosage.

Objectifs fonctionnels :

- mesurer le débit instantané et le volume cumulé pour chaque voie ;
- associer la consommation à chaque cycle, programme et zone ;
- afficher les volumes consommés sur l’interface locale et, à terme, dans le cloud ;
- conserver des historiques journaliers, mensuels et saisonniers ;
- détecter un débit anormalement faible, nul ou excessif pendant l’ouverture d’une vanne ;
- détecter une circulation d’eau alors qu’aucune voie n’est commandée ;
- permettre une calibration propre à chaque débitmètre.

Points d’architecture à étudier :

- choix et plage de mesure des débitmètres ;
- nombre de capteurs nécessaires et implantation hydraulique par voie ;
- acquisition fiable des impulsions sans perturber le planificateur ni l’interface Web ;
- utilisation éventuelle d’un microcontrôleur secondaire, par exemple un Lolin S2 Mini, pour compter les impulsions puis transmettre les mesures au module principal ;
- protocole d’échange entre le module principal et le module de comptage, notamment I²C, UART ou autre liaison robuste ;
- stockage des index cumulés et reprise correcte après redémarrage ou coupure de courant ;
- gestion du débordement des compteurs, du bruit électrique et des impulsions parasites ;
- fréquence de remontée, précision attendue et impact sur la mémoire et la charge CPU ;
- définition des seuils d’alerte selon les caractéristiques de chaque voie.

Invariant impératif : une panne d’un débitmètre ou du module de comptage ne doit pas provoquer l’activation intempestive d’une vanne ni bloquer le fonctionnement de base du programmateur. La mesure doit rester découplée de la sécurité de commande des relais.

### Mesure de l’humidité du sol par zone

Ajouter la possibilité de mesurer l’humidité du sol à l’aide de sondes associées aux zones d’arrosage concernées.

Objectifs fonctionnels :

- associer une ou plusieurs sondes d’humidité à une zone précise ;
- afficher la valeur actuelle et l’état de fraîcheur de la mesure ;
- conserver un historique des mesures par zone ;
- définir des seuils propres à chaque zone selon le type de sol, les plantations et la profondeur de mesure ;
- signaler une sonde absente, déconnectée, incohérente ou dont la valeur reste figée ;
- utiliser, après validation, l’humidité mesurée pour éviter, reporter ou ajuster un arrosage devenu inutile ;
- comparer l’évolution de l’humidité avant et après un cycle afin d’évaluer son efficacité ;
- rendre les mesures consultables dans l’interface locale et, à terme, dans le cloud.

Points d’architecture à étudier :

- choix de sondes adaptées à une installation durable, de préférence capacitives ou numériques et résistantes à la corrosion ;
- nombre maximal de sondes et possibilité d’associer plusieurs sondes à une même zone ;
- implantation, profondeur et représentativité de chaque point de mesure ;
- calibration individuelle en sol sec et en sol humide ;
- alimentation intermittente des sondes afin de réduire la corrosion et la consommation ;
- transmission filaire ou déportée des mesures vers le module principal ;
- utilisation éventuelle d’un microcontrôleur secondaire pour l’acquisition de plusieurs sondes ;
- filtrage, moyenne temporelle, fréquence de mesure et détection des valeurs aberrantes ;
- stockage de la dernière mesure valide avec son horodatage ;
- stratégie explicite lorsque la mesure est indisponible ou trop ancienne ;
- définition de l’autorité de la sonde : information seule, blocage d’un cycle, report ou adaptation de durée.

Invariant impératif : une sonde absente, en défaut ou non calibrée ne doit jamais provoquer une décision silencieuse ou imprévisible. Le comportement de repli doit être configurable, visible dans l’interface et conserver la sécurité ainsi que l’autonomie du planificateur local.

### Recommandations d’arrosage intelligent

Ajouter un moteur d’aide à la décision capable d’analyser les mesures de consommation d’eau et d’humidité du sol afin de proposer des ajustements d’arrosage adaptés à chaque zone.

Objectifs fonctionnels :

- comparer la quantité d’eau réellement distribuée avec l’évolution mesurée de l’humidité du sol ;
- détecter les zones sur-arrosées, sous-arrosées ou dont l’eau semble mal absorbée ;
- proposer une modification de durée, de fréquence, d’intervalle ou de volume cible ;
- recommander le report ou l’annulation d’un cycle lorsque le sol reste suffisamment humide ;
- recommander un fractionnement des cycles lorsque l’apport d’eau est trop rapide par rapport à la capacité d’absorption du sol ;
- tenir compte des caractéristiques propres à chaque zone, notamment le type de sol, les plantations, la profondeur des racines et le débit mesuré ;
- apprendre progressivement la réponse habituelle de chaque zone après un arrosage ;
- afficher pour chaque recommandation les mesures utilisées, le raisonnement appliqué, le gain d’eau estimé et le niveau de confiance ;
- permettre à l’utilisateur d’accepter, de modifier ou de refuser chaque proposition ;
- conserver l’historique des recommandations et des décisions prises afin d’évaluer leur efficacité.

Points d’architecture à étudier :

- démarrage par un moteur de règles explicites et paramétrables avant toute approche statistique ou apprentissage automatique ;
- distinction entre données valides, données anciennes, valeurs aberrantes et capteurs en défaut ;
- période minimale d’observation avant de formuler une recommandation fiable ;
- calcul de la réponse hydrique d’une zone à partir du volume apporté et de la variation d’humidité observée ;
- prise en compte éventuelle des prévisions météo, de la pluie mesurée et de l’évapotranspiration ;
- exécution locale des règles essentielles et possibilité d’une analyse plus avancée dans le cloud ;
- limitation stricte des ajustements proposés afin d’éviter les variations excessives ;
- mécanisme de retour arrière vers les paramètres précédents ;
- traçabilité complète de la version de l’algorithme, des données d’entrée et de la décision proposée ;
- définition progressive de plusieurs niveaux de fonctionnement : observation seule, recommandation, application après validation et automatisation encadrée.

Invariant impératif : aucune modification de programme ou de durée ne doit être appliquée silencieusement. Le mode par défaut doit rester la recommandation soumise à validation de l’utilisateur. Toute automatisation future devra être explicitement activée, bornée par des limites de sécurité, réversible et désactivée automatiquement en cas de données insuffisantes ou de capteur défaillant.
