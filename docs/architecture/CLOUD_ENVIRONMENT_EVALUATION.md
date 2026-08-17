# Évaluation — environnement cloud AquaLook et rôle du mini PC

Document d'aide à la décision, rédigé le 16 août 2026 à la demande de l'utilisateur, qui hésitait entre « une solution VPS » et « un site web hébergé avec base de données et un outil d'interconnexion MQTT externe », sans savoir comment relier ce dernier à un site hébergé.

Il s'adosse à `SYSTEM_ARCHITECTURE.md` (trajectoire Phases A→D, invariants de sécurité et d'autonomie locale) et ne le remplace pas. En cas de divergence, ce sont les invariants de `SYSTEM_ARCHITECTURE.md` qui priment.

## 1. La réponse courte

**Il ne faut pas choisir maintenant entre mini PC et VPS.** Le bon réflexe est de construire l'environnement de façon *portable* — tout en conteneurs, décrit dans un seul fichier `docker-compose.yml` — de sorte que la même pile tourne à l'identique sur le mini PC aujourd'hui et sur un VPS demain, sans réécriture. C'est exactement ce que prévoit déjà la roadmap : validation d'abord (Phase A), migration vers un VPS OVHcloud ensuite (Phase C), *sans* réécriture du firmware ni de l'application.

Le mini PC n'est donc pas une alternative au VPS : c'est **l'environnement de développement et de validation** qui permet d'arriver au VPS en sachant précisément ce qu'on y déploie et ce qu'il doit dimensionner.

## 2. Pourquoi la piste « site web hébergé + MQTT externe » bloque

C'est le point sur lequel l'utilisateur butait, et le blocage est réel, pas un manque de méthode.

MQTT fonctionne par **connexion longue** : un client se connecte au broker et *reste* connecté pour recevoir les messages au fil de l'eau. Or un hébergement web mutualisé classique (PHP/MySQL) exécute un script à chaque requête HTTP puis le termine. Il ne peut pas :

- maintenir une souscription MQTT permanente ;
- faire tourner un démon en tâche de fond ;
- écouter sur un port autre que 80/443 ;
- encore moins héberger le broker lui-même.

Il manque donc **une pièce indispensable : un processus permanent** qui souscrit au broker et écrit en base. Cette pièce doit tourner quelque part en continu — et « quelque part » signifie précisément soit le mini PC, soit un VPS. C'est pour cette raison que l'assemblage « site mutualisé + broker externe » ne se referme pas : il n'existe pas de configuration qui évite cette machine.

**Une seule variante permettrait un site mutualisé**, et il faut la connaître pour l'écarter en connaissance de cause : certains brokers gérés proposent un pont HTTP sortant (souvent appelé *data integration* ou *webhook*), qui appelle une URL à chaque message reçu. Un site mutualisé pourrait alors recevoir ces appels et les stocker. Les limites sont sérieuses : la voie descendante (envoyer une commande vers le module) reste malcommode, la fonctionnalité et sa tarification varient selon le fournisseur, et l'architecture devient dépendante de ce fournisseur — ce que `SYSTEM_ARCHITECTURE.md` demande explicitement d'éviter (« rendre les contrats de messages indépendants du fournisseur de broker »). À réserver à un prototype jetable, pas à la cible.

## 3. Contrainte matérielle à intégrer dès maintenant — issue du REX du 16 août 2026

Point important, découvert le jour même et qui pèse directement sur la faisabilité de la Phase A sur le matériel actuel.

La journée a montré que la RAM interne du module est le facteur limitant : deux sprites d'affichage occupant ~95 Ko ne laissaient qu'environ 32 Ko libres et un plus gros bloc contigu de ~17 Ko, au point qu'une simple poignée de main TLS échouait (`SSL - Memory allocation failed`) et que trois requêtes HTTP simultanées suffisaient à rendre le serveur muet.

Or **MQTT/TLS suppose une connexion TLS permanente**, et non ponctuelle comme les vérifications HTTPS déjà en place. Les tampons TLS resteraient donc alloués *en continu*, et non le temps d'un téléchargement. Conséquences à anticiper :

- la marge mémoire actuelle, déjà mince écran allumé, serait durablement amputée ;
- l'atténuation trouvée aujourd'hui (libérer les sprites en veille) ne s'applique pas : on ne peut pas libérer une connexion permanente ;
- cela renforce nettement l'intérêt d'une carte disposant de PSRAM, déjà en cours d'évaluation, et en fait probablement un **prérequis** de la Phase A plutôt qu'un simple confort.

**Recommandation : trancher l'évaluation PSRAM avant d'engager le client MQTT sur le firmware.** Construire l'infrastructure côté serveur peut se faire en parallèle sans risque — elle ne dépend pas de ce choix — mais le raccordement du module devrait l'attendre. À défaut, prévoir de mesurer précisément l'empreinte mémoire d'une session MQTT/TLS maintenue avant toute intégration durable.

## 3 bis. Télémétrie et supervision prédictive — objectif affirmé le 16 août 2026

Exigence exprimée : les journaux doivent remonter vers le serveur afin d'**anticiper et prédire** le comportement des modules, en mode proactif — et non constater les pannes après coup.

### Pourquoi c'est fondé, et pas une élégance

Les deux pannes du 16 août 2026 étaient l'une et l'autre **précédées d'indicateurs mesurables**, et toutes deux ont pourtant été découvertes par la panne :

- **saturation NVS** : les entrées libres de la partition diminuaient à mesure que des namespaces s'ajoutaient. Le seuil a été franchi sans que rien ne le signale, et la première manifestation a été la perte silencieuse d'un réglage, puis celle des identifiants WiFi ;
- **effondrement du serveur Web** : le plus gros bloc mémoire contigu était descendu à ~17 Ko, et le minimum de tas atteint à ~650 octets. La première manifestation a été « la page ne se charge pas », attribuée à tort au WiFi pendant plusieurs heures.

Dans les deux cas, un suivi de tendance aurait alerté **avant** l'incident. C'est exactement ce que cette exigence vise.

### Les indicateurs précurseurs identifiés

Repérés à l'usage, ils forment le socle de ce qu'il faut remonter. À noter : ce sont des **tendances** qui portent l'information, pas des valeurs instantanées.

| Indicateur | Ce qu'il annonce | Remarque |
|---|---|---|
| **Plus gros bloc libre contigu** | fragmentation mémoire | **meilleur prédicteur que le tas libre total** — c'est lui qui décide si une allocation aboutit |
| Minimum de tas atteint | marge réelle sous charge | valeur cumulée depuis le démarrage, très parlante |
| Entrées NVS libres | saturation de la configuration | seuil franchi = pertes silencieuses |
| Échecs de sauvegarde de configuration | perte de réglages | doit remonter comme **événement**, pas comme métrique |
| Dépassements de boucle | **à ne pas utiliser tel quel** | mesuré le 17 août 2026 : ~1 dépassement par requête HTTP servie, avant comme après les modifications du 16 août (test comparatif sur deux versions). Le compteur suit le **trafic**, pas la santé — un onglet de navigateur laissé ouvert produisait 500/heure sans le moindre symptôme. La mesure étant en temps écoulé, elle compte la préemption normale par la tâche réseau. À remplacer par une mesure de temps **processeur**, ou à normaliser par le nombre de requêtes |
| Événements de connexion zombie WiFi | qualité réseau du site | fréquence plus informative que l'occurrence |
| Incidents carte SD | usure ou défaut du support | |
| Redémarrages et leur cause | instabilité | un redémarrage non sollicité est toujours un signal |

### Principe de conception : des métriques bornées, pas des journaux bruts

Remonter les journaux tels quels serait une erreur : volume important, coût réseau et mémoire sur un module déjà contraint, et faible densité d'information. L'approche retenue :

- **métriques périodiques**, structurées et bornées, sur `aqualook/v1/<moduleId>/diag` (arborescence déjà définie) ;
- **événements à la survenue** pour ce qui est ponctuel et important — échec de persistance, redémarrage, incident SD, zombie WiFi ;
- **extraits de journal uniquement sur incident**, en rafale bornée, jamais en flux continu ;
- fréquence et taille limitées côté module, conformément à l'exigence existante de préservation de sa stabilité.

Le stockage est déjà prévu (hypertable `module_message` en TimescaleDB) et la visualisation aussi (Grafana). L'essentiel du travail est donc côté module et côté définition des seuils.

### Le prototype existe déjà

La campagne de surveillance nocturne mise en place le 16 août 2026 (`logs/nuit-260816.log`) échantillonne précisément ces indicateurs à intervalle régulier, mesure le tas **au repos** pour disposer d'une série comparable, et n'alerte que sur franchissement de seuil. C'est, à la main et pour un seul module, exactement ce que la télémétrie doit automatiser pour une flotte. Les seuils qui s'y révéleront pertinents seront réutilisables tels quels.

### Précaution

Les journaux du module contiennent des éléments qui ne doivent pas partir sans y avoir pensé : nom du réseau WiFi, adresses IP locales, cible keepalive. Définir explicitement ce qui est transmis avant d'ouvrir le flux, plutôt que de filtrer après coup.

## 4. Ce qu'il faut installer sur le mini PC

### Socle

- **Système** : une distribution Linux serveur stable et à support long — Debian stable ou Ubuntu LTS. Éviter une distribution à cycle rapide pour une machine censée tourner en continu.
- **Docker et Docker Compose** : tout le reste tourne en conteneurs. C'est ce qui rend la pile transposable telle quelle sur un VPS, et ce qui évite d'installer des services directement sur la machine (désinstallation propre, versions maîtrisées, sauvegarde par volumes).

### Services de la pile

| Service | Rôle | Remarque |
|---|---|---|
| **Mosquitto** | broker MQTT | déjà nommé dans la roadmap Phase C ; TLS + identifiants par module |
| **PostgreSQL + TimescaleDB** | base et historique | TimescaleDB est une extension de PostgreSQL orientée séries temporelles, adaptée aux mesures horodatées ; PostgreSQL seul suffit pour démarrer |
| **Service passerelle** | souscrit au MQTT, écrit en base, expose une API | c'est *la pièce manquante* de la section 2 ; Python (FastAPI + paho-mqtt) ou Node.js |
| **Caddy** | reverse proxy et TLS | obtient et renouvelle les certificats Let's Encrypt automatiquement, configuration très courte |
| **Grafana** | tableaux de bord | très rentable au début : visualiser l'historique sans écrire d'interface |
| **Node-RED** | prototypage, scénarios, diagnostics | prévu par la roadmap ; permet de tester des flux sans coder |

### Accès depuis l'extérieur

Sujet à traiter explicitement, car c'est là que se jouent la sécurité et la différence réelle avec un VPS.

- **Ne pas ouvrir de ports** de la box vers le mini PC pour commencer. C'est la solution qui semble la plus simple et c'est celle qui expose le plus.
- Préférer un **tunnel sortant** — type Cloudflare Tunnel — ou un **réseau privé maillé** — type Tailscale/WireGuard — selon le besoin : le tunnel convient pour publier une interface web accessible publiquement, le réseau maillé pour un accès réservé à ses propres appareils.
- C'est précisément sur ce point qu'un VPS devient intéressant plus tard : adresse publique stable, certificats simples, aucune exposition du réseau domestique.

### Sauvegardes

À prévoir dès l'installation, pas après : volumes Docker et export régulier de la base. Une infrastructure d'historique sans sauvegarde perd sa raison d'être au premier incident — la journée du 16 août a montré concrètement ce que coûte une perte de configuration non anticipée.

## 5. Trajectoire proposée

1. **Socle serveur** — mini PC : Docker + Mosquitto + Node-RED + Grafana. Objectif : voir arriver des messages MQTT et les afficher, sans toucher au firmware (on peut publier des messages de test depuis un client de bureau).
2. **Historique** — ajouter PostgreSQL/TimescaleDB et le service passerelle. Objectif : les messages sont persistés et consultables.
3. **Raccordement du module** — Phase A de la roadmap, **après arbitrage PSRAM** (section 3). Contrats de topics versionnés dès le premier message.
4. **Accès distant** — tunnel ou réseau maillé, puis application Flutter (Phase B).
5. **Bascule VPS** — Phase C : le même `docker-compose.yml` est déployé sur le VPS, les données sont migrées, les contrats MQTT restent inchangés.

L'intérêt de cet ordre est qu'aucune étape ne rend la suivante plus coûteuse, et que les trois premières peuvent être menées sans dépendre de la décision matérielle en cours.

## 6. Points à arbitrer, non tranchés ici

- **HiveMQ Cloud ou Mosquitto local pour la Phase A ?** La roadmap prévoit HiveMQ Cloud. Un Mosquitto local est plus simple à itérer (le module est sur le même réseau) mais ne teste pas le trajet distant réel. Les deux sont peu coûteux ; le choix dépend de ce qu'on veut valider en premier, la mécanique MQTT ou l'accès distant.
- **TimescaleDB ou InfluxDB** : deux approches valables pour des séries temporelles. PostgreSQL/TimescaleDB a l'avantage de rester du SQL classique, réutilisable pour les données non temporelles (utilisateurs, sites, modules).
- **Dimensionnement du VPS** : à déduire des mesures faites sur le mini PC, pas à estimer à l'avance.
