# Enrôlement en ligne, comptes et sécurité locale du module (D016)

- Rédigé le 9 octobre 2026, base `main` = `11b3f46` (code `bc9e568`).
- Statut : **proposition validée dans ses orientations par le propriétaire le
  9 oct. 2026** ; aucun code écrit. Les points marqués « ouvert » restent à
  trancher avant le lot qui les concerne.
- Décision résumée : `docs/codex/02_DECISIONS.md`, D016.
- Origine : checkpoint `CHECKPOINT_2026-10-08_bc9e568.md`, §11 et §12.

## 1. Pourquoi une décision commune

L'espace en ligne (§11) et la sécurité locale (§12) se rejoignent en un point :
**rattacher un module à un compte est un acte de propriétaire**. Si l'interface
locale reste ouverte, n'importe quel poste du LAN peut faire afficher un code
d'enrôlement et rattacher le module à un autre compte. L'enrôlement s'appuie donc
sur une preuve de présence physique (le LCD), elle-même protégée par le PIN dès
qu'il existe.

## 2. Existant (vérifié dans le code le 9 oct. 2026)

| Domaine | État |
|---|---|
| Comptes | Créés par l'administrateur seulement (`POST /admin/user`). Tables `app_user`, `app_session`, `login_attempt` (`schema-v2-comptes.sql`). Mots de passe en `password_hash`, sessions en base, empreinte SHA-256 du jeton de cookie. |
| Rattachement | `POST /admin/module/owner`, colonne `module.owner_user_id`. |
| Identité du module | `module_id` libre (ex. `Jardin-01`), saisi dans le module (`CloudSync`, NVS `KEY_MODULE_ID`) ; pas d'identifiant matériel côté serveur. |
| Jeton du module | Émis par `POST /admin/module-token`, recopié à la main ; table `module_token` (empreinte SHA-256, unique). |
| Mails | Aucun. |
| LCD | Écran `ADMIN` libre ; démarrage et arrêt manuel des zones libres. |
| Web local | Verrou **visuel** (`data/index.html` : `sessionStorage` `aqualook-admin-unlocked`, mot de passe `1598753` en clair dans la page). Routes `/api/*` ouvertes au LAN. |
| `ApiAuth` | Secret partagé posé depuis le navigateur (confiance au premier usage), HMAC sur une forme canonique, nonce persisté ; protège scripts, câblage, variables, redémarrage. Oubli du secret : uniquement par deux appuis sur ADMIN > Système (`DisplayManager::handleTouchAdmin`). |

## 3. Envoi de mails depuis AlwaysData (analyse de la documentation)

Sources : [Using an e-mail address](https://help.alwaysdata.com/en/e-mails/use-an-e-mail-address),
[SPF/DKIM/DMARC](https://help.alwaysdata.com/en/docs/e-mails/outgoing-e-mails/set-up-spf-dkim-dmarc/),
[Improving delivery](https://help.alwaysdata.com/en/docs/e-mails/outgoing-e-mails/delivery/),
[Check e-mail sending](https://help.alwaysdata.com/en/docs/e-mails/outgoing-e-mails/check-email-sending/),
[Reacting to spam](https://help.alwaysdata.com/en/docs/e-mails/outgoing-e-mails/react-to-spam-mailing/).

Constats :

- SMTP sortant : `smtp-<compte>.alwaysdata.net`, port 465 (TLS) ou 587
  (STARTTLS), **authentification par adresse et mot de passe d'une boîte**.
- La documentation se contredit sur l'envoi depuis un site hébergé : une page dit
  que l'authentification n'est pas requise depuis les services hébergés, une autre
  qu'elle est « nécessaire » depuis une application HTTP. → **à vérifier par un
  essai réel.**
- Anti-spam Rspamd sur l'offre mutualisée : **tout message de score > 3 est
  bloqué**. Recommandations : `MAIL FROM` (enveloppe) identique à l'en-tête
  `From`, pas de HTML chargé, pas de majuscules ou de ponctuation excessives.
- SPF : enregistrement créé par défaut (`include:_spf.alwaysdata.com ~all`) pour
  les domaines servis par les DNS AlwaysData. DKIM : paire de clés générée
  automatiquement, clé publique en TXT. DMARC : enregistrement proposé dans
  l'onglet DNS, à activer une fois SPF et DKIM en place.
- Suivi : panneau **E-mails > Historique** (7 jours par défaut), avec score,
  statut « bloqué » et rapport Rspamd.
- Aucune limite de débit chiffrée n'est publiée. En cas d'abus, AlwaysData coupe
  les `POST` du site, change le mot de passe de la boîte, et peut suspendre le
  compte en cas de récidive. **Un formulaire d'envoi de mail mal limité met donc
  tout le service en danger, pas seulement les mails.**

Décisions :

1. Une seule fonction serveur `send_mail(to, subject, text)` dans un nouveau
   `mail.php`, **texte brut**, en-têtes minimaux, `From` = enveloppe.
2. Transport par **SMTP authentifié** sur une boîte dédiée. Paramètres
   (`MAIL_SMTP_HOST`, `MAIL_SMTP_PORT`, `MAIL_SMTP_USER`, `MAIL_SMTP_PASS`,
   `MAIL_FROM`, `MAIL_FROM_NAME`, plafonds) réglés depuis la console
   d'administration et rangés en base (`app_setting`), `.env` en repli —
   jamais dans Git (dépôt public), jamais écrits dans une page. Client SMTP minimal écrit dans le projet (TLS
   implicite, `AUTH LOGIN`), sans Composer, comme le reste du serveur. `mail()`
   n'est gardé que comme repli si l'essai montre qu'il passe mieux.
3. Limites côté application, **avant** l'envoi : par adresse destinataire et par
   IP (table `mail_log`), plafond global journalier ; dépassement = réponse
   identique, aucun envoi.
4. Premier lot : un essai d'envoi depuis l'administration (`POST /admin/mail-test`)
   et le relevé du score dans l'historique AlwaysData.

**Tranché (M1), 9 oct. 2026** : expéditeur `aqualook@alwaysdata.net`, boîte
créée par le propriétaire ; SMTP `smtp-aqualook.alwaysdata.net` (465 TLS).
Le domaine `alwaysdata.net` appartient à l'hébergeur : SPF, DKIM et DMARC sont
les siens, nous ne les réglons pas. Un domaine propre reste une évolution
possible si la délivrabilité le demande.

## 4. Identité et unicité des modules

- **Identifiant matériel** `hw_id` = `aql-` + MAC eFuse de base en hexadécimal
  (12 caractères). Stable, lisible au LCD, ne change pas avec un reflash ni un
  effacement de la NVS.
- Serveur : nouvelle colonne `module.hw_id VARCHAR(20) NULL UNIQUE`.
  `module_id` reste la clé primaire (aucune table existante ne change de clé) ;
  les nouveaux modules reçoivent `module_id = hw_id`, le nom affiché va dans un
  libellé modifiable par l'utilisateur.
- Modules existants (`Jardin-01`) : le firmware ajoute `hw_id` à son rapport ;
  le serveur l'enregistre **au premier rapport authentifié par jeton** si la
  colonne est vide (liaison au premier usage), puis refuse tout autre `hw_id`
  pour ce `module_id` (journalisé côté administration).
- Un module appartient à **au plus un compte** (`owner_user_id` unique par
  construction).

## 5. Enrôlement par code court (modèle RFC 8628)

```text
LCD (ADMIN > En ligne > Rattacher, PIN si posé)
  module ── POST /v1/enroll/start {hw_id, firmware} ──► serveur
         ◄── {device_code, user_code "K7QM-4TZP", expire 600 s, interval 5 s}
  LCD affiche user_code + hw_id + compte à rebours
utilisateur (session /app) ── POST /app/module/claim {user_code} ──► serveur
  serveur : vérifie code, rattache, émet le jeton du module
  module ── POST /v1/enroll/poll {device_code} (toutes les 5 s) ──► serveur
         ◄── {pending} … puis {module_id, token} (une seule fois)
  module : enregistre module_id + jeton en NVS, LCD « rattaché à <compte masqué> »
```

Règles :

- `user_code` : 8 caractères dans un alphabet sans ambiguïté
  (`ABCDEFGHJKLMNPQRSTUVWXYZ23456789`, ni O/0 ni I/1), affiché en deux groupes ;
  validité 10 minutes ; usage unique ; `device_code` : 256 bits. **Seules les
  empreintes SHA-256 sont stockées** (table `enroll_request`).
- Le code n'est produit qu'à la demande d'un geste sur le LCD : **aucune route
  Web locale ne déclenche l'enrôlement**. Le PIN le protège dès qu'il existe.
- `/v1/enroll/start` est public : limite par IP et par `hw_id` (ex. 5 demandes
  par heure), une seule demande vivante par `hw_id`.
- `/app/module/claim` : limite d'essais par compte et par IP (même principe que
  `login_attempt`) ; message identique pour un code faux ou expiré.
- Le jeton n'est rendu qu'une fois, au premier `poll` qui suit l'approbation ;
  la demande est alors consommée.
- **Preuve de possession (ajoutée au lot D, 10 oct. 2026)** : `/v1/enroll/start`
  étant public et le `hw_id` lisible sur le LAN (`/api/diagnostics`), un tiers
  pourrait sans cela obtenir un code pour le boîtier d'un autre et le
  « transférer ». Si le `hw_id` appartient à un module rattaché ou muni d'un
  jeton, la demande doit présenter ce jeton (`Authorization: Bearer`, HTTPS
  seulement) ; sinon `403 already_enrolled`. Un module vendu garde son jeton :
  le transfert reste possible. Un module « oublié » sur le LCD n'a plus de
  jeton : le détacher d'abord en ligne. Pour un `hw_id` libre, le pire
  possible est de bloquer un enrôlement, jamais d'obtenir le boîtier.
- Le jeton n'est remplacé qu'au `poll` qui suit l'approbation (pas à la
  saisie) : un module approuvé mais coupé avant son `poll` garde un jeton
  valable et peut recommencer.
- Le chemin manuel (jeton recopié depuis `/admin/module-token`) reste disponible
  pendant toute la transition et pour le mode maintenance.

### Transfert et suppression

- **Désenrôlement par l'utilisateur** (`POST /app/module/delete`) : révoque le
  jeton, efface `owner_user_id`. Choix explicite dans l'interface : conserver ou
  purger l'historique (`module_message`), la configuration et les sauvegardes.
  Le module reçoit 401 au cycle suivant : LCD « non rattaché », synchronisation
  suspendue (recul long), rien d'autre ne change localement.
- **Oublier le compte depuis le LCD** (ADMIN > En ligne, PIN, double
  confirmation) : efface jeton et `module_id` de la NVS. Le serveur n'est pas
  prévenu ; le module apparaît hors ligne jusqu'au désenrôlement ou à un nouvel
  enrôlement.
- **Transfert** (module vendu ou donné) : un enrôlement réussi pour un `hw_id`
  déjà rattaché à un autre compte vaut transfert, puisque le code n'a pu être lu
  que sur l'écran. L'ancien jeton est révoqué, l'ancien propriétaire est prévenu
  par mail, et **les données de l'ancien propriétaire sont purgées** (elles ne
  passent jamais au nouveau).

## 6. Comptes

- **Inscription** : sur invitation au départ (l'administrateur crée le compte ou
  envoie un lien d'invitation à usage unique) ; inscription libre avec
  vérification de l'adresse **seulement après validation des mails en production**
  (lot ultérieur, décision à confirmer à ce moment-là).
- **Mot de passe oublié** : table `password_reset` (empreinte SHA-256 du jeton,
  expiration 30 minutes, usage unique) ; réponse **identique** que l'adresse
  existe ou non ; limite par adresse et par IP ; après réinitialisation, toutes
  les sessions du compte sont fermées et un mail de notification est envoyé.
- Changement de mot de passe connecté : exige le mot de passe actuel, ferme les
  autres sessions.
- Les autres services proposés (alertes par mail, partage, TOTP, journal
  d'audit, export RGPD…) restent hors de D016 et seront arbitrés lot par lot.

## 7. PIN sur le LCD

- **4 à 6 chiffres**, pavé tactile ; dérivé en NVS par PBKDF2-HMAC-SHA256
  (mbedTLS, sel aléatoire de 16 octets, nombre d'itérations borné pour rester
  sous ~300 ms sur l'ESP32-S3) ; **jamais en clair**.
- Persistance dans un **espace NVS dédié `aqlsec`**, bloc versionné
  `{magic, version, …, crc32}`, hors du bloc `ALOK` (pas de hausse de
  `configRevision`). Bloc illisible = pas de PIN + message `[SEC]`, jamais
  d'échec de démarrage.
- **Périmètre protégé** : écran ADMIN (configuration) et **démarrage manuel**
  d'une zone. **L'arrêt d'une zone en cours reste toujours libre** (sécurité
  relais, D012). Consultation (état, planning, météo, À propos) libre.
- Déverrouillage valable 5 minutes ou jusqu'à la mise en veille de l'écran.
- Échecs : 5 essais, puis temporisation croissante (30 s, doublée, plafond
  15 min). Compteur **persisté** pour qu'un redémarrage ne remette pas à zéro.
- **Transition** : pas de PIN posé = comportement actuel, avec un rappel visible
  dans ADMIN. La mise à jour ne bloque jamais un module installé.
- **PIN oublié** :
  1. geste physique au démarrage (appui maintenu ~10 s sur l'écran pendant le
     splash, avec décompte affiché) : efface le PIN, journalise `[SEC]`. Aucun
     relais n'est touché, aucun délai ajouté au démarrage normal ;
  2. pour un module enrôlé, depuis l'espace en ligne : commande d'effacement du
     PIN **seulement** (ni pose, ni changement), mail au propriétaire, journal
     côté module. Lot ultérieur, après le lot PIN.
- Réinitialisation de la configuration (`/api/resetConfig`) **n'efface pas** le
  PIN.

## 8. Accès Web local sécurisé

- **Le secret `ApiAuth` devient le mot de passe d'accès Web** : même confiance au
  premier usage, même oubli par le LCD, une seule chose à retenir. Longueur
  minimale 10 caractères (le PIN, trop court, ne sert pas sur le réseau : un
  échange capturé se force hors ligne).
- **Ouverture de session par défi-réponse** : `GET /api/session/challenge` rend
  un nonce ; le navigateur répond `HMAC(secret, nonce)` ; le module émet un
  jeton de session de 128 bits en cookie `HttpOnly; SameSite=Strict`. Le mot de
  passe ne circule jamais.
- Sessions en RAM (au plus 4), expiration après 30 minutes d'inactivité, perdues
  au redémarrage (accepté). Échecs limités comme le PIN.
- **Toutes les routes d'écriture exigent la session.** Les écritures déjà signées
  (scripts, câblage, variables, redémarrage) **gardent leur signature HMAC**
  pendant la transition ; leur simplification sera une décision séparée.
- Lecture libre conservée pour le diagnostic : `/api/status`,
  `/api/diagnostics`, `/api/logs.txt`, `/api/script-globals`, ressources Web.
  **W1 tranché le 10 oct. 2026** : liste définitive au §8.1 (aucune lecture
  libre n'expose un mot de passe Wi-Fi, une clé météo, un jeton CloudSync ou
  le secret `ApiAuth`).
- Retirer `1598753` et le verrou visuel de `data/index.html` : les sections
  administrateur s'affichent selon la session réelle.
- **Transition** : pas de secret posé = comportement actuel + bandeau « accès non
  protégé » ; dès que le secret existe, les écritures exigent la session.
- HTTP reste en clair sur le LAN : un cookie peut être capté par un poste qui
  écoute le réseau. Risque assumé et documenté (même limite qu'`ApiAuth`) ;
  HTTPS local jugé trop lourd pour l'ESP32 à ce stade.
- **Outillage de banc** (`tools/`, `/api/debug/*`, `announce_reboot.py`) :
  ouverture de session par les outils avec le secret lu dans le `.env` local
  (jamais dans Git). Pas de porte dérobée compilée dans le firmware de
  production.

### 8.1 Classement des routes (W1, lot F)

Relevé route par route le 10 oct. 2026 sur `main` = `37b5921`
(`WebManager.cpp`, `WebManager.h`, `SdStaticHandler.cpp`).

**Application** : un seul filtre placé en tête du routage, juste après la
garde de longueur d'URL. ESPAsyncWebServer choisit le gestionnaire dès la fin
des en-têtes (`_attachHandler`) : une requête refusée par le filtre ne voit
donc jamais son corps traité (dépôt SD, script, câblage). Le filtre ne fait
rien tant qu'aucun secret n'est posé (transition, §8).

Règle : **toute méthode autre que `GET`/`HEAD` exige la session**, sauf les
exceptions listées ici. Un `GET` est libre sauf s'il figure dans « lectures
sous session ». Une route ajoutée plus tard est donc protégée par défaut si
elle écrit.

| Classe | Routes |
|---|---|
| Lectures libres — pages | ressources Web (SD, LittleFS), `/`, `/setup`, `/logs`, `/ota`, redirections du portail captif (`/generate_204`…) |
| Lectures libres — état | `/api/status`, `/api/zonesConfig`, `/api/zone`, `/api/forecast`, `/api/display`, `/api/io`, `/api/adminStatus` (secrets masqués), `/api/storage` |
| Lectures libres — diagnostic | `/api/diagnostics`, `/api/health`, `/api/faults`, `/api/logs.txt`, `/api/logs`, `/api/logConfig`, `/api/incidents/storage-sd`, `/api/maintenance/last-result`, `/api/debug/heap-info`, `/api/debug/nvs-stats`, `/api/debug/script-selftest`, `/api/debug/verify-web-asset/status`, `/api/relay/topology`, `/api/relay/input` |
| Lectures libres — scripts | `/api/scripts`, `/api/script-one`, `/api/script-source`, `/api/script-messages`, `/api/script-globals`, `/api/log-messages` |
| Lectures libres — session | `/api/auth/state`, `/api/session/*` (défi, état) |
| Lectures sous session | `/api/notifications` (rend le sujet ntfy, qui vaut un secret sur `ntfy.sh`), `/api/wifi/scan` (hors portail captif), `/api/debug/script-dryrun` (sa variante `action=` suspend ou reprend une zone) |
| Écritures sous session | tous les `POST` : planning, zones, **démarrage et arrêt manuels** (`/api/manual`), réglages, notifications, câblage, maintenance et OTA, dépôt SD (`/api/debug/*`), acquittements, redémarrage, `resetConfig`, `cloudSyncNow`, `bootguard/clear` |
| Écritures sous session **et** signées | `/api/script-save`, `/api/script-erase`, `/api/script-run`, `/api/script-messages`, `/api/script-globals` (seules routes qui appellent réellement `ApiAuth::verify`) |
| Exceptions | `POST /api/session/*` (ouverture, fermeture) ; `POST /api/auth-secret` tant qu'**aucun** secret n'existe (premier secret, confiance au premier usage) ; **portail captif actif** : `/api/wifi/scan`, `POST /api/wifi`, `POST /api/captive` libres (décision du propriétaire, 10 oct. 2026 : il faut être à portée radio du point d'accès) |

Décisions du propriétaire (10 oct. 2026) :

- l'**arrêt** d'une zone depuis le Web exige la session, comme le démarrage
  (l'arrêt reste libre sur l'écran, D012) ;
- la configuration Wi-Fi reste libre pendant le portail captif seulement.

Écarts constatés avec le §2, corrigés ici : le câblage et le redémarrage ne
sont **pas** signés aujourd'hui (seuls les scripts, phrases et variables le
sont) ; le firmware exige un secret d'au moins **12** caractères (et non 10),
valeur conservée.

## 9. Découpage en lots

Chaque lot : branche dédiée, commits petits, compilation `ProgrammeArrosage_s3`,
test sur `.141`, checkpoint.

| Lot | Contenu | Côté | Prérequis |
|---|---|---|---|
| A | `mail.php`, `mail_log`, `/admin/mail-test` ; essai réel, score Rspamd relevé | serveur | boîte créée (fait), mot de passe dans le `.env` du serveur |
| B | Mot de passe oublié, changement de mot de passe, invitation | serveur + `app.html` | A |
| C | `hw_id` dans le rapport, liaison au premier usage, colonne unique | firmware + serveur | — |
| D | Enrôlement par code court, écran LCD « En ligne », désenrôlement, transfert | firmware + serveur | C (et E pour la protection PIN) |
| E | PIN LCD (`aqlsec`, pavé, temporisation, geste au splash) | firmware | — |
| F | Session Web locale, retrait du verrou visuel, outillage de banc | firmware + `data/` | E conseillé |
| G | Effacement du PIN depuis l'espace en ligne | firmware + serveur | B, D, E |

**Principe ajouté le 9 oct. 2026 (propriétaire)** : tout paramètre du service
(boîte mail, serveurs, plafonds…) se règle sans recompilation ni redéploiement.
Côté serveur, registre `settings.php` + table `app_setting`, édité depuis la
console (`.env` en repli ; seuls `DB_*` et `ADMIN_TOKEN` y restent). Côté
module, les adresses de serveur restent en NVS (c'est déjà le cas de l'hôte
CloudSync) ; aucun lot ne doit en écrire une en dur dans le firmware ou une page.

Lot D : **validé sur `.141` le 10 oct. 2026** (branche
`feature/enrolement-code-court`, firmware build 1326). Serveur : `enroll.php`,
`schema-v6-enrolement.sql` (`enroll_request`, `enroll_claim_attempt`,
`module.released_owner_user_id`), routes `/v1/enroll/start|poll`,
`/app/module/claim|release|label`, paramètres `ENROLL_*` dans la console ;
banc local 58/58. Firmware : page ADMIN « En ligne » (10/10), tâche
`cloud-enroll`, envoi immédiat de la configuration après rattachement. Testé
par le propriétaire en production : refus « déjà rattaché » après « Oublier le
compte », détachement + purge en ligne, rattachement par code, configuration
visible. Constaté pendant le test : un `settings.php` non republié rendait les
plafonds à 0 (« trop de demandes ») -- désormais `503 not_configured`, et le
429 annonce l'heure du prochain essai. Ajouts demandés : bouton
« Synchroniser maintenant » (page locale Synchro cloud, 60 s minimum entre
deux tentatives), libellé « Identifiant technique » (le nom est libre,
l'identifiant fixe). Envoi immédiat au rattachement validé le 10 oct. 2026
(build 1328) : module détaché en ligne puis rattaché par code, « rattache » à
09:08:53, synchro lancée dans la même seconde, cycle `rapport=ok config=ok` à
09:09:12. Les flashs ne font plus monter la garde anti-boucle (empreinte de
l'image, `d07fbb0`).

Lot B : **validé en production le 10 oct. 2026** (banc local 44/44 ;
branche `feature/lot-b-comptes` fusionnée). Essai du propriétaire : mot de
passe oublié, mail reçu, lien suivi, nouveau mot de passe posé, connexion. Serveur : `account.php`,
`schema-v7-liens-compte.sql` (table `account_token`, une seule table pour les
liens de réinitialisation et d'invitation au lieu de `password_reset` : même
geste, seules durée et texte changent), routes `/app/password/forgot|reset|change`
et `/admin/user/invite`, paramètres `APP_BASE_URL`, `RESET_TOKEN_TTL_MIN`,
`INVITE_TOKEN_TTL_H`. Jeton dans le fragment de l'URL ; réponse de
`forgot` envoyée avant le SMTP (durées mesurées au banc : 26 ms connue,
21 ms inconnue) ; notifications exemptées des plafonds par adresse et par IP.
`app.html` : « Mot de passe oublié ? », écran « choisir un mot de passe »
ouvert par le lien, carte « Mon compte ». `admin.html` : « Inviter par mail »,
lien affiché aussi à l'administrateur. Production : fichiers publiés,
`schema-v7` importé, `APP_BASE_URL` saisi ; `forgot` sur adresse inconnue
≈ 150 ms. Non essayés en production (banc seulement) : invitation,
changement de mot de passe connecté, durée de `forgot` sur adresse connue.

Lot E : **validé sur `.141` le 9 oct. 2026** (firmware `370a6eb`, build
1317). `PinLock` (NVS `aqlsec`, PBKDF2, essais limités et persistés), portes
uniques `requestAdmin()` / `requestStart()`, arrêt toujours libre, page ADMIN
« Sécurité », pavé paysage à touches rondes, cadenas dans le bandeau (S3),
geste d'effacement au démarrage. Écarts voulus par le propriétaire pendant le
test : après le PIN, **rien n'est lancé** (la zone touchée n'est pas
mémorisée, l'écran revient déverrouillé) ; garde de toucher (doigt relevé
puis 600 ms) après OK, Annuler et le geste. Non confirmés explicitement : le geste
d'effacement au démarrage, le décompte au-delà de la 1re attente, la CYD.
Le cadenas n'a pas encore été vu à l'écran par le propriétaire.

Lot C : **validé de bout en bout le 9 oct. 2026**. Firmware `d2891f9`
(build 1307) flashé sur `.141` (COM4) : `build.hwId` = `aql-44bd8d7acb88`
(= `wifi.mac`), ligne de démarrage du journal avec `hw=…`, cycle CloudSync
`rapport=ok` avant comme après le déploiement serveur ; `schema-v5` importé,
console : « Identifiant matériel aql-44bd8d7acb88 », sans conflit. Banc
serveur local 8/8. Constats antérieurs au lot, non corrigés : la bannière
série de démarrage est perdue sur le S3 (imprimée avant la réouverture du
port USB natif), et la ligne de démarrage affiche `target=unsupported` pour
le S3.

Lot A : **validé en production le 9 oct. 2026** (banc local 27/27 ; fichiers
publiés, `schema-v4` importé, réglages saisis dans la console, `/health`
`mail: true`, essai reçu). **Délivrabilité à reprendre** : le mail d'essai est
arrivé dans les indésirables de Gmail, alors que SPF et DKIM passent et sont
alignés (« envoyé par / signé par alwaysdata.net »). Facteurs restants :
réputation partagée du domaine `alwaysdata.net`, premier envoi court sans
historique. Piste retenue, décision reportée par le propriétaire : domaine
d'expédition propre (SPF/DKIM/DMARC à notre nom), à changer depuis la console
seule. **Clos le 10 oct. 2026** : le propriétaire a corrigé le classement en
indésirables, ce point ne conditionne plus le lot B.

Ordre proposé : A → C → E → D → B → F → G (les lots serveur A/B peuvent avancer
pendant les essais firmware). Version proposée à la fin de E + F : `5.12.0`.

## 10. Invariants

- La mise à jour reste déclenchée par l'utilisateur sur le module ; rien ne se
  lance à distance (D014) ; l'effacement du PIN à distance (G) n'ouvre aucune
  vanne et ne pose aucun secret.
- Durée maximale de sécurité et arrêt d'urgence d'une zone intangibles.
- Format NVS `ALOK` inchangé ; nouvelles données dans `aqlsec`, versionnées.
- Routes et IDs existants conservés (F5, F6) ; les nouvelles routes s'ajoutent.
- Aucun secret dans le dépôt public (SMTP, jetons, mots de passe de banc).
- Toute nouvelle étape de démarrage (lecture `aqlsec`, geste au splash) intégrée
  à la progression de boot.
