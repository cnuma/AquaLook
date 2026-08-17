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
3. format du manifeste des ressources Web et extension de la chaîne de publication GitHub Releases — **fait et validé en conditions réelles le 16 août 2026 (release v5.9.5)** : `tools/generate_web_manifest.py` (schéma `aqualook-web-manifest-v1`, un fichier par entrée avec URL/taille/SHA-256, même convention que `generate_ota_manifest.py`) et `.github/workflows/ota-release.yml` étendu pour publier chaque fichier de `data/` comme asset de la release et générer `aqualook-web-manifest.json` à côté du manifeste firmware — validé localement (manifeste généré sur les 10 fichiers de `data/`, ~2 Ko, sous le budget de 8 Ko) ; validé au tag v5.9.5 : manifeste et 10 fichiers `data/` publiés comme assets, puis consommés par le module ;
4. téléchargement + vérification SHA-256 d’un fichier, sans écriture (symétrique à l’étape « vérification sans installation » de l’OTA firmware) — **fait et validé le 16 août 2026** (`WebAssetsUpdater::verifyOnly`, `src/WebAssetsUpdater.h/.cpp`, route de test `/api/debug/verify-web-asset` + `/status`) ; voir l’enquête ci-dessous, résolue ;

**Constat et résolution du 16 août 2026 — mémoire contiguë insuffisante pour une poignée de main TLS en fonctionnement normal.**

Premier symptôme : `WebAssetsUpdater::verifyOnly()` appelée en fonctionnement normal (Display + SD + serveur Web déjà actifs) contre une vraie URL de release GitHub (`aqualook-manifest.json` de la release v5.9.4, 867 octets) échouait systématiquement à la connexion TLS — `ssl_client.cpp: start_ssl_client(): (-32512) SSL - Memory allocation failed`. `/api/diagnostics` confirmait `heapFree` ~71 Ko mais `heapLargestBlock` seulement ~17-21 Ko selon le moment. Aucun crash — l'appel échouait proprement.

Root-cause précisée avec `/api/debug/heap-info` (nouvelle route de diagnostic lecture seule, `heap_caps_get_info`/`heap_caps_print_heap_info`) : le seul bloc mémoire assez grand pour héberger un jour un tampon mbedTLS est une zone de 113 840 octets dont 94 568 sont occupés par exactement 2 allocations — qui correspondent (calcul octet pour octet, en-têtes malloc inclus) aux sprites TFT_eSPI `_sprPlan` (320×90 = 57 600 octets) et `_sprBtn0` (154×120 = 36 960 octets), alloués une fois au démarrage et jamais libérés. Confirmation indépendante : le test `CHECK_VERSION` de l'OTA firmware **déjà en production** (mode maintenance, `api.github.com`, même bundle `OtaTlsTrust`, même chaîne de certificats Sectigo `CA DV E36` → `Root E46`) a été déclenché pour de vrai sur le module et a réussi, avec `heapFree=245004` à l'instant de la poignée de main — près de 8x plus que ce qui restait ici.

Correctif appliqué : `DisplayManager::suspendForMemoryRelief()`/`resumeAfterMemoryRelief()` libère puis recrée `_sprBtn0` **et** `_sprPlan` (un essai intermédiaire avec `_sprBtn0` seul a changé l'erreur mbedTLS de « allocation mémoire » à « vérification de certificat », sans suffire — signe que le manque de mémoire se manifestait juste plus loin dans la validation, pas qu'il avait disparu). Appelé uniquement depuis `WebManager::update()` (boucle principale), jamais depuis le callback AsyncTCP : `TFT_eSprite` n'est pas thread-safe, donc la demande de vérification est déposée par le callback HTTP dans des champs fixes sous section critique (même schéma que `_pendingSystem`/« ne jamais écrire depuis AsyncTCP », déjà en place) et exécutée plus tard par la boucle principale, qui suspend le sprite, télécharge, vérifie, puis le recrée avant que `DisplayManager::update()` ne soit rappelé dans la même itération — jamais de rendu concurrent pendant la fenêtre où le sprite n'existe pas. Nouvelle route `/api/debug/verify-web-asset/status` (polling) puisque l'opération ne s'exécute plus dans le callback HTTP d'origine.

Pendant l'enquête, un root CA manquant a aussi été ajouté par précaution (`Sectigo Public Server Authentication Root E46`, empreinte SHA-256 vérifiée par deux sources indépendantes, régénéré via `tools/generate_ota_tls_trust.ps1`) — utile en soi (raccourcit la chaîne de confiance d'un cran) mais **pas la cause du blocage** : la même erreur générique persistait après son ajout, tant que la mémoire manquait.

Validé sur le module : 3/3 tentatives réussies contre le vrai fichier de release GitHub, SHA-256 vérifié correct à chaque fois, `heapLargestBlock` revenu à sa valeur de repos (17 396, stable) après chaque cycle libération/recréation — pas de fuite mémoire.

Limite connue, à traiter à l'étape 6 : l'opération bloque la boucle principale pendant toute la durée du téléchargement (jusqu'à 15 s de timeout configuré) — acceptable pour ce test manuel et rare, mais à revoir avant un déclenchement périodique automatique (l'invariant OTA existant interdit d'interrompre silencieusement un cycle d'arrosage ; une vérification qui retarderait de plusieurs secondes une commande relais pendant sa fenêtre irait à l'encontre de cet invariant).

5. écriture effective sur SD, avec stratégie explicite de cohérence en cas d’interruption — **fait et validé le 17 août 2026** : déploiement transactionnel via un répertoire de transit (`/www.new`), bascule par déplacements fichier par fichier, et rattrapage au montage (`StorageManager::beginAssetStaging/commitAssetStaging/recoverInterruptedStaging`). Règle absolue tirée de l’incident du 17 août : on n’écrit jamais directement dans `/www`. Une coupure avant la bascule laisse `/www` intact ; test d’interruption réel mené et concluant. À noter, limite de SdFat rencontrée en chemin : `FatVolume::rename()` ouvre la source en lecture seule et ne peut donc pas mettre à jour l’entrée `..` d’un sous-répertoire — le renommage de répertoire échoue, d’où la bascule fichier par fichier ;
6. interface locale (`/ota`) et journalisation de chaque étape — **fait et validé le 17 août 2026**. Le déploiement s’exécute en **mode maintenance** et non en fonctionnement normal : c’est la réponse à la limite laissée ouverte à l’étape 4 (mémoire contiguë insuffisante, blocage de la boucle principale). Le module redémarre sur un contexte minimal disposant d’environ 242 Ko de tas libre au lieu de ~32 Ko, y fait son travail, puis redémarre en production — séparation nette entre un démarrage de mise à jour et un démarrage de production. C’est la seule commande de maintenance à monter la carte SD, ce que l’isolation d’origine autorise puisqu’elle n’écrit pas en flash. Deux tentatives précédentes en fonctionnement normal avaient échoué (poignée de main TLS impossible, puis débordement de pile de `loopTask`) ; le code correspondant reste garé sur `wip/step6-webassets-check`, son plantage au démarrage n’étant **pas** expliqué à ce jour.

   L’interface `/ota` porte désormais une section « Ressources Web » : version installée, nombre de fichiers, origine, et un bouton qui déclenche `POST /api/webassets/update` avec confirmation, messages d’attente dédiés et rechargement automatique une fois le module revenu. Le résultat est persisté dans `MaintenanceResultStore` et affiché au retour : sans cela, le journal en RAM étant effacé par le redémarrage, un échec serait passé totalement inaperçu. Les champs propres à l’OTA firmware sont préservés par la fusion de `MaintenanceResultStore::save()` — vérifié sur matériel : une mise à jour firmware en attente survit intacte à un déploiement de ressources Web.

   Garde d’arrosage conforme à la règle retenue — la mise à jour est prioritaire car elle apporte des corrections, et n’est refusée que si un cycle est **en cours** : vérifié dans les deux sens (arrosage manuel actif → `409 arrosage en cours` pour les deux canaux ; arrosage arrêté → `202` immédiatement).

   **Reste à faire de la spécification UX d’origine :** notification ntfy à la fin de l’opération, signalement d’une mise à jour en attente (LED violette, icône LCD, pastille dans la barre du haut renvoyant vers `/ota`). Seule la confirmation à l’écran est en place.

7. vérification périodique, une fois la chaîne manuelle éprouvée sur le terrain.

### Pages HTML servies tronquées sous un `Content-Length` complet — 17 août 2026

**Statut : contourné côté application, défaut de bibliothèque non corrigé.**

Découvert en validant l’interface de l’étape 6, et sans rapport avec elle : la page `/ota` arrivait au navigateur corrompue. Trois défauts distincts, tous silencieux, tous producteurs de fausses informations.

**1. La page contenait de la mémoire brute.** `/ota` et `/logs` passaient par la surcharge `beginResponse(code, type, const char*)`, qui recopie la page entière dans une `String` du tas puis rappelle `substring()` sur le reste à chaque acquittement TCP — plus de 25 Ko de pic pour une page de 12 Ko, alors que le plus gros bloc libre tombe à 17 Ko quand l’écran est allumé. L’échec d’allocation n’est vérifié nulle part et la longueur prévue est écrite depuis une `String` devenue vide : le module a envoyé au navigateur neuf kilo-octets de son propre tas à la place de la page. Corrigé en passant par la surcharge `(const uint8_t*, size_t)`, qui diffuse directement depuis la flash (`AsyncProgmemResponse`) sans aucune copie en tas.

**2. Des octets disparaissaient au milieu du document.** `AsyncAbstractResponse::_ack()` calcule la taille d’un bloc à partir de la place annoncée par la pile TCP (`client()->space()`), remplit le bloc, puis écrit — et ignore la valeur rendue par `write()`. Si la place a diminué entre-temps parce que d’autres connexions ont consommé le tampon partagé, `AsyncClient::add()` n’envoie que ce qui rentre encore et renvoie ce nombre, mais le curseur de lecture avance de la taille **demandée**. Les octets refusés ne sont jamais reproposés. Mesuré à huit chargements simultanés : jusqu’à 174 octets manquants en plein milieu, le reste du document intact et dans l’ordre, sous un `Content-Length` annonçant la taille complète. `AsyncBasicResponse::_ack()` a le même défaut.

Une correction du contrôle de flux de la bibliothèque a été écrite, appliquée via un script de pré-compilation, testée sur matériel, puis **abandonnée le même jour** : elle dégradait nettement le comportement (la plupart des connexions restaient sans réponse). Elle n’est pas conservée dans l’historique. Tant que cette machine à états — crédits en vol, `_cache`, comptabilité `_sentLength`/`_writtenLength` — n’est pas comprise de bout en bout, on n’y touche pas : un défaut connu et borné vaut mieux qu’une correction hasardeuse au cœur du transport.

**3. La parade retenue est applicative et volontairement modeste.** Une seule page embarquée servie à la fois (`WebManager::sendEmbeddedPage`), refus explicite au-delà : `503`, `Retry-After`, et une petite page qui se recharge d’elle-même après trois secondes. Les pages embarquées sont autonomes — CSS et JS en ligne, une seule requête par chargement — donc la borne ne gêne pas l’usage réel. Un délai anti-blocage de dix secondes garantit que le compteur ne peut pas rester coincé en haut : une fuite rendrait les pages définitivement inaccessibles, panne bien pire que le défaut contourné.

Mesures sur matériel, écran allumé (plus gros bloc libre 17 396 octets), page `/ota` de 12 503 octets, comparaison octet à octet contre une référence :

| Situation | Résultat |
|---|---|
| Avant, 8 chargements simultanés | 8 pages sur 8 défectueuses |
| Avant, seuil mesuré | 1 et 2 simultanés intacts ; dégradation à partir de 3 |
| Borne à 2 pages en parallèle | encore 2 pages corrompues sur 32 — la contention porte sur les tampons TCP partagés, pas seulement sur nos pages |
| Borne à 1 page, 5 rafales de 8 | 10 pages servies **toutes intactes**, 30 refus explicites, **0 corrompue**, 0 connexion sans réponse |
| Usage nominal, 20 chargements enchaînés de `/ota` et de `/logs` | 40/40 intactes, aucun refus |

**Portée réelle et leçon.** Ce défaut n’est pas propre à `/ota` : il touche toute réponse assez grande pour être découpée en plusieurs blocs TCP, y compris les fichiers servis depuis la carte SD. `/logs` n’était épargné que par sa taille. Il explique très probablement une partie des symptômes « la page ne se charge pas » / « la page est cassée » observés précédemment et attribués à la pression mémoire seule.

La leçon de méthode : le symptôme était visible depuis le début sur une simple lecture d’octets, mais un `curl` qui renvoie `HTTP 200` et une taille conforme au `Content-Length` a l’air d’un succès. Il a fallu comparer octet à octet contre une référence pour le voir. **Un code de retour n’est pas une vérification** — à généraliser aux tests de non-régression Web.

### Partition NVS saturée — incident et correction du 16 août 2026

**Statut : corrigé, mais la correction ne peut pas être déployée par OTA — voir l’avertissement en fin de section.**

Découvert en marge des tests de robustesse de la mise à jour des ressources Web, sans lien avec celle-ci.

Symptôme initial : toute écriture de configuration échouait en silence. Le journal montrait `putBytes(): nvs_set_blob fail: config NOT_ENOUGH_SPACE` puis `Config: ecriture NVS incomplete (0/4884)` — **zéro octet écrit sur 4884** — pendant que l’API HTTP répondait `{"ok":true}`. L’utilisateur croyait donc enregistrer alors que rien n’était persisté. Reproduit deux fois sur deux (modification d’un créneau, puis d’un nom de zone). Absent des 27 fichiers de journal des deux jours précédents : la saturation venait d’être atteinte, vraisemblablement parce que la session du 16 août avait ajouté deux namespaces NVS (`aq_log_cfg` pour la bascule des logs Timing, `aq_wifi_ka` pour la cible keepalive).

Aggravation constatée juste après : au redémarrage suivant, `Config: bloc NVS invalide (entete/CRC)` puis `Config: valeurs par defaut appliquees`, `SSID='' mot de passe present=non`. Le module est parti en portail captif, injoignable sur le réseau. L’analyse du dump binaire de la partition (`esptool read_flash 0x9000 0x5000`) a montré pourquoi : le blob de configuration est réécrit **en entier** à chaque changement ; l’écriture s’était interrompue faute de place après avoir écrit le premier morceau, laissant les 4 octets de CRC en fin de blob à `0xFFFFFFFF` (flash vierge). Le blob était donc structurellement tronqué, donc rejeté, donc configuration perdue — y compris les identifiants WiFi.

Point de conception à conserver : le code refuse d’écraser un bloc NVS invalide (`NVS invalide conserve sans ecrasement; defauts RAM actifs`). C’est ce qui a permis de récupérer intégralement, depuis le dump, les identifiants WiFi, la clé OpenWeatherMap, l’étalonnage tactile et les plannings des deux zones actives — les données étaient toutes présentes, seul le CRC final manquait. Sans ce garde-fou, un redémarrage aurait réécrit des valeurs par défaut par-dessus.

Cause racine : la partition NVS faisait `0x5000` (20 Kio) pour un blob de configuration de 4884 octets réécrit intégralement à chaque modification — ce qui exige transitoirement environ le double, plus la place des sept autres namespaces. Le dimensionnement était insuffisant dès l’origine ; l’ajout de deux namespaces l’a simplement fait franchir le seuil.

Correction : NVS portée de 20 à **84 Kio** (`0x15000`) dans `aqualook_partitions.csv`. La NVS est délibérément placée **après** les deux slots applicatifs plutôt qu’à l’adresse traditionnelle `0x9000` : la chaîne Arduino/PlatformIO écrit `boot_app0.bin` à `0xe000` en dur (`framework-arduinoespressif32/tools/platformio-build.py`) et l’image applicative à `0x10000` par défaut (`upload.offset_address`, qui ne dérive pas de la table de partitions — vérifié dans `builder/main.py`). Une première tentative d’agrandissement sur place a été abandonnée pour cette raison : elle plaçait la NVS sous `0xe000`, garantissant sa corruption à chaque téléversement. La disposition retenue garde `otadata` et `app0` à leurs adresses par défaut, donc aucun patch du framework ni surcharge d’offset. Coût : slots applicatifs ramenés de `0x1F0000` à `0x1E0000` (1920 Kio chacun, firmware à 72 %), et 20 Kio laissés inutilisés en `0x9000-0xE000`. La constante de contrôle correspondante dans `SystemDiagnostics.cpp` (qui rapportait `ready=no` si les slots étaient plus petits qu’attendu) a été extraite en `MIN_OTA_PARTITION_SIZE`, et `MAX_FIRMWARE_SIZE` dans `tools/generate_ota_manifest.py` a suivi, faute de quoi la chaîne de publication aurait pu produire un firmware trop gros pour le slot.

**Avertissement pour la mise en production :** une table de partitions ne se déploie pas par OTA — l’OTA ne pousse que l’image applicative. Un module déjà livré conserverait l’ancienne table et resterait exposé à cet incident, sans correctif possible à distance. Ce changement doit donc être appliqué par USB avant toute mise en service, au même titre que la mise à jour des ressources Web est un prérequis de production. Cette contrainte relève plus généralement du chapitre « Première mise en service d’un module », ouvert à la suite de cet incident : elle en est le cas d’école.

**Leçon de la restauration — la configuration n’est pas un seul bloc.** La récupération menée après l’incident n’a restauré que le blob principal (namespace `aqualook`) et la cible keepalive (`aq_wifi_ka`), en laissant tomber les autres namespaces sans les inventorier. Deux d’entre eux contenaient de vrais réglages utilisateur, redécouverts seulement à l’usage : `aq_notify` (serveur et sujet ntfy — l’utilisateur a dû les ressaisir) et `aq_log_cfg` (bascule des logs de lenteur, revenue à « activés » et repolluant le journal). Les deux étaient pourtant présents et lisibles dans le dump binaire. `aq_incidents` a aussi été perdu, sans conséquence puisqu’il ne porte que de l’historique.

À retenir pour toute récupération future : la configuration du module est **répartie sur huit namespaces NVS** (`aqualook`, `aq_wifi_ka`, `aq_log_cfg`, `aq_notify`, `aq_incidents`, `aq_maint`, `aq_maint_res`, `aq_ota_guard`), et non concentrée dans le blob principal. Une restauration doit les parcourir tous explicitement, et se conclure par une relecture comparative poste par poste — pas seulement sur les créneaux d’arrosage, qui sont la partie la plus visible mais pas la plus complète. La route de diagnostic `/api/debug/nvs-stats` liste ces namespaces et le nombre d’entrées de chacun, ce qui permet de repérer d’un coup d’œil ceux qui sont absents (`-1`) après une réinitialisation.

Campagne de validation menée le 16 août 2026 après correction, sur le module réel :

- 60 écritures de configuration consécutives : 60/60 réussies, nombre d’entrées NVS rigoureusement stable (312), donc pas de fuite — le ramasse-miettes recycle correctement ;
- configuration poussée au maximum (les 80 créneaux des deux zones activés) : 80/80 réussies, le blob restant de taille fixe (159 entrées quel que soit son contenu) ;
- **six coupures brutales (reset matériel) déclenchées à 700, 780, 820, 850, 900 et 1000 ms après une modification**, c’est-à-dire de part et d’autre du debounce de sauvegarde de 800 ms, donc en pleine fenêtre d’écriture NVS : configuration et identifiants WiFi intacts dans les six cas, aucun bloc invalide, aucun retour aux valeurs par défaut. C’est la reproduction directe du scénario qui avait provoqué l’incident ;
- persistance vérifiée après redémarrage matériel (`mise sous tension`) : zéro écart sur les 82 créneaux ;
- non-régression applicative après repartitionnement : `CHECK_VERSION` OTA réussi, vérification HTTPS d’une ressource Web réussie, déploiement SD réseau avec SHA-256 identique, commande relais ON/OFF en ~130 ms, refus de vérification pendant arrosage toujours actif.

**Chemin d’installation OTA complet — validé le 16 août 2026 sous la nouvelle table.** Comme `STAGE_UPDATE_TEST` et `INSTALL_UPDATE` refusent de s’exécuter sans version plus récente publiée (`check-version-required`, garde légitime), la validation a nécessité de publier réellement la release **v5.9.5**. Déroulé complet sur le module, depuis la 5.9.4 installée :

1. `CHECK_VERSION` → `update-available`, `installed=5.9.4 available=5.9.5` ;
2. `DOWNLOAD_UPDATE_TEST` → 1 424 384 octets en 6,4 s, SHA-256 attendu et calculé identiques, sans écriture flash ;
3. `STAGE_UPDATE_TEST` → **écriture flash réelle dans `app1` à sa nouvelle adresse `0x1F0000`**, 1 424 384 octets en 10,9 s, `detail=firmware-staged-inactive-app1`, partition de démarrage inchangée (`boot=app0`) ;
4. `INSTALL_UPDATE` → `partition activee label=app1 address=0x1F0000`, redémarrage, module en **5.9.5** (`build 888`, `sha=ca56fd1`) exécuté depuis `app1`.

Après installation : `running=app1`, `boot=app1`, `ready=yes`, et surtout **configuration intégralement préservée** — WiFi, clé OWM, cible keepalive, réglages système et les 82 créneaux des deux zones, zéro écart. L’OTA ne touche pas la NVS, ce qui est confirmé en pratique et non seulement par construction. C’était le risque principal du repartitionnement : il est levé.

Cette release a aussi exercé pour la première fois en conditions réelles la chaîne de publication des ressources Web (étape 3) : `aqualook-web-manifest.json` et les 10 fichiers de `data/` publiés comme assets, puis un fichier décrit par ce manifeste (`app.js`, 53 408 octets) téléchargé et vérifié par le module en 227 ms — étapes 3 et 4 validées bout en bout sur l’infrastructure réelle, et non plus seulement en local.

Pistes non retenues, à garder en tête si le besoin revient : réduire le blob lui-même (16 zones sont persistées alors que 2 sont utilisées, `CfgZone` fait 276 octets, soit environ 3,8 Kio de zones inutilisées sur 4884) serait déployable par OTA, mais impose une migration de schéma et ne résout pas la place des autres namespaces. À reconsidérer si le nombre de namespaces continue d’augmenter.

#### Expérience utilisateur du déclenchement — spécification du 16 août 2026

À ce jour (test du 16 août 2026), le déclenchement d’un déploiement se fait directement depuis ce poste de travail (route de test `/api/debug/deploy-file`, appelée manuellement) : cela valide le mécanisme d’écriture, mais ce n’est pas utilisable par l’utilisateur final. Une fois la chaîne manifeste + téléchargement + vérification en place (étapes 3 à 5 ci-dessus), le déclenchement et la visibilité doivent devenir entièrement pilotables par l’utilisateur, pour les deux canaux (OTA firmware et ressources Web/SD) :

- **notification qu’une mise à jour est en attente**, dès que le module détecte que sa version diffère de celle publiée (comparaison de manifeste), via le canal de notification déjà en service (ntfy) — informative, avant toute action ;
- **LED** : clignotement violet dédié à l’état « mise à jour en attente », distinct des trois autres états déjà fixés (bleu = arrosage en cours, ambre = recherche réseau, rouge clignotant = panne réservée à `FaultManager`) — le violet a déjà été réservé à cet usage lors de la réorganisation des couleurs LED/LCD du 16 août 2026 ;
- **LCD** : icône dédiée reprenant le même violet, sur le même principe que l’icône signal WiFi (`renderSignalSprite()`) déjà utilisée pour l’état « recherche réseau » ;
- **page `/`** : icône ou badge dans la barre du haut signalant une mise à jour en attente, avec lien direct vers `/ota` ;
- **page `/ota`** : lien mis en avant pour accéder facilement à la mise à jour en attente ; bouton de déclenchement manuel du processus complet (téléchargement, vérification, installation) pour le canal concerné (firmware ou SD/Web) ;
- **confirmation de fin de processus** : notification (même canal ntfy) une fois le déploiement terminé, en cas de succès comme d’échec — l’utilisateur ne doit pas avoir à revenir consulter la page pour savoir si ça a fonctionné.

Ce comportement s’applique uniformément aux deux canaux de mise à jour (OTA firmware et SD/Web), avec un vocabulaire visuel et de notification commun, même si le déclenchement et le contenu déployé restent indépendants l’un de l’autre (cf. décision de canal découplé ci-dessus). Prérequis : ce point dépend de l’étape 3 (manifeste) pour savoir qu’une mise à jour existe — tant que cette détection n’existe pas, il n’y a rien à signaler ni à déclencher depuis l’interface.

### Page Web qui ne se charge pas — saturation mémoire par les sprites d’affichage (16 août 2026)

**Statut : corrigé, avec une limite résiduelle assumée.**

Symptôme rapporté : « la page ne se charge pas ». Reproduit et mesuré : en accès **séquentiel** tout répond parfaitement (15/15, chaque fichier en moins de 0,7 s), mais en **parallèle** — ce que fait tout navigateur, qui ouvre couramment 6 connexions pour charger une page — le module s’effondre. Seuil mesuré : 2 requêtes simultanées passent, **3 suffisent à faire échouer la page**. À 8 requêtes : `/app.js` et `/style.css` en échec réseau, `/index.html` servi en 20 à 25 secondes, et surtout le serveur devient **totalement muet** — même les routes API qui ne touchent pas à la carte SD restent sans réponse.

Deux fausses pistes écartées en chemin, chacune par la mesure : le `logo.png` absent de LittleFS (il répond en 0,09 s depuis la SD, 338 octets — il n’a jamais été en cause), puis l’attente bloquante du mutex SD dans le rappel de lecture. Cette seconde piste était réelle et a été corrigée au passage — `StorageManager::readChunkNonBlocking()` + `RESPONSE_TRY_AGAIN`, pour ne jamais bloquer la tâche unique d’AsyncTCP sur laquelle transitent **toutes** les connexions — mais elle n’a rien changé au symptôme : les mêmes 4 échecs subsistaient. Le correctif est conservé car il supprime un vrai risque de blocage en tête de file, mais ce n’était pas la cause.

Cause réelle, établie par une expérience concluante : le tas ne laisse que ~32 Ko libres au repos (plus gros bloc contigu ~17 Ko), parce que **deux sprites TFT_eSPI sont alloués en permanence pour environ 95 Ko à eux deux** — `_sprPlan` (320×90×2 = 57 600 octets) et `_sprBtn0` (154×120×2 = 36 960 octets). En profitant de la fenêtre où la vérification HTTPS d’une ressource Web les libère déjà, les mêmes 6 requêtes simultanées passent **toutes en moins de 1,7 s** au lieu d’échouer. La démonstration est directe : ces sprites, et rien d’autre, dictaient le nombre de connexions simultanées possibles.

Point important : ce n’est **pas** une régression des travaux du 16 août. Le tas libre global est resté à ~71 Ko avant comme après l’ensemble des changements du jour. Cela réinterprète rétrospectivement deux incidents de la même journée, attribués sur le moment à l’instabilité WiFi : une page restée blanche depuis un téléphone, et un module « ne répondant plus aux appels de `/` » ayant nécessité un redémarrage manuel. Les deux correspondent très probablement à cette saturation, déclenchée par un simple chargement de page depuis un navigateur.

Correctif retenu : libérer les deux sprites dès que l’écran passe en veille, les recréer avant tout rendu au réveil. L’écran en veille ne dessine rien (`update()` sort avant tout redraw), la libération y est donc sans effet visible. L’invariant est réévalué à chaque passage dans `update()` plutôt qu’au seul instant du réveil, car le réveil emprunte plusieurs chemins (tactile, démarrage d’arrosage, appel direct) et la vérification HTTPS libère aussi ces sprites de son côté. Les méthodes `suspendForMemoryRelief()`/`resumeAfterMemoryRelief()` sont rendues idempotentes, un `createSprite()` sur un sprite déjà alloué perdant sinon l’ancien tampon.

Résultat mesuré après correction : à la mise en veille le tas libre passe de 32 376 à **126 976 octets**, et 8 requêtes simultanées réussissent toutes en moins de 2,2 s, la sonde API répondant en 0,23 s *pendant* la charge.

**Limite résiduelle assumée : écran allumé, la contrainte demeure** (~2 connexions simultanées). Avec la veille écran par défaut à quelques minutes, le module passe l’essentiel de son temps dans le cas favorable, mais une consultation Web pendant qu’on manipule l’écran reste exposée. Pistes si le besoin s’en fait sentir, par ordre de coût croissant : réduire `_sprPlan` en le dessinant par bandes (57 Ko à lui seul, gain d’environ 38 Ko sans changer le rendu), réduire le nombre de ressources chargées en parallèle par la page, ou passer à une carte disposant de PSRAM (voir ci-dessous).

À évaluer — **matériel avec PSRAM, comme socle unique du projet** : une carte équipée de PSRAM permettrait de loger les sprites en mémoire externe et de libérer durablement la RAM interne, seule utilisable par les tampons réseau (la PSRAM ne convient pas au DMA). La contrainte de connexions simultanées disparaîtrait sans dépendre de la veille écran. Vérifié le 16 août 2026 : le projet n’utilise **aucune** broche GPIO 16 ou 17, celles que les modules WROVER réservent à la PSRAM — pas de conflit de câblage à prévoir.

**Décision d’architecture actée le 16 août 2026 : le projet ne comportera jamais de code gérant les deux cartes.** Si la PSRAM est retenue après évaluation, elle devient le socle matériel unique, en bascule franche ; les modules sans PSRAM sortent du périmètre supporté. Aucune détection à l’exécution, aucun chemin de code conditionnel, aucun parc mixte. C’est ce qui évite de payer indéfiniment le coût de deux comportements à tester et à maintenir.

Conséquence directe sur la façon de mener l’évaluation : elle doit être conduite **avant** tout engagement, car le choix est en pratique irréversible une fois le socle basculé. Le point à mesurer en priorité est le coût réel du rendu de sprites en PSRAM, sensiblement plus lent que sur RAM interne — ce qui n’est pas neutre alors que des avertissements de lenteur d’affichage existent déjà sur la carte actuelle. Si ce coût s’avérait rédhibitoire pour la fluidité de l’écran, la bascule serait à écarter et les pistes purement logicielles (rendu de `_sprPlan` par bandes) reprendraient la main.

Relève également du chapitre « Première mise en service » : un changement de carte ne se déploie pas à distance, et sous cette décision il ne s’agit pas d’une variante à provisionner mais du seul matériel cible.

### Première mise en service d’un module — ce que le premier flash doit obligatoirement embarquer

**Statut : chapitre à développer.** Ouvert le 16 août 2026 à la suite du repartitionnement NVS, qui a mis en évidence une catégorie de contraintes jusque-là implicite.

Principe directeur : **tout ce que l’OTA ne sait pas livrer doit être posé au premier flash, sinon ce n’est jamais rattrapable à distance.** L’OTA firmware ne pousse que l’image applicative ; le canal de mise à jour des ressources Web ne pousse que des fichiers vers la carte SD. Tout le reste — table de partitions, contenu initial de la carte, ressources de secours en flash — n’a aucun chemin de livraison distant. Un module livré avec l’un de ces éléments incorrect reste définitivement à corriger sur place, ce qui est précisément ce que l’ensemble du chantier OTA cherche à éviter.

Cadre matériel : le projet vise **une seule carte cible**, décision actée le 16 août 2026 — jamais de code gérant plusieurs variantes (voir la section précédente à propos de la PSRAM). Un changement de socle matériel est donc une bascule de parc, pas une variante à provisionner en parallèle.

Éléments identifiés à ce jour comme relevant obligatoirement du premier flash :

- **table de partitions** (`aqualook_partitions.csv`) : NVS de 84 Kio et slots applicatifs de `0x1E0000`. Un module flashé avec l’ancienne table (NVS 20 Kio) subira tôt ou tard la perte silencieuse de configuration décrite plus haut, sans correctif distant possible. C’est le cas d’école qui a fait naître ce chapitre ;
- **contenu initial de la carte SD** (`/www`) : le canal de mise à jour des ressources Web suppose une arborescence déjà en place et un `assets-version.json` exploitable pour comparer les versions. Une carte vierge ou incohérente doit être traitée à la préparation, pas espérée d’une première mise à jour ;
- **ressources de secours en flash/LittleFS** : portail captif et diagnostic minimal, indispensables pour reprendre la main sur un module qui n’a pas encore de réseau — donc avant tout OTA. À noter, `splash.jpg` et `logo.png` sont actuellement absents de LittleFS (repli texte au démarrage) : à trancher, soit les embarquer, soit acter le repli comme comportement nominal ;
- **première configuration réseau** : par portail captif. Le point d’accès de secours est aujourd’hui ouvert (`WiFi.softAP()` sans mot de passe) — acceptable en développement, à réexaminer avant livraison.

À définir dans ce chapitre : la procédure de préparation d’un module neuf (ordre des opérations, vérifications de recette), la manière de constater a posteriori qu’un module a bien la bonne table de partitions, et le sort des modules éventuellement déjà flashés avec l’ancienne.

### Plantages au démarrage — corrigés le 17 août 2026, et pourquoi ils menaçaient l'OTA

**Symptôme observé sur le terrain, avant tout diagnostic :** l'utilisateur avait remarqué que le module redémarrait parfois plusieurs fois d'affilée, puis finissait par se stabiliser. Le comportement était resté inexpliqué, et sans conséquence apparente puisque le module repartait seul.

Mesuré le 17 août 2026 : **3 sessions de démarrage sur 4 se terminaient par un `abort()` environ 5 secondes après le boot**, contre aucune sur les dix journaux de la veille. Deux causes distinctes, la seconde n'étant devenue visible qu'une fois la première écartée :

1. **`weather-fetch` créée sans affinité de cœur.** L'ordonnanceur pouvait la placer sur le cœur 0, celui des piles WiFi et lwIP, où elle analyse un JSON d'environ 16 Ko lu directement depuis le flux réseau — d'autant plus longtemps que le signal est faible (RSSI descendu à −87 dBm ce jour-là). La tâche `IDLE0`, de priorité 0, ne s'exécutait alors plus, `yield()` d'Arduino ne cédant qu'aux priorités égales ou supérieures, et le chien de garde abattait le système. C'est **le même mécanisme que les deux plantages du 16 août** (tâche WiFi dédiée, puis mDNS) : à chaque fois une tâche supplémentaire atterrissait sur le cœur 0 en concurrence avec celle-ci. Épinglée au cœur 1, la signature `task_wdt` a entièrement disparu.
2. **Premier appel météo déclenché à la toute première itération de boucle**, exactement au moment où tout le reste s'alloue : 95 Ko de sprites d'affichage, association WiFi, démarrage du serveur Web, plus la réponse OWM à analyser. Ce pic simultané épuisait le tas et provoquait un `abort()` sur allocation échouée, les exceptions étant désactivées. Premier appel désormais différé de 20 secondes.

Validé sur matériel : **8 démarrages consécutifs, aucun plantage**, contre 3 sur 4 auparavant avec le même protocole.

**Conséquence indirecte qui rendait ce défaut bien plus grave qu'il n'y paraissait — et qui n'avait pas été anticipée : il pouvait provoquer des retours arrière OTA injustifiés.** La garde de démarrage (`OtaBootGuard`) exige 45 secondes de fonctionnement sain pour valider une nouvelle version, et revient à la précédente après 3 tentatives infructueuses. Or le plantage survenait vers 5 secondes, donc bien avant toute validation. Trois démarrages successifs touchés auraient suffi à faire annuler une mise à jour parfaitement saine, sans le moindre rapport avec elle. Le symptôme observé aurait été « les mises à jour reviennent seules en arrière », pratiquement indiagnosticable. La validation OTA du 16 août a réussi (validée à 45 005 ms) : le défaut n'a simplement pas frappé pendant cette fenêtre.

Ces deux correctifs restent des **atténuations d'une contrainte structurelle**, pas des guérisons : cette carte ne dispose que d'environ 32 Kio de tas libre écran allumé. Ils étalent les pics, ils ne créent pas de mémoire. C'est le troisième sujet de la semaine à converger vers le même constat, avec la page qui ne se chargeait pas et l'échec de poignée de main TLS.

### 🌙 Portage vers Guition JC4827W543C (ESP32-S3) — chantier en sommeil

**Statut : en sommeil depuis le 16 août 2026, en attente de livraison des cartes.**

Nouvelles cartes commandées le 16 août 2026 : ESP32-S3, 8 Mo de PSRAM, 4 Mo de flash, dalle IPS 4,3" 480×272, tactile capacitif. Elles deviendront le **socle matériel unique** du projet, conformément à la décision « jamais de code gérant deux cartes ».

Analyse d'impact complète : **`docs/architecture/HW_JC4827W543_PORT_IMPACT.md`**, sur la branche **`hw/jc4827w543-esp32s3-port`**. Le firmware en service n'est pas impacté — cette branche ne contient aucune modification de code.

Ce que l'analyse a établi, et qui contredit l'hypothèse initiale d'un simple changement cosmétique :

- le contrôleur n'est pas un ST77xx mais un **NV3041A en QSPI**, que `TFT_eSPI` ne sait pas piloter — la couche d'affichage doit passer à `Arduino_GFX` ;
- surface concernée mesurée : 3 625 lignes, 431 appels, dont ~423 portables mécaniquement via un adaptateur et **8 seulement** demandant une décision individuelle ;
- le coût est concentré dans le **texte** (pas d'équivalent au `drawString` aligné par datum), et dans la **refonte de la mise en page** en 480×272 ;
- bonnes surprises : les polices du thème sont déjà au format Adafruit `GFXfont` attendu par `Arduino_GFX`, et les couleurs déjà en RGB565 — ces deux postes ne coûtent rien.

Opportunité à arbitrer une fois le matériel disponible : avec 8 Mo de PSRAM, un framebuffer plein écran coûte 261 Ko (3 % de la PSRAM) et permettrait de retirer les sprites partiels, la libération des sprites en veille ajoutée le 16 août 2026, et avec eux toute la classe de bugs « RAM interne saturée ». Conditionné à la mesure du coût d'un rafraîchissement depuis la PSRAM.

Reprise : dérouler les validations par sous-système du document (§8) dès réception, en commençant par les deux mesures décisives — coût du rafraîchissement plein écran, et disponibilité du bus I2C pour le bloc relais. Aucune estimation de charge n'est donnée avant ces mesures, qui peuvent remettre en cause la stratégie.

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
