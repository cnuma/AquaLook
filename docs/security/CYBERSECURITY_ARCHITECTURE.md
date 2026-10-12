# AquaLook — Architecture cybersécurité

## 1. Statut

La cybersécurité est un pilier permanent d'AquaLook. Elle s'applique dès la conception, pendant le développement, au déploiement, en exploitation, lors des mises à jour et jusqu'à la fin de vie des composants.

Ce document définit les principes directeurs. Les choix techniques précis doivent être confirmés par des ADR et validés sur le matériel et l'infrastructure réellement retenus.

C'est le **référentiel de sécurité du projet** : toute évolution (firmware, pages Web, service en ligne, outils) s'y conforme et en profite. Les mesures déjà en service sont décrites au §4.8 (service en ligne) et dans `docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md` (module et réseau local).

### 1.1 Des règles vivantes, jamais figées

Décision du propriétaire (12 octobre 2026, D017) : **aucune règle de ce document n'est acquise une fois pour toutes.** Les menaces, les failles connues et les bonnes pratiques changent en permanence ; une règle juste aujourd'hui peut devenir insuffisante, voire dangereuse, demain. Chaque règle est donc remise en cause en continu et évolue avec les nouvelles découvertes.

Concrètement :

- **revue à chaque évolution** touchant le réseau, l'authentification, les secrets, les pages Web, le service en ligne, l'OTA ou les commandes d'équipements : vérifier que les règles appliquées sont toujours l'état de l'art, pas seulement qu'elles sont respectées ;
- **revue à chaque alerte** : faille publiée (CVE) sur une dépendance ou une technologie utilisée (ESP-IDF, Arduino, AsyncTCP/ESPAsyncWebServer, PHP, MariaDB, Apache, libsodium, navigateurs), nouvelle technique d'attaque, incident chez l'hébergeur ;
- **revue périodique**, au moins une fois par trimestre, même sans évolution : relire ce document et le registre des risques, relancer `tools/web_security_probe.py`, comparer aux référentiels publics ;
- **référentiels à consulter**, dans leur version courante : OWASP (Top 10, ASVS, Cheat Sheets), ANSSI et CERT-FR (avis et alertes), bases CVE/NVD, guide de sécurité de l'hébergeur, recommandations Mozilla pour les en-têtes HTTP et la CSP, avis de sécurité d'Espressif ;
- **une règle devenue insuffisante se corrige** : nouvelle mesure, nouveau risque au registre ou réévaluation d'un risque existant, entrée datée dans `docs/codex/02_DECISIONS.md` si elle change une décision, et mise à jour de la date de revue ci-dessous ;
- **un choix technique reste justifié** : chaque mesure du §4.8 dit pourquoi elle existe, pour qu'on puisse juger si cette raison tient encore.

| Revue | Date | Portée | Résultat |
|---|---|---|---|
| Audit statique et tests actifs du site | 10-11 oct. 2026 | service en ligne | `docs/security/AUDIT_WEB_2026-10-10.md` |
| Mesures P0.4b à P0.4e | 11-12 oct. 2026 | service en ligne | §4.8 ci-dessous |
| Vérification après publication | 12 oct. 2026 | service en ligne | P0.4b à P0.4e en ligne ; `web_security_probe.py` : tout PASS ; parcours navigateur validé par le propriétaire |
| Prochaine revue périodique | au plus tard 12 janv. 2027 | tout le référentiel | — |

## 2. Objectifs

- empêcher une activation non autorisée des équipements ;
- protéger les secrets, identités et données de configuration ;
- garantir l'intégrité et l'origine des firmwares ;
- limiter l'exposition réseau ;
- conserver un fonctionnement local sûr en cas d'attaque ou de panne distante ;
- détecter, tracer et traiter les événements anormaux ;
- permettre la révocation d'un équipement, d'un utilisateur ou d'un certificat compromis ;
- maintenir la sécurité malgré l'évolution des dépendances.

## 3. Modèle de confiance

### 3.1 Principe général

Aucun composant n'est implicitement digne de confiance parce qu'il se trouve sur le réseau local.

Chaque action sensible doit répondre à quatre questions :

1. quelle identité demande l'action ?
2. cette identité est-elle authentifiée ?
3. possède-t-elle l'autorisation exacte ?
4. l'action et son résultat sont-ils traçables ?

### 3.2 Domaines

- domaine embarqué : ESP32 principal et extensions ;
- domaine local : navigateur, Wi-Fi, point d'accès, réseau domestique ;
- domaine distant : VPS, broker MQTT, notifications, GitHub OTA ;
- domaine utilisateur : application Flutter, navigateur, comptes ;
- domaine développement : dépôt Git, CI, postes de développement, secrets de publication.

## 4. Exigences par composant

### 4.1 Firmware ESP32

- désactiver les services inutiles ;
- limiter les routes et commandes administratives ;
- séparer les commandes de consultation et de modification ;
- vérifier les entrées, tailles, bornes et états avant action ;
- refuser en sécurité toute commande invalide ;
- ne jamais journaliser un secret en clair ;
- évaluer Secure Boot et Flash Encryption selon la carte, la chaîne OTA et les contraintes de maintenance ;
- conserver les limites de durée et les sécurités relais indépendamment du réseau.

### 4.2 Wi-Fi et interface locale

- aucun mot de passe universel par défaut ;
- identifiants propres à chaque appareil ;
- point d'accès de secours protégé et signalé ;
- délai, limitation et journalisation des échecs d'authentification ;
- session administrative explicite ;
- protection CSRF et validation d'origine à prévoir pour les actions sensibles ;
- HTTPS à privilégier dès que la mémoire, le certificat et l'architecture le permettent.

Le verrouillage visuel actuel côté navigateur ne constitue pas une authentification forte.

### 4.3 MQTT

- TLS obligatoire hors laboratoire isolé ;
- identité distincte par appareil ;
- ACL par appareil et par topic ;
- commandes séparées des télémétries ;
- messages de commande avec identifiant, horodatage, expiration et anti-rejeu ;
- accusé de réception fondé sur l'état réellement appliqué ;
- révocation individuelle d'un appareil compromis ;
- limitation de débit et de taille des messages.

### 4.4 OTA

- firmware signé ;
- vérification d'intégrité avant activation ;
- provenance de la version vérifiée ;
- partitions doubles et rollback conservés ;
- version minimale autorisée pour empêcher le retour vers une version vulnérable, après étude de la procédure de secours ;
- journalisation du téléchargement, de la validation, du basculement et du rollback ;
- aucun secret de publication stocké dans le firmware ou la carte SD.

### 4.5 VPS et services cloud

- SSH par clé ;
- authentification par mot de passe désactivée lorsque possible ;
- pare-feu restrictif ;
- comptes de service séparés ;
- mises à jour de sécurité suivies ;
- sauvegardes testées ;
- journaux centralisés et rotation ;
- supervision des ressources, connexions et certificats ;
- secrets hors dépôt Git ;
- exposition publique limitée aux ports nécessaires.

### 4.6 Application Flutter

- jetons stockés dans le stockage sécurisé du système ;
- sessions expirables et révocables ;
- aucune clé globale embarquée dans l'application ;
- validation TLS stricte ;
- distinction des rôles utilisateur ;
- confirmation renforcée pour les actions critiques ;
- affichage de l'identité de l'équipement ciblé et du résultat réel.

### 4.7 Développement et chaîne logicielle

- secrets interdits dans Git, les ZIP, logs et checkpoints ;
- dépendances inventoriées et versions maîtrisées ;
- revue des alertes de vulnérabilité ;
- commits et releases traçables ;
- artefacts de publication identifiés par SHA-256 et version ;
- permissions GitHub minimales ;
- procédure de rotation immédiate après fuite présumée.

### 4.8 Service en ligne mutualisé (AlwaysData) — mesures en vigueur

Mis à jour le 12 octobre 2026. Code : `cloud/php-mutualized/`. État de chaque mesure : **en ligne** (vérifiée sur le site), **fusionnée** (dans `main`, publication à confirmer) ou **branche** (testée sur banc, pas encore fusionnée). Ces mesures sont réexaminées selon le §1.1.

**Principe : le code est public, la sécurité ne repose pas sur son secret.** Le dépôt reste open source (choix du propriétaire). Tout ce qu'un navigateur exécute (HTML, JS, CSS) est lisible par n'importe qui, humain ou IA, et doit être conçu comme tel. La sécurité repose sur les secrets et les contrôles côté serveur. L'obfuscation du code n'est pas une mesure de sécurité.

| Mesure | Pourquoi | Où | État |
|---|---|---|---|
| Secrets uniquement dans le `.env` du serveur (`DB_*`, `ADMIN_TOKEN`, `SETTINGS_KEY`, `ADMIN_PAGE_*`) ; `.env.example` sans valeur réelle | dépôt public ; une base ne doit pas suffire à tout ouvrir | `.env.example`, `settings.php` | en ligne |
| Fichiers sensibles refusés par Apache (`.env*`, `schema*.sql`, `router.php`, `cleanup.php`, `*.md`, `*.log` ; fichiers PHP internes `account|auth|console_page|db|enroll|env|mail|settings.php`, en ligne le 12 oct. 2026) ; pas de listing de répertoire | un fichier présent sur disque est servi tel quel sinon | `.htaccess` | en ligne |
| Jetons (modules, admin) comparés en temps constant (`hash_equals`) ; seules des empreintes SHA-256 des jetons de module et codes d'enrôlement sont stockées | éviter fuite par mesure de temps et vol de jetons par lecture de la base | `index.php`, `db.php`, `enroll.php` | en ligne |
| En-têtes : `X-Content-Type-Options`, `X-Frame-Options: DENY`, `Referrer-Policy: no-referrer`, HSTS | défense en profondeur (type MIME, cadrage, fuite d'adresse, rétrogradation HTTP) | `.htaccess` | en ligne |
| CSP `script-src 'self'` sans `'unsafe-inline'` pour `admin.html` et `app.html` (scripts dans `admin.js`, `app.js`) | un script injecté (XSS) ne s'exécute pas | `.htaccess`, P0.4b | en ligne (vérifié le 12 oct. 2026) |
| Éditeur sans script inline (`script-editeur.js`), puis CSP sans `'unsafe-inline'` sur **tout** le site | même raison, étendue à `/editeur/` | `data/`, `.htaccess`, P0.4d / P0.4e | en ligne (12 oct. 2026) |
| Mot de passe SMTP chiffré en base (XChaCha20-Poly1305, libsodium, clé `SETTINGS_KEY`, nom du paramètre en donnée associée) ; refus d'écrire un secret sans clé | une sauvegarde ou une fuite de la base seule ne livre pas la boîte mail | `settings.php`, P0.4c | en ligne (12 oct. 2026) |
| Console d'administration derrière une authentification HTTP Basic (`ADMIN_PAGE_USER`, empreinte `password_hash`), en plus du jeton admin ; 503 si non configurée | un inconnu ne charge même pas la console | `console_page.php`, `.htaccess`, P0.4e | en ligne (12 oct. 2026) |
| Site non indexé : `robots.txt` et `X-Robots-Tag: noindex, nofollow, noarchive` | ne pas exposer le service dans les moteurs de recherche (robots polis seulement : pas une protection) | `robots.txt`, `.htaccess`, P0.4e | en ligne (12 oct. 2026) |
| Sessions de l'espace utilisateur : case « Rester connecté » (7 jours fixes) ; sinon cookie de navigateur, 2 h d'inactivité, 12 h au plus ; plafond appliqué aussi aux anciennes sessions de 30 jours ; cookie `HttpOnly; Secure; SameSite=Strict`, jeton 256 bits stocké en empreinte ; durées réglables dans la console | une session oubliée ou un appareil partagé ne donne plus l'accès pendant 30 jours | `auth.php`, `app.html` | en ligne (12 oct. 2026) |
| Mails : CR/LF refusés, certificat SMTP vérifié, plafonds par adresse, IP et jour **avant** envoi, journal sans adresse en clair | injection d'en-têtes, abus qui ferait couper le site | `mail.php`, `settings.php` | en ligne |
| Liens des mails construits depuis `APP_BASE_URL` (https obligatoire), jamais depuis l'en-tête `Host` | empoisonnement du lien de réinitialisation | `account.php`, `settings.php` | en ligne |
| Publication depuis Git (`tools/deploy_cloud.py`, FTPS, relecture « ALIGNE », pages publiées après leurs scripts, `.htaccess` en dernier) | le serveur correspond à un commit ; pas de demi-publication | `tools/deploy_cloud.py` | outil prêt, identifiants FTP à fournir (P0.4a) |
| Contrôle après publication : en-têtes, absence de 500, `tools/web_security_probe.py` | une `.htaccess` fautive coupe tout le site | `tools/` | à chaque publication |

**Règles pour toute évolution du service en ligne ou des pages Web :**

- aucun `<script>` inline ni attribut `on*=` : script dans un fichier `.js` du même site, gestionnaires posés en JS (`el.onclick = …`) ;
- aucune ressource externe (CDN, police, image distante) : `default-src 'self'` ;
- aucun secret, jeton ou mot de passe dans le dépôt, dans une page, dans un journal ou dans un message de chat ; un nouveau secret va dans le `.env` du serveur, ou chiffré en base s'il doit se régler depuis la console ;
- tout nouveau paramètre de service se règle sans recompilation (registre `settings.php`) ;
- toute nouvelle page d'administration passe par la même protection que la console ;
- toute route qui écrit exige une authentification et borne la taille de ce qu'elle reçoit ;
- tester sur banc sous la CSP réelle (avec un témoin négatif) avant publication, puis vérifier en production juste après ;
- relire le §1.1 : la règle appliquée est-elle encore la bonne ?

## 5. Secrets et identités

Catégories minimales :

- identifiants Wi-Fi ;
- identifiants MQTT ;
- certificats et clés privées ;
- jetons API météo et notifications ;
- secrets VPS et CI ;
- jetons utilisateurs mobiles.

Pour chaque secret, documenter : propriétaire, emplacement, méthode de provisionnement, durée de vie, rotation, révocation, sauvegarde éventuelle et comportement après perte.

## 6. Journalisation de sécurité

Événements à prévoir :

- authentification réussie ou refusée ;
- commande distante acceptée ou refusée ;
- modification de configuration critique ;
- changement d'identité ou de certificat ;
- échec TLS, certificat expiré ou non valide ;
- firmware invalide, OTA échouée ou rollback ;
- démarrage en mode de secours ;
- répétition anormale de requêtes ;
- perte ou restauration d'un service de sécurité.

Les journaux ne doivent contenir ni mots de passe, ni clés privées, ni jetons complets.

## 7. Niveaux de déploiement

Les niveaux ci-dessous sont des profils de déploiement, pas des excuses pour désactiver les protections fondamentales.

- laboratoire : accès isolé, diagnostic facilité, aucune exposition Internet ;
- habitation locale : authentification locale, réseau protégé, mises à jour maîtrisées ;
- habitation connectée : MQTT TLS, identités individuelles, commandes distantes tracées ;
- installation professionnelle : supervision, sauvegarde, rotation et procédure d'incident ;
- multi-site : séparation des locataires, gestion centralisée des identités, révocation et audit.

## 8. Cycle de vie sécurité

Chaque évolution suit :

0. vérification que les règles de ce document sont toujours l'état de l'art (§1.1) ;
1. identification des actifs et surfaces d'attaque ;
2. analyse des menaces ;
3. définition des protections et du repli ;
4. implémentation minimale ;
5. tests positifs et négatifs ;
6. journalisation vérifiée ;
7. documentation et mise à jour du registre des risques ;
8. suivi des vulnérabilités après livraison ;
9. correction, rotation ou révocation en cas d'incident ;
10. plan de retrait ou fin de support.

## 9. Invariants

- Une perte de cloud ne doit pas désactiver les sécurités locales.
- Une commande non authentifiée ou expirée ne doit jamais activer un équipement.
- Une erreur de sécurité doit conduire à un état sûr et visible.
- Les secrets ne doivent jamais être intégrés aux ressources Web publiques ou à la carte SD.
- Une fonction de récupération ne doit pas devenir une porte dérobée permanente.
- Toute protection dépendant de l'heure doit définir son comportement lorsque NTP est indisponible.
- La sécurité ne repose jamais sur le secret du code source : le dépôt est public.
- Aucune règle de sécurité n'est figée : elle est réexaminée à chaque évolution, à chaque alerte et au moins chaque trimestre (§1.1).
- Toute nouvelle interface externe doit être ajoutée au registre des risques avant mise en service.