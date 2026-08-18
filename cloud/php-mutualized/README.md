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

php -S 127.0.0.1:8001 index.php
curl http://127.0.0.1:8001/health
```

Le serveur de développement intégré de PHP (`php -S`) route toutes les requêtes
vers `index.php`, comme le fera `.htaccess` sur l'hébergement réel — pas besoin
d'Apache local pour tester.

## Ce qui n'est pas fait

Le module ESP32 ne parle pas encore à ce service (ni à `cloud/api`) — prochaine
étape naturelle, un client HTTP côté firmware réutilisant le patron
`MaintenanceRequest`/`MaintenanceResult` déjà éprouvé sur l'OTA.
