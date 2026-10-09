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
2. Transport par **SMTP authentifié** sur une boîte dédiée (`noreply@…`),
   identifiants dans le `.env` du serveur seulement (`MAIL_SMTP_HOST`,
   `MAIL_SMTP_PORT`, `MAIL_SMTP_USER`, `MAIL_SMTP_PASS`, `MAIL_FROM`) — jamais
   dans Git (dépôt public). Client SMTP minimal écrit dans le projet (TLS
   implicite, `AUTH LOGIN`), sans Composer, comme le reste du serveur. `mail()`
   n'est gardé que comme repli si l'essai montre qu'il passe mieux.
3. Limites côté application, **avant** l'envoi : par adresse destinataire et par
   IP (table `mail_log`), plafond global journalier ; dépassement = réponse
   identique, aucun envoi.
4. Premier lot : un essai d'envoi depuis l'administration (`POST /admin/mail-test`)
   et le relevé du score dans l'historique AlwaysData.

**Ouvert (M1)** — domaine d'expédition : la boîte peut-elle exister sur
`aqualook.alwaysdata.net` (sous-domaine par défaut), ou faut-il un domaine
propre (meilleure délivrabilité, DMARC maîtrisé) ? À vérifier dans le panneau
AlwaysData.

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
  **Ouvert (W1)** : liste définitive des lectures libres, à établir route par
  route dans `WebManager.cpp` (aucune lecture libre ne doit exposer un mot de
  passe Wi-Fi, une clé météo, un jeton CloudSync ou le secret `ApiAuth`).
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

## 9. Découpage en lots

Chaque lot : branche dédiée, commits petits, compilation `ProgrammeArrosage_s3`,
test sur `.141`, checkpoint.

| Lot | Contenu | Côté | Prérequis |
|---|---|---|---|
| A | `mail.php`, `mail_log`, `/admin/mail-test` ; essai réel, score Rspamd relevé | serveur | M1 tranché, boîte créée |
| B | Mot de passe oublié, changement de mot de passe, invitation | serveur + `app.html` | A |
| C | `hw_id` dans le rapport, liaison au premier usage, colonne unique | firmware + serveur | — |
| D | Enrôlement par code court, écran LCD « En ligne », désenrôlement, transfert | firmware + serveur | C (et E pour la protection PIN) |
| E | PIN LCD (`aqlsec`, pavé, temporisation, geste au splash) | firmware | — |
| F | Session Web locale, retrait du verrou visuel, outillage de banc | firmware + `data/` | E conseillé |
| G | Effacement du PIN depuis l'espace en ligne | firmware + serveur | B, D, E |

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
