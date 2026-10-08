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
