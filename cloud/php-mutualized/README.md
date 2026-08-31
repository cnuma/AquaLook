# API AquaLook — PHP + MySQL, pour hébergement mutualisé OVH

Même contrat d'API que `cloud/api` (FastAPI, piste VPS) — mêmes routes, même schéma
logique — mais réécrit pour une contrainte réelle : **l'hébergement mutualisé OVH
exécute du PHP par requête, il ne fait pas tourner de processus Python permanent
comme Uvicorn.** Ce n'est pas une préférence, c'est structurel à ce type d'offre.

Contexte et arbitrage complet : `docs/architecture/SYSTEM_ARCHITECTURE.md` §5.0,
`docs/architecture/CLOUD_ENVIRONMENT_EVALUATION.md` §7.

## Pourquoi PHP convient bien ici

Le transport retenu (HTTP à jeton, connexions courtes, module toujours à l'initiative)
correspond exactement à ce que PHP fait le mieux : un script s'exécute, répond, se
termine. Aucun processus à tenir en vie, contrairement à ce qu'exigeait MQTT — c'est
précisément la contrainte qui avait fait écarter cette piste le 16 août 2026
(`CLOUD_ENVIRONMENT_EVALUATION.md` §2), et qui n'existe plus.

## Routes

Identiques à `cloud/api` (voir son README pour le détail du raisonnement) :

| Route | Sens | Auth |
|---|---|---|
| `GET /health` | — | aucune |
| `POST /v1/report` | module → serveur | jeton module |
| `GET /v1/pending-command` | module → serveur | jeton module |
| `POST /v1/command/ack` | module → serveur | jeton module |
| `POST /admin/module-token` | admin | jeton admin |
| `POST /admin/command` | admin | jeton admin |
| `GET /admin/modules` | admin | jeton admin |

Routes ajoutées pour la console d'administration (lecture, sauf l'annulation) :

| Route | Rôle |
|---|---|
| `GET /admin/messages?moduleId=&limit=&type=` | historique des remontées d'un module |
| `GET /admin/commands?moduleId=&limit=` | file et historique des commandes |
| `GET /admin/config?moduleId=` | dernier instantané de configuration, pour lire `revision` |
| `POST /admin/command/cancel` | annule une commande **encore en attente** |
| `GET /admin` | redirige vers la console (`admin.html`) |

`POST /admin/command/cancel` répond `409` si la commande est déjà réglée : le
module l'a alors appliquée, et réécrire son état effacerait la trace de ce qui
s'est réellement passé. Une commande partie ne se rattrape pas côté serveur.

## Console d'administration

`admin.html` — page autonome (aucune dépendance, aucun CDN), servie en statique
par Apache. Elle consomme les routes `/admin/*` ci-dessus.

Elle permet de : lister les modules et leur fraîcheur, déclarer un module et
générer son jeton, lire les remontées et l'historique des commandes, composer
une commande `config.apply` à partir de modèles, et annuler une commande en
attente.

Le jeton administrateur est saisi dans la page et conservé en `sessionStorage`
— il disparaît à la fermeture de l'onglet. Ce n'est pas un système de session :
c'est le même jeton porteur que l'API, transporté par le navigateur. Le choix
est assumé pour un administrateur unique ; il devra être revu si l'accès doit
être partagé ou tracé par personne.

La console renseigne `baseRevision` automatiquement depuis le dernier
instantané de configuration remonté par le module. C'est délibéré : le firmware
refuse toute commande dont la `baseRevision` ne correspond pas à sa révision
courante (verrouillage optimiste, `CloudSync.cpp`), et ce refus est silencieux
du point de vue de l'administrateur. Faire saisir ce nombre à la main serait
une invitation à l'erreur.

## Schéma

`schema.sql` — mêmes quatre tables (`module`, `module_token`, `module_message`,
`command`) que les autres implémentations, adaptées à MySQL/MariaDB (type `JSON`
natif, `AUTO_INCREMENT`, moteur `InnoDB`). Migration triviale entre implémentations
si un jour nécessaire : la forme logique ne change pas.

## Sécurité

Mêmes principes que `cloud/api` : jeton porteur par module, jeton admin distinct,
charge utile bornée à 64 Ko, accusé sur commande déjà réglée = no-op (anti-rejeu,
vérifié par test réel). Comparaison de jetons via `hash_equals()` (temps constant,
évite une fuite par mesure de temps). `.env` et `schema.sql` explicitement bloqués
par `.htaccess` en cas d'accès direct.

## Déploiement sur AlwaysData

C'est l'hébergement réellement utilisé (`aqualook.alwaysdata.net`, qui sert déjà
les ressources Web du module). HTTPS y est fourni et renouvelé automatiquement
par Let's Encrypt, sur le domaine `*.alwaysdata.net`.

1. **Base MySQL** — dans l'administration AlwaysData, *Bases de données →
   MySQL → Ajouter une base*. Noter les quatre valeurs : hôte
   (`mysql-<compte>.alwaysdata.net`), nom de la base, utilisateur, mot de passe.
2. **Type de site** — *Web → Sites*, le site doit être de type **PHP**, pas
   Node.js. Une erreur déjà commise : un site déclaré Node.js répond 502 sur
   tout, y compris les fichiers statiques.
3. **Dépôt des fichiers** — envoyer le contenu de `cloud/php-mutualized/` dans
   le répertoire racine du site. **Transfert en mode binaire** : le mode ASCII
   réécrit les fins de ligne et a déjà corrompu des empreintes SHA-256.
4. **`.env`** — copier `.env.example` en `.env`, y reporter les identifiants de
   l'étape 1 et générer un jeton :
   `php -r "echo bin2hex(random_bytes(32));"`
5. **Schéma** — importer `schema.sql` via phpMyAdmin (fourni par AlwaysData).
   ⚠️ `schema.sql` n'est **pas** une migration : il ne contient que des
   `CREATE TABLE IF NOT EXISTS`. Sur une base neuve il fait le travail ; sur une
   base existante il ne modifie rien, silencieusement. Le piège s'est déjà
   refermé en local, où une table `module_token` gardait l'ancienne colonne
   `token` en clair alors que le code attendait `token_sha256`.
6. **Vérifications** — `https://aqualook.alwaysdata.net/health` doit répondre
   `{"ok":true}`, et `https://aqualook.alwaysdata.net/.env` doit répondre 403.

Le site sert déjà les ressources Web sous `/web/v<version>/`. L'API et la
console cohabitent avec elles sans conflit : le `.htaccess` ne réécrit que ce
qui n'existe pas sur disque.

## Déploiement sur OVH — hébergement mutualisé

1. Créer la base MySQL depuis l'espace client OVH (Hébergements → Bases de données).
   Noter hôte, port, nom, utilisateur, mot de passe fournis.
2. Choisir la version PHP dans l'espace client (Hébergements → Multisite → PHP) —
   ce code vise PHP 8.1+ (`str_starts_with`, `never` en type de retour).
3. Déposer ce dossier (`cloud/php-mutualized/`) à la racine du sous-domaine choisi,
   via FTP/SFTP ou Git si l'offre le permet.
4. Copier `.env.example` en `.env`, renseigner les identifiants MySQL de l'étape 1
   et générer un `ADMIN_TOKEN` (`php -r "echo bin2hex(random_bytes(32));"`).
5. Importer `schema.sql` (phpMyAdmin fourni par OVH, ou `mysql < schema.sql` en
   SSH si l'offre le permet).
6. Vérifier `https://<sous-domaine>/health` répond `{"ok":true}`.

OVH mutualisé fournit HTTPS (Let's Encrypt) nativement sur les sous-domaines — pas
besoin d'un Caddy séparé comme pour la piste VPS.

## Développement local (Windows, sans Docker ni droits admin)

Deux blocages rencontrés en le mettant en place, réglés sans élévation :

- **MariaDB** : l'installeur MSI officiel exige une élévation UAC (bloquant sans
  interaction humaine). Utiliser l'**archive ZIP portable** à la place :
  ```powershell
  # Extraire mariadb-*-winx64.zip (depuis mariadb.org) hors du depot, par ex. :
  #   C:\Users\<vous>\mariadb-local\
  cd C:\Users\<vous>\mariadb-local\mariadb-*-winx64
  .\bin\mariadb-install-db.exe --datadir="C:\Users\<vous>\mariadb-local\data"
  .\bin\mariadbd.exe --datadir="C:\Users\<vous>\mariadb-local\data" --port=3307 --console
  ```
  Port `3307` plutôt que le `3306` par défaut, pour ne pas entrer en conflit avec
  une éventuelle installation existante — ajuster `.env` en conséquence.

- **`pdo_mysql` désactivé par défaut** dans le PHP installé via `winget` : copier
  `php.ini-development` en `php.ini` dans le dossier d'installation PHP, puis
  décommenter `extension_dir = "ext"`, `extension=pdo_mysql` et `extension=mysqli`.

Ensuite :

```powershell
cd cloud\php-mutualized
Copy-Item .env.example .env
# éditer .env : DB_PORT=3307 (ou celui choisi), identifiants créés dans MariaDB,
# ADMIN_TOKEN généré

# créer la base et un utilisateur dédié, une fois :
#   CREATE DATABASE aqualook CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
#   CREATE USER 'aqualook_user'@'127.0.0.1' IDENTIFIED BY '...';
#   GRANT ALL PRIVILEGES ON aqualook.* TO 'aqualook_user'@'127.0.0.1';
mysql -h 127.0.0.1 -P 3307 -u root aqualook < schema.sql

php -S 127.0.0.1:8001 router.php
curl http://127.0.0.1:8001/health
# console : http://127.0.0.1:8001/admin.html
```

Utiliser `router.php` et **non** `index.php` comme routeur. `php -S ... index.php`
envoie toutes les requêtes au routeur JSON, y compris `admin.html` : la console
devient alors inaccessible en local alors qu'elle fonctionne en production —
la pire forme de divergence entre les deux environnements. `router.php`
reproduit exactement le comportement du `.htaccess` : fichier existant servi tel
quel, tout le reste vers `index.php`, et `.env`/`schema.sql` refusés.

## Ce qui n'est pas fait

- **Limitation de débit** sur les routes admin et module. Le service est exposé
  sur l'Internet public ; rien n'y freine aujourd'hui une tentative répétée.
- **Traçabilité par personne** : un unique `ADMIN_TOKEN` partagé, donc aucune
  distinction entre administrateurs dans `command.issued_by`.
- **Purge de l'historique** : `module_message` grossit sans limite. Un module
  qui se synchronise tous les quarts d'heure produit environ 100 messages par
  jour ; rien ne les élague.
- **Déclenchement d'une mise à jour depuis le serveur** : le firmware
  n'accepte qu'un seul type de commande, `config.apply` (`CloudSync.cpp`,
  `applyCommand`). Tout autre type est explicitement refusé. Lancer une mise à
  jour reste donc une action locale — voir la note ci-dessous.

## Comment se lance une mise à jour

Trois chaînes distinctes, à ne pas confondre.

| | Ressources Web (`/www` sur SD) | Firmware (OTA) | Dépôt direct |
|---|---|---|---|
| Ce qui change | HTML/CSS/JS | le binaire | HTML/CSS/JS |
| Déclencheur | `POST /api/webassets/update` | `POST /api/maintenance/*` | `POST /api/debug/deploy-*` |
| Source | manifeste HTTPS configurable | **GitHub uniquement** | le poste qui pousse |
| Redémarrage | oui, mode maintenance | oui, mode maintenance | non |

Aucune de ces chaînes n'est déclenchable depuis ce serveur aujourd'hui : toutes
partent d'une requête vers le module lui-même, donc depuis son réseau local.
C'est le principal manque à combler pour administrer un module réellement
distant, et cela demande d'ajouter un type de commande côté firmware.
