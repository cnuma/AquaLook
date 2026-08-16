# Pile serveur AquaLook — « VPS maison »

Configuration logicielle à déployer sur le mini PC. La même pile est destinée à être transposée telle quelle sur un VPS (Phase C de la roadmap) : c'est la raison d'être de la conteneurisation ici, et non un goût pour Docker.

Contexte et justification des choix : `docs/architecture/CLOUD_ENVIRONMENT_EVALUATION.md`. Invariants d'architecture et de sécurité : `docs/architecture/SYSTEM_ARCHITECTURE.md`.

## Ce que fait cette pile

| Service | Rôle |
|---|---|
| `mosquitto` | broker MQTT, TLS, un compte et des droits par module |
| `db` | PostgreSQL + TimescaleDB — référentiel et historique |
| `bridge` | souscription MQTT permanente → écriture en base + API de lecture |
| `caddy` | reverse proxy, certificats Let's Encrypt automatiques |
| `grafana` | tableaux de bord |
| `nodered` | prototypage de flux et diagnostics |

Le service `bridge` est la pièce centrale : c'est le processus permanent qu'un hébergement mutualisé ne peut pas fournir, et sans lequel l'assemblage « site web + MQTT externe » ne se referme pas.

## Prérequis sur le mini PC

- une distribution Linux serveur stable à support long (Debian stable ou Ubuntu LTS) ;
- Docker et le plugin Compose.

Sur Debian/Ubuntu, l'installation recommandée est celle du dépôt officiel Docker (`docs.docker.com/engine/install/`) : les paquets de la distribution sont souvent en retard. Ajouter ensuite son utilisateur au groupe `docker` pour éviter `sudo` à chaque commande, puis se reconnecter.

## Mise en route

### 1. Paramètres

```bash
cd cloud
cp .env.example .env
# renseigner DOMAIN, TZ et générer les mots de passe :
#   openssl rand -base64 24
```

`.env` n'est jamais versionné (voir `.gitignore`).

### 2. Comptes MQTT

Un compte par module, plus celui de la passerelle. Le fichier de mots de passe est créé par Mosquitto lui-même :

```bash
# compte de la passerelle (valeur identique à BRIDGE_MQTT_PASSWORD du .env)
docker run --rm -v "$PWD/mosquitto/config:/mosquitto/config" eclipse-mosquitto:2 \
  mosquitto_passwd -c -b /mosquitto/config/passwd aqualook-bridge 'MOT_DE_PASSE'

# un compte par module — le nom d'utilisateur DOIT être l'identifiant du module,
# c'est lui que l'ACL substitue à %u
docker run --rm -v "$PWD/mosquitto/config:/mosquitto/config" eclipse-mosquitto:2 \
  mosquitto_passwd -b /mosquitto/config/passwd aq-0001 'MOT_DE_PASSE_MODULE'
```

Attention : `-c` **écrase** le fichier. Ne l'utiliser que pour le tout premier compte.

### 3. Démarrage

```bash
docker compose up -d
docker compose ps
docker compose logs -f bridge
```

Vérification rapide, depuis la machine elle-même :

```bash
curl -s localhost:8000/health          # via le conteneur bridge exposé par Caddy
docker compose exec bridge python -c "print('ok')"
```

### 4. TLS pour MQTT

Ordre imposé : Caddy doit avoir obtenu le certificat **avant** que Mosquitto puisse s'en servir.

1. démarrer d'abord la pile sans l'écouteur 8883 (commenter le bloc `listener 8883` de `mosquitto/config/mosquitto.conf`) ;
2. laisser Caddy obtenir le certificat pour `DOMAIN` — vérifier avec `docker compose logs caddy` ;
3. relever le chemin réel du certificat dans le volume :
   ```bash
   docker compose exec mosquitto ls -R /caddy-certs/caddy/certificates
   ```
4. remplacer les deux `CHANGEME_DOMAIN` de `mosquitto.conf` par le chemin observé, décommenter l'écouteur, puis `docker compose restart mosquitto`.

**Pourquoi Let's Encrypt plutôt qu'une autorité privée** : la chaîne Let's Encrypt remonte à ISRG Root X1, **déjà présente dans le magasin de confiance du firmware** (`src/OtaTlsTrust.h`, ajoutée pour l'OTA). Le module fera donc confiance au broker sans embarquer de certificat supplémentaire, et sans qu'il faille gérer le renouvellement d'une autorité maison — sujet notoirement pénible sur un parc de modules.

### 5. Accès depuis l'extérieur

Ne pas ouvrir de ports de la box vers le mini PC pour commencer : c'est la solution qui paraît la plus simple et c'est celle qui expose le plus.

- **Tunnel sortant** (type Cloudflare Tunnel) si l'interface doit être publiquement joignable ;
- **réseau privé maillé** (type Tailscale/WireGuard) si l'accès peut rester réservé à ses propres appareils.

À noter : un tunnel HTTP ne transporte pas MQTT. Tant que le module et le serveur sont sur le même réseau local, la question ne se pose pas. Dès que le module doit joindre le broker depuis l'extérieur, il faut soit exposer le port 8883, soit passer au VPS — c'est précisément là que le VPS devient réellement intéressant.

## Arborescence des topics

Version de protocole en tête, conformément à l'exigence de contrats stables :

```
aqualook/v1/<moduleId>/status     présence (retenu, avec Last Will)
aqualook/v1/<moduleId>/state      état courant (retenu)
aqualook/v1/<moduleId>/event      événements (début/fin d'arrosage, incidents)
aqualook/v1/<moduleId>/diag       diagnostics techniques
aqualook/v1/<moduleId>/cmd        demandes de commande, serveur → module
aqualook/v1/<moduleId>/cmd/ack    acquittements, module → serveur
```

Chaque message est un objet JSON comportant au minimum un horodatage, un type et, pour les commandes et leurs acquittements, un `correlationId`.

Les droits sont posés dans `mosquitto/config/acl` : un module écrit uniquement dans son sous-arbre et lit uniquement ses commandes. Un module compromis ne peut donc ni observer les autres, ni leur donner d'ordres.

## Sauvegardes

À mettre en place dès l'installation, pas après. Le minimum :

```bash
docker compose exec -T db pg_dump -U aqualook aqualook | gzip > sauvegarde-$(date +%F).sql.gz
```

À automatiser (tâche planifiée) et à copier hors de la machine. Une infrastructure d'historique sans sauvegarde perd sa raison d'être au premier incident.

## Migration vers un VPS (Phase C)

1. installer Docker sur le VPS ;
2. y copier ce dossier et le `.env` ;
3. restaurer le dump de base ;
4. faire pointer `DOMAIN` vers le VPS ;
5. `docker compose up -d`.

Les contrats MQTT ne changent pas : côté firmware, seuls l'adresse du broker et éventuellement les identifiants sont à mettre à jour — ce que la roadmap exige explicitement (« la migration ne doit pas imposer de réécriture du firmware »).

## Points de vigilance

- **Épingler les versions d'images** avant une mise en service réelle. Les tags majeurs utilisés ici (`eclipse-mosquitto:2`, `caddy:2`) suivent les correctifs, ce qui est souhaitable en développement mais laisse la porte ouverte à un changement inattendu sur une machine qui tourne en continu.
- **Node-RED n'est pas exposé** par défaut dans le `Caddyfile`. Ne l'ouvrir qu'avec une authentification supplémentaire : il permet d'exécuter du code arbitraire.
- **L'écouteur MQTT en clair (1883)** est destiné aux conteneurs voisins et au réseau local. À retirer une fois le TLS validé si le broker doit être joignable de l'extérieur.
- **Contrainte firmware préalable** : MQTT/TLS implique une session TLS *permanente*. Les mesures du 16 août 2026 ont montré que la RAM interne du module est déjà le facteur limitant. Voir `CLOUD_ENVIRONMENT_EVALUATION.md`, section 3 — l'arbitrage sur une carte à PSRAM devrait précéder l'intégration du client MQTT au firmware.
