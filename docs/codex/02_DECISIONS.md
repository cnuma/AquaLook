# 02 — Décisions d’architecture

## D001 — PlatformIO comme environnement de référence

`platformio.ini` est la source des dépendances, broches TFT et environnements. Toute dépendance doit être versionnée dans `lib_deps`.

## D002 — Architecture par managers

`main.cpp` assemble les modules. Les responsabilités métier restent dans les managers appropriés.

## D003 — Persistance NVS

La configuration active est stockée en NVS. Toute évolution de structure exige schéma, compatibilité et stratégie de migration.

## D004 — LittleFS en lecture pour les ressources

LittleFS sert le Web et le splash. ConfigManager est l’unique propriétaire du montage. Aucun fichier de travail dans `data/`.

## D005 — Activation relais par callback

ScheduleManager reste indépendant du matériel. Le callback est câblé dans `main.cpp`.

## D006 — EventBus minimal

Les communications transversales utilisent des flags statiques. Ne pas créer de second bus global.

## D007 — Interface Web statique embarquée

Chaque octet compte. Les gros frameworks frontend sont exclus et `buildfs` est obligatoire après changement.

## D008 — Limite fonctionnelle à 8 zones

La capacité interne reste à 16 pour compatibilité, mais l’interface et les sorties actives sont limitées à 8.

## D009 — Deux modes de planning

Modes supportés : jours fixes et intervalle. Toute nouvelle répétition exige une décision d’architecture.

## D010 — Verrouillage administrateur visuel

Le verrouillage actuel est côté navigateur. Il ne protège pas les secrets et ne constitue pas une authentification forte.

## D011 — Configuration LCD hot-reload

Les paramètres d’affichage prennent effet sans reboot via `EventBus::displayDirty`.

## D012 — Sécurité avant ergonomie

La logique relais, la durée maximale et la réponse HTTP avant reboot sont prioritaires sur les simplifications visuelles.

## D013 — Traçabilité du firmware par version Git

Chaque firmware AquaLook doit embarquer une identité de build issue de Git ou générée au moment de la compilation. Une source compile-time unique fournit au minimum le SHA Git court et la date/heure de compilation ; elle expose également la branche ou l’origine de build lorsqu’elle peut être déterminée de manière fiable.

Cette identité est réutilisée sans duplication manuelle par :

- la page Système du LCD ;
- la vue Web « À propos » ;
- les diagnostics système et les journaux de démarrage ;
- les futurs exports de diagnostic.

La vue Web « À propos » doit être facilement accessible depuis l’interface principale. Elle fait partie des ressources complètes servies prioritairement depuis la SD, mais une information de version minimale doit rester consultable lorsque l’interface de secours LittleFS est utilisée.

## D014 — Scripts et phrases modifiables depuis l’espace en ligne (6 oct. 2026)

Décision du propriétaire. Le périmètre de `config.apply` (`docs/architecture/CLOUD_REMOTE_CONFIG.md`) s’élargit aux scripts : emplacements, nom, actif, déclencheur, cible, contenu, effacement. Il s’élargit aussi à la bibliothèque de phrases (`/scripts/messages.tsv`).

- **Même méthode que les créneaux d’arrosage** : le serveur propose, le module arbitre, le local gagne (`baseRevision`). Aucun nouveau type de commande n’est créé.
- **Révision unique** : un enregistrement local de script, un effacement ou une sauvegarde de phrases incrémente désormais `configRevision`, comme tout autre réglage. Une commande bâtie avant est refusée, scripts et planning confondus.
- **Pas de signature de bout en bout.** Sur ce chemin, la confiance repose sur le jeton du module et sur TLS, et non sur `ApiAuth` (HMAC) qui protège les routes locales. Risque accepté en connaissance de cause : un serveur compromis pourrait installer un script qui ouvre une vanne au prochain déclencheur local. Ce risque est borné par la durée maximale de sécurité, qui reste intangible.
- **Rien ne se lance à distance** : pas de « Lancer maintenant », pas de pose ni de changement du secret HMAC. Un script modifié à distance ne part que sur un déclencheur local.
- **Le module ne fait pas confiance au bytecode reçu** : il le revalide (`validateScriptProgram`) avec les mêmes règles que `/api/script-save`. Un script en cours d’exécution n’est pas modifiable à distance.
- **Une commande = un emplacement ou le catalogue de phrases.** La réponse de `/v1/pending-command` peut atteindre 16 Ko (au lieu de 4 Ko).

## D015 — Variables globales des scripts et bloc « En parallèle » en Et / Ou (8 oct. 2026)

Décision du propriétaire, choix arbitrés le 8 octobre 2026.

- **Seize variables globales `g1`..`g16`**, entiers signés 32 bits, lues et écrites par tous les scripts (opcodes `GLOAD` 5 / `GSTORE` 6). Les variables locales `a`..`h` restent inchangées.
- **Persistance en NVS, espace `aqlvars`, à part de la configuration** : une variable qui change ne fait pas monter `configRevision` et ne touche pas le bloc `ALOK`. Blocs versionnés `{magic, version, nombre, …, crc32}` ; un bloc illisible est ignoré (valeurs à 0, noms vides) avec un message `[GVAR]`, jamais d’échec de démarrage.
- **Usure de la flash maîtrisée** : les valeurs sont écrites au plus une fois par minute, et tout de suite avant chaque redémarrage voulu (`BootLoopGuard::restartDeliberately`). Une coupure de courant peut perdre la dernière minute ; c’est accepté.
- **Noms libres** (23 octets au plus, sans `#` ni `|`), rangés en NVS avec les valeurs, pas sur la SD : les variables restent utilisables sans carte. Le bytecode ne transporte qu’un numéro ; le nom n’est qu’un libellé.
- **Écriture locale signée** (`POST /api/script-globals`, HMAC `ApiAuth`) ; lecture libre (`GET`). Une valeur n’est fixée depuis l’éditeur que si l’utilisateur l’a saisie, pour ne pas écraser ce qu’un script vient d’écrire.
- **Remontée vers l’espace en ligne (ajout du 8 oct. 2026, soir), en lecture seule** :
  - les noms et valeurs vont dans le miroir (`scripts.variables`, mêmes champs que `GET /api/script-globals`), absent en mode maintenance ; enregistrer les noms sur le module fait renvoyer le miroir ;
  - une valeur changée par un script ne renvoie pas le miroir : le rapport `diag` de chaque cycle porte `variables: {crc, valeurs[16]}` **seulement** quand le CRC32 des seize valeurs diffère du dernier envoi confirmé (toujours au premier cycle après démarrage). Renvoyer tout le miroir à chaque changement aurait fait tourner les 20 sauvegardes de configuration et grossi l’historique ;
  - le serveur (`merge_module_variables`) reporte ces valeurs dans le miroir sans toucher à la révision, à `updated_at` ni aux sauvegardes, et pose `scripts.variablesSuivies` ; l’éditeur en ligne date alors les valeurs du dernier contact du module, sinon de la date du miroir ;
  - l’onglet Variables en ligne est en lecture seule (pas de « Nouvelle valeur » ni d’envoi) et reste masqué pour un firmware qui ne remonte pas les variables.
- **Hors périmètre** : modification des noms ou valeurs depuis l’espace en ligne.
- **« En parallèle » en Et / en Ou** : « Et » (défaut, octet pour octet comme avant) attend les deux branches ; « Ou » reprend dès que l’une est finie, l’autre continue et le script ne se termine qu’avec elle (`JOINANY` 87, `JOIN` avant le `HALT` final). Un firmware antérieur refuse ces opcodes à l’enregistrement.
## D017 — Sécurité du service en ligne et règles de sécurité vivantes (12 oct. 2026)

Décision du propriétaire, 12 octobre 2026.

- **Le code reste open source** : le dépôt n'est pas rendu privé et le code n'est pas obfusqué. La sécurité repose sur les secrets et contrôles côté serveur, jamais sur le secret du code.
- **Service en ligne** : console d'administration derrière une authentification HTTP Basic (en PHP, identifiants dans le `.env` du serveur) en plus du jeton admin ; site non indexé (`robots.txt`, `X-Robots-Tag`) ; CSP sans `'unsafe-inline'` sur tout le site ; mot de passe SMTP chiffré en base. Détail et état : `docs/security/CYBERSECURITY_ARCHITECTURE.md` §4.8.
- **Route `GET /admin` retirée** (12 oct. 2026) : elle redirigeait vers la console et en révélait l'adresse ; `/admin` répond 404. La racine `/` sert une page d'accueil neutre (`accueil.html`) qui ne mène qu'à l'espace utilisateur.
- **Sessions de l'espace utilisateur** (12 oct. 2026) : fin des sessions de 30 jours. Case « Rester connecté » : 7 jours fixes ; sans la case : session de navigateur, 2 h d'inactivité, 12 h au plus. Durées réglables dans la console (`SESSION_REMEMBER_DAYS`, `SESSION_IDLE_MIN`, `SESSION_MAX_H`).
- **Règles vivantes** : aucune règle de sécurité n'est figée. Elles sont remises en cause à chaque évolution, à chaque faille publiée concernant une technologie du projet et au moins chaque trimestre, au regard des bonnes pratiques du moment, et évoluent avec les nouvelles découvertes (`CYBERSECURITY_ARCHITECTURE.md` §1.1).

## D016 — Enrôlement en ligne, comptes et sécurité locale du module (9 oct. 2026)

Décision du propriétaire (orientations validées le 9 octobre 2026). Conception détaillée, analyse des mails AlwaysData et découpage en lots : `docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md`.

- **Rattacher un module à un compte est un acte de propriétaire** : il exige une preuve de présence physique (code lu sur le LCD), protégée par le PIN dès qu’il existe. Aucune route Web locale ne déclenche l’enrôlement.
- **Identité matérielle** `hw_id` = `aql-` + MAC eFuse, colonne unique côté serveur ; `module_id` reste la clé ; liaison au premier rapport pour les modules existants. Un module appartient à au plus un compte.
- **Enrôlement par code court** (modèle RFC 8628) : `user_code` de 8 caractères sans ambiguïté, 10 minutes, usage unique ; le module reçoit son jeton par `poll`, plus de copie manuelle (le chemin manuel reste pendant la transition). Seules des empreintes SHA-256 sont stockées. Un enrôlement sur un `hw_id` déjà rattaché vaut transfert : ancien jeton révoqué, ancien propriétaire prévenu, ses données purgées. **Complément du 10 oct. 2026 (lot D)** : le `hw_id` étant lisible sur le LAN, la demande de code pour un module rattaché ou muni d'un jeton doit présenter ce jeton (`403 already_enrolled` sinon) ; un module « oublié » sur le LCD se détache donc d'abord en ligne. Un module détaché en gardant ses données puis rattaché par un autre compte est traité comme un transfert (`module.released_owner_user_id`).
- **Comptes** : inscription sur invitation d’abord, libre avec vérification de l’adresse une fois les mails validés en production ; mot de passe oublié par jeton à usage unique (30 minutes), réponse identique que l’adresse existe ou non, toutes les sessions fermées après réinitialisation.
- **Mails** : `send_mail()` unique, texte brut, SMTP authentifié sur une boîte dédiée AlwaysData, paramètres (serveur, boîte, expéditeur, plafonds) réglés depuis la console d’administration et rangés en base, `.env` en repli — rien d’écrit en dur ; limites par adresse, par IP et journalières **avant** l’envoi (un abus peut faire couper le site par l’hébergeur). Expéditeur `aqualook@alwaysdata.net` (M1, tranché le 9 oct. 2026).
- **PIN LCD** : 4 à 6 chiffres, PBKDF2-HMAC-SHA256 salé dans l’espace NVS `aqlsec` (hors `ALOK`), temporisation croissante persistée. Protège ADMIN et le démarrage manuel ; **l’arrêt d’une zone en cours reste toujours libre**. Pas de PIN posé = comportement actuel avec rappel. Oubli : geste maintenu au splash, ou effacement seul depuis l’espace en ligne (lot ultérieur). `resetConfig` n’efface pas le PIN.
- **Web local** : le secret `ApiAuth` devient le mot de passe d’accès (10 caractères au moins) ; session ouverte par défi-réponse HMAC, cookie `HttpOnly; SameSite=Strict`, sessions en RAM. Toutes les écritures exigent la session ; les écritures déjà signées gardent leur HMAC pendant la transition ; lectures de diagnostic libres (liste à fixer, W1). Le verrou visuel et `1598753` disparaissent de `index.html`. HTTP en clair sur le LAN : risque d’écoute assumé.
- **Lot F, complément du 10 oct. 2026** (classement W1 : `ENROLEMENT_ET_SECURITE_LOCALE.md` §8.1) : filtre unique en tête du routage, installé avant toute route ; tout `POST` exige la session (arrêt d’une zone compris, décision du propriétaire), Wi-Fi libre en portail captif seulement ; essais limités **par adresse IP** ; échec fermé si la NVS est illisible ; écritures d’un autre site refusées (`Origin`, `Sec-Fetch-Site`) et API refusée sous un nom d’hôte achetable (anti « DNS rebinding »), même sans mot de passe posé ; page `/login` intégrée au firmware ; `/` mène à `/login` sans session ; plus de déconnexion dans l’interface ; bouton « Options avancées du module » (repli de la configuration, pas un verrou). Changement de mot de passe chiffré (B1) et premier mot de passe autorisé par un geste sur l’écran (B5) : validés sur `.141` le 10 oct. 2026 (`84806ca`), version `5.12.0`.
- **Hors périmètre** : alertes par mail, partage de module, TOTP, journal d’audit, export RGPD — arbitrés plus tard, lot par lot.

## D018 — Signature des mises à jour, indépendante de l'hébergeur (13 oct. 2026)

Demande du propriétaire, 13 octobre 2026 : que la sécurité des mises à jour ne dépende plus de l'hébergeur (AlwaysData aujourd'hui, peut-être un autre demain) ni de son accès FTP.

- **Le manifeste est signé** (ECDSA P-256 / SHA-256). Il porte la taille et le SHA-256 de chaque fichier : le signer authentifie tout le paquet. Signature dans un fichier texte voisin, `<url du manifeste>.sig` (`aqualook-sig-v1 <id> <base64>`), calculée sur les octets exacts du manifeste.
- **Le module vérifie avant toute utilisation** (`src/UpdateSignature.*`, appelé par `WebAssetsUpdater::checkForUpdate()` et `deployFromManifest()`), avant même d'analyser le manifeste. Signature absente, illisible, d'une clé inconnue ou fausse → rien n'est installé, les pages en service restent intactes, code court dans le résultat de maintenance (`signature-absente`, `-illisible`, `-cle-inconnue`, `-invalide`, `-injoignable`), expliqué dans `data/ota-help.json` ; journal `[MAJ-SIGNATURE]`. **Aucun repli sans signature** : un manifeste non signé est refusé.
- **ECDSA P-256 plutôt qu'Ed25519** : mbedTLS de la plate-forme (IDF 4.4) n'a pas Ed25519, et l'ECDSA P-256 y est déjà compilé pour TLS : aucune bibliothèque ajoutée.
- **Clés** : la clé privée reste hors du dépôt, hors de OneDrive et hors de l'hébergeur (`%USERPROFILE%\.aqualook\signature\cle-privee.pem`, droits restreints au compte du propriétaire, ou `AQUALOOK_SIGNING_KEY`), avec une sauvegarde hors ligne à faire par le propriétaire. Les clés **publiques** sont dans Git (`tools/cles_publiques/<id>.pem`) et intégrées au firmware par `src/UpdateSignatureKeys.h`, généré par `tools/update_signing.py entete`. Plusieurs clés peuvent être acceptées à la fois : changer de clé = ajouter la nouvelle, diffuser le firmware, puis retirer l'ancienne (révocation).
- **Chaîne de publication** : `tools/publish_web_assets.py` signe et revérifie ; ordre d'envoi : fichiers, puis `.sig`, puis manifeste ; `tools/check_published_web.py` contrôle aussi la signature, avec la même règle que le module.
- **Ce que la signature ne couvre pas encore** : le manifeste du firmware (OTA GitHub, voir P1.4, à signer de la même façon) ; le dépôt direct et la carte SD elle-même (SEC-015). Le dépôt direct `/api/debug/deploy-*` est absent du binaire de production depuis D020 (P1.8).
- **Anti-rejeu (P1.9 lot 2, 13 oct. 2026)** : un manifeste signé plus ancien que la version installée est refusé (`version-anterieure`), de même qu'un numéro qui n'est pas `X.Y.Z` (`version-illisible`), à la vérification comme à l'installation, juste après la signature (`replayRefused()`, `src/WebAssetsUpdater.cpp` ; comparateur `src/VersionCompare.h`). Sans version installée comparable (inconnue, « depot direct », SHA Git), rien n'est refusé : c'est le chemin qui remet un module sur une version publiée. **Revenir en arrière = republier sous un numéro supérieur.**

## D019 — Mises à jour visibles partout, interface cohérente, nom local du module (13 oct. 2026)

Demandes du propriétaire, 13 octobre 2026.

- **Une seule interface, deux accès** : l'UI vue sur le module (réseau local) et celle du site sont **cohérentes** (mêmes couleurs, pastilles, libellés). Mise à jour disponible = pastille « ⬆ MAJ » violette `#6633cc` (`--purple`), blanche, clignotante (`blink 2s`), dans le bandeau du module, du site et de l'éditeur en ligne ; versions présentées « Programme du module · installé X » et « Pages Web · installé Y → disponible Z » (classes `upd-*` communes).
- **Le serveur annonce, le module signale** : la réponse au rapport `diag` porte `webAvailable` (version des pages publiée). Plus récente que l'installée → bandeau LCD « MAJ DISPO », voyant violet, pastille Web, `ota.html`, sans attendre la vérification nocturne. **Information seulement** (D014 inchangée : rien ne s'installe sans l'utilisateur) ; l'installation reste vérifiée par la signature (D018). Le rapport `diag` porte en retour la version des pages installées (`webAssets`) et le nom local (`netName`).
- **Après une mise à jour réussie**, le module renvoie son état dès que l'heure est connue (synchro forcée), pour que le site cesse aussitôt d'annoncer la mise à jour ; il attend toutefois la fin d'un arrosage.
- **Nom local** : le module répond à `http://<nom>.local` (mDNS, et nom DHCP pour la box), `<nom>` = nom donné par l'utilisateur dans son espace en ligne (accents translittérés, 32 caractères), transmis à chaque synchro et mémorisé en NVS (`aq_net`) ; sans nom donné, `aqualook-<fin MAC>`. Jamais l'identifiant technique.
- **Mode d'emploi** `maj.html` : une étape à la fois, illustrations SVG fidèles aux vrais écrans, lien direct vers le module ; l'adresse IP lue sur le LCD reste le recours.

## D020 — Routes de développement absentes du binaire de production (13 oct. 2026)

P1.8, décision du propriétaire du 13 octobre 2026.

- **Fermées par défaut** : les onze routes `/api/debug/*` (dépôt direct sur la SD `deploy-begin`/`deploy-file`/`deploy-commit`, écriture du catalogue `log-messages`, `sd-selftest`, `verify-web-asset` et son état, `heap-info`, `nvs-stats`, `script-selftest`, `script-dryrun`) ne sont enregistrées que si `AQUALOOK_DEV_ROUTES=1` (`src/OtaBuildIdentity.h`, défaut `0`). Absentes, elles répondent 404. Un oubli ne peut donc jamais les ouvrir en production.
- **Deux environnements** : `ProgrammeArrosage_s3` est le binaire de production (routes fermées) ; `ProgrammeArrosage_s3_banc` (`extends` du précédent + `-DAQUALOOK_DEV_ROUTES=1`) est flashé sur le module de banc `.141`.
- **Visible** : bannière série (`Routes debug : OUVERTES (binaire de banc)` / `fermees (production)`) et `/api/diagnostics` (`build.env`, `build.devRoutes`).
- **Toutes fermées, lectures comprises** : `heap-info` et `nvs-stats` étaient des diagnostics temporaires ; `/api/diagnostics` et `/api/health` couvrent le service.
- **OTA firmware** : `MaintenanceBoot` compare l'environnement du manifeste à celui du binaire ; un binaire de banc ne recevra donc jamais un firmware de production par OTA (et inversement). À reprendre avec P1.4 si le banc doit suivre l'OTA.

## D021 — Release du programme S3 seule, compilée par GitHub, signée sur le poste du propriétaire (14 oct. 2026)

P1.4 et P1.9 lot 2b, choix du propriétaire du 14 octobre 2026.

- **Compilation par GitHub Actions** (`.github/workflows/ota-release.yml`, déclenché par une étiquette `vX.Y.Z` égale à `VERSION`) : binaire de **production** `ProgrammeArrosage_s3` seul, manifeste `aqualook-manifest.json` (une seule cible `s3`, carte `jc4827w543c_i`, environnement `ProgrammeArrosage_s3`), `SHA256SUMS.txt`. Plus de carte historique ni de pages Web dans la release (les pages ont leur catalogue signé sur AlwaysData).
- **Release en brouillon** : invisible des modules (`releases/latest` ignore les brouillons). La CI refuse de modifier une release déjà publiée.
- **Signature hors ligne** (D018) : la clé privée ne quitte pas le poste du propriétaire ; `tools/release_firmware.py signer X.Y.Z` télécharge le brouillon, contrôle tout ce que le module contrôlera (version, cible, carte, environnement, adresse, taille, SHA-256, image ESP32-S3, binaire de production et non de banc), signe le manifeste, ajoute `aqualook-manifest.json.sig`, publie comme dernière release et relit ce que reçoivent les modules. `verifier` ne change rien ; `publier-sans-signature` sert seulement à l'essai du refus.
- **Module** : `CHECK_VERSION` télécharge `<manifeste>.sig` et vérifie la signature sur les octets exacts **avant** toute analyse ; aucun repli non signé (`signature-absente`, `-injoignable`, `-illisible`, `-cle-inconnue`, `-invalide`). Le téléchargement (`OtaStageUpdate`) n'accepte qu'une vérification réussie, et une version antérieure ou identique n'est jamais proposée.
- **Identité** : cible OTA et carte fixées par `tools/version_build.py` (`s3` / `jc4827w543c_i` pour les environnements S3 ; carte historique `unsupported`) ; alias `ProgrammeArrosage_legacy` et `_v4` retirés.
- **Banc** : `ProgrammeArrosage_s3_banc` ne suit pas l'OTA (`environment-mismatch`, D020) ; un essai OTA se fait en flashant `.141` en production par USB, puis retour au banc par USB.
- **Leçon** : la lecture du manifeste ajoutait un octet parasite (invisible pour le JSON) ; tout contenu signé se lit aux octets exacts (`Content-Length`, `read()` en échec ignoré, corps tronqué refusé). Un module en **5.17.1** garde ce défaut et refuse toute release signée : le remettre à jour par USB.
