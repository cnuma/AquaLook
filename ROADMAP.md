# Roadmap AquaLook

Ce document regroupe les évolutions envisagées pour AquaLook. Il ne constitue pas un engagement de développement immédiat ; chaque point devra être détaillé, priorisé et traité sur une branche dédiée.

## Évolutions futures

### Migration des ressources Web vers la carte SD — préalable à l’OTA

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

### Connexion à un cloud externe

Permettre au module AquaLook de se connecter de manière sécurisée à un service cloud externe afin de rendre le système accessible sans connexion directe au réseau local du module.

Objectifs fonctionnels :

- centraliser et historiser les logs techniques et fonctionnels du module ;
- consulter à distance l’état du programmateur, des zones, des cycles et des éventuelles erreurs ;
- piloter l’application et envoyer des commandes au module depuis une interface distante ;
- modifier certains paramètres ou programmes d’arrosage à distance ;
- conserver un fonctionnement local autonome si la connexion Internet ou le service cloud est indisponible ;
- synchroniser les données accumulées localement après une interruption de connexion.

Points d’architecture à étudier :

- protocole de communication sortant initié par le module, par exemple MQTT sécurisé ou HTTPS ;
- authentification forte du module et des utilisateurs ;
- chiffrement TLS des échanges ;
- gestion des droits d’accès et protection contre les commandes non autorisées ;
- file locale persistante pour les logs et commandes en attente ;
- limitation du volume et de la fréquence des remontées afin de préserver la mémoire, la bande passante et la stabilité du firmware ;
- choix entre une plateforme existante, un serveur auto-hébergé ou un service cloud dédié ;
- mécanisme de mise à jour ou de révocation des identifiants du module ;
- traçabilité des commandes distantes et confirmation de leur exécution réelle.

Invariant impératif : le cloud doit rester une extension du système. Le planificateur, la sécurité des relais et les cycles d’arrosage doivent continuer à fonctionner localement et de manière autonome en cas de perte du cloud ou d’Internet.

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
