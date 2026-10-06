# Configuration du module depuis un serveur externe

Conception arrêtée le 29 août 2026 avec l'utilisateur, sur la branche
`feat/cloud-remote-config`.

## Point de départ : ce qui existait déjà

Rien de tout ceci n'a été construit pour ce chantier. `CloudSync` était en
place, avec un contrat d'API défini et deux implémentations serveur de
référence (`cloud/api` en FastAPI, `cloud/php-mutualized` en PHP).

**Chaîne vérifiée de bout en bout avant toute conception**, le 29 août, avec
le serveur de référence lancé sur le poste de l'utilisateur
(`192.168.1.169:8000`) et le module pointé dessus :

```
POST /v1/report          200   remontée d'état
POST /v1/report          200   remontée de configuration effective
GET  /v1/pending-command 200   récupération d'une commande déposée
POST /v1/command/ack     200   accusé de réception
```

La commande de test est passée à `accepted` en 4 secondes. Le transport,
l'authentification par jeton et la traçabilité fonctionnent.

**Le seul chaînon manquant** est l'application de la commande, que le
firmware annonce lui-même :

```cpp
r["note"] = "recue, application non encore implementee cote firmware";
```

## Invariant préservé : le module reste l'autorité

`CloudSync.h` pose que le module est l'autorité et le serveur un miroir. La
raison est concrète : sans cela, un réglage fait sur l'écran tactile serait
écrasé au sondage suivant, en silence.

Ce chantier ne remet pas cet invariant en cause. Il le rend opérationnel :
le serveur **propose**, le module **arbitre**.

## Décisions

### 1. Périmètre — configuration seulement

Sont applicables à distance : créneaux d'arrosage, réglages de zone,
paramètres système d'affichage, options météo, seuils d'alerte vent — et,
depuis le 6 octobre 2026, scripts et phrases (voir « Élargissement du
6 octobre 2026 » plus bas).

Sont **refusés**, quelle que soit la commande :

| Interdit | Pourquoi |
|---|---|
| Démarrer ou arrêter un arrosage | une action immédiate n'est pas une configuration. Un serveur compromis ne doit pas pouvoir noyer le jardin |
| Modifier les identifiants WiFi | une valeur fausse coupe le module du réseau **définitivement** — plus de serveur, plus d'interface web, seul l'accès physique reste |
| Déclencher une mise à jour ou un redémarrage | chemins déjà couverts par l'OTA, avec leurs propres garde-fous |

Ces refus ne sont pas silencieux : ils sont renvoyés dans l'accusé avec un
motif explicite.

### 2. Conflit avec une modification locale — le local gagne toujours

Choix explicite de l'utilisateur. Réalisé par **verrouillage optimiste**,
et non par un simple « dernier arrivé gagne » :

- `ConfigManager` tient un compteur `configRevision`, incrémenté à chaque
  écriture locale de configuration ;
- ce compteur est remonté dans `/v1/report` avec la configuration ;
- une commande porte `baseRevision`, la version sur laquelle le serveur
  s'est appuyé ;
- **si `baseRevision` ne correspond pas à la version courante, la commande
  est refusée** avec le motif `config-modifiee-localement`.

C'est le schéma classique du verrouillage optimiste. Il honore « le local
gagne » sans bloquer quoi que ce soit : le serveur relit la configuration au
cycle suivant et peut reproposer sur une base à jour.

Une commande sans `baseRevision` est refusée elle aussi : accepter
reviendrait à écrire à l'aveugle, exactement ce que cette règle interdit.

**Aucune modification serveur nécessaire** : `/admin/command` accepte déjà un
objet libre, `baseRevision` y tient sa place.

### 3. Format — partiel

Seuls les champs présents dans la commande sont appliqués. Le reste est
laissé intact.

Ce n'est pas plus complexe que le format complet ici, parce que c'est déjà
le motif employé par le gestionnaire `POST /api/display` du module : chaque
champ est testé (`if (doc["x"].is<int>())`) avant d'être repris. Le partiel
est en outre plus sûr — une commande tronquée ou mal formée ne peut pas
effacer des réglages qu'elle ne mentionne pas.

## Forme d'une commande

```json
{
  "type": "config.apply",
  "baseRevision": 42,
  "zones": [
    { "i": 0, "name": "Tomates", "mode": 0, "intervalDays": 2 }
  ],
  "system":    { "screenTimeoutMin": 5 },
  "windAlert": { "gustKmh": 25, "severeKmh": 45 }
}
```

Et l'accusé correspondant :

```json
{
  "correlationId": "...",
  "state": "accepted",
  "result": { "applied": ["zones[0].name", "windAlert.gustKmh"],
              "revision": 43 }
}
```

En cas de refus :

```json
{
  "correlationId": "...",
  "state": "refused",
  "result": { "reason": "config-modifiee-localement",
              "expected": 42, "current": 45 }
}
```

## Stockage du compteur de version

Sous sa **propre clé NVS**, comme les seuils d'alerte vent et les ancres
d'intervalle — et non dans `CfgSystem` ou `CfgDisplay`.

Raison identique à celle déjà rencontrée le 29 août : ces structures sont
stockées par valeur dans `PersistedConfig`, dont la taille est vérifiée au
chargement (`read == sizeof(PersistedConfig)`). Les agrandir invaliderait
tout bloc déjà enregistré — identifiants WiFi, noms de zones et planning
compris — à moins de figer une copie de l'ancienne structure pour migrer.

## Sécurité de la transaction

Exigence de l'utilisateur : la transaction doit être sûre, car elle
transitera à terme par Internet ; chaque module doit être unique et
indépendant.

### Ce qui était déjà correct

| Exigence | Mécanisme en place |
|---|---|
| Un module = une identité unique | table `module_token`, contrainte `UNIQUE` sur le jeton |
| Parler au bon module | **l'identité est déduite du jeton**, jamais lue dans la charge utile : `require_module()` résout le porteur en `module_id`, et c'est celui-là qui range les messages et sélectionne les commandes. Mentir sur `moduleId` dans le JSON ne sert à rien |
| Modules indépendants | messages et commandes cloisonnés par ce `module_id` |
| Jeton invalide | 401 explicite, jamais de traitement silencieux |
| Administration | jeton distinct, réservé à `/admin/*` |
| TLS | `WiFiClientSecure` + `setCACert(ROOT_CA_PEM)` — vraie validation de chaîne, jamais `setInsecure()`. Réserve d'autorités : DigiCert, Sectigo/USERTrust, ISRG |

Passer à Internet est donc un changement de **posture**, pas de code :
activer `useHttps`, et placer un proxy TLS devant le serveur
(`cloud/caddy/`). Tant que la liaison est en HTTP, le jeton porteur circule
en clair — acceptable sur le LAN, jamais au-delà.

### Contrainte mémoire levée sur ESP32-S3 — mesuré le 29 août 2026

`CloudSync.h` et `MaintenanceRequest.h` posaient qu'une poignée de main TLS
exige environ 16 Ko de tas contigu que le fonctionnement normal ne peut pas
garantir, d'où un passage par le mode maintenance — donc **un redémarrage
par synchronisation**. À un cycle toutes les 15 minutes, rédhibitoire.

Cette contrainte a été mesurée sur la carte ESP32-2432S028R, qui disposait
d'environ 17 Ko de tas libre au repos. La JC4827W543C_I en rapporte **207 Ko**.

Vérification faite plutôt que supposée : `CloudSync` a été pointé
temporairement vers un hôte HTTPS réel (`github.com`, dont le certificat
chaîne vers la réserve embarquée), en fonctionnement normal.

```
POST /v1/report          -> http=422
GET  /v1/pending-command -> http=404
```

Ces codes sont le résultat attendu — l'hôte n'a pas ces routes. Ce qui
compte est qu'il a fallu, pour les obtenir, une connexion TLS complète :
poignée de main, validation du certificat, requête et lecture de la
réponse. Sans redémarrage, sans plantage, dans la boucle principale.

**Conclusion : le mode maintenance est inutile pour ce chemin sur cette
carte.** À noter que le gain ne vient PAS de la PSRAM : le framework force
les tampons mbedTLS en RAM interne (`CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=1`,
voir `docs/engineering/38_MEMORY_MANAGEMENT.md`). C'est la RAM interne bien
plus généreuse du S3 qui suffit.

## Élargissement du 6 octobre 2026 — scripts et phrases

Décision `D014` (`docs/codex/02_DECISIONS.md`). Les scripts et la
bibliothèque de phrases deviennent modifiables depuis l'espace utilisateur
(`app.html`), par `config.apply`. Les trois décisions ci-dessus (périmètre,
le local gagne, format partiel) s'appliquent sans changement.

### Ce qui reste refusé

| Interdit | Pourquoi |
|---|---|
| Lancer un script (« Lancer maintenant ») | lancer un script, c'est commander des vannes : une action immédiate, pas une configuration |
| Poser ou changer le secret HMAC (`ApiAuth`) | c'est lui qui protège les routes locales ; le céder au serveur reviendrait à lui confier le module entier |
| Modifier un script en cours d'exécution | remplacer le bytecode sous une machine qui l'exécute ; refus explicite, l'utilisateur recommence une fois le script terminé |

### Pas de signature de bout en bout — risque accepté

Les routes locales `/api/script-save`, `/api/script-erase` et
`/api/script-messages` sont signées HMAC. Le chemin cloud ne l'est pas, par
choix du propriétaire. Sur ce chemin, la confiance repose sur le jeton
porteur du module et sur TLS. Conséquence assumée : un serveur compromis
pourrait installer un script qui ouvre une vanne au prochain déclencheur
local. Le risque est borné par la durée maximale de sécurité des relais, qui
s'applique à toute ouverture, quelle que soit son origine.

### Révision unique

Jusqu'ici, un enregistrement local de script ne touchait pas
`configRevision` (ScriptStore a sa propre clé NVS). Il l'incrémente
désormais, tout comme un effacement de script ou une sauvegarde de phrases.
Sans cela, une commande distante pourrait écraser un script que
l'utilisateur vient de corriger sur place. Le compteur reste unique : une
édition locale de script fait aussi refuser une modification de planning
bâtie avant elle. Le serveur relit la configuration et repropose, comme pour
le planning.

### Remontée (module → serveur)

Un bloc `scripts` est ajouté au miroir de configuration (`buildConfigPayload`).
Comme le reste du miroir, il n'est envoyé que lorsque la révision change.

```json
"scripts": {
  "max": 6, "tailleMax": 400, "simultanes": 4,
  "emplacements": [
    { "i": 0, "nom": "Cuve pleine", "actif": true, "declencheur": 1,
      "cible": 3, "octets": 112, "source": "si entree ... " },
    { "i": 1 }
  ],
  "entrees": [ { "id": 3, "nom": "Flotteur cuve" } ],
  "phrases": { "max": 48, "lenMax": 80,
               "entries": [ { "code": 1, "texte": "Cuve pleine" } ] }
}
```

- `source` vient de la carte SD (`/scripts/s<i>.txt`). Il est absent si la SD
  manque ou si le fichier n'existe pas. L'éditeur cloud affiche alors
  l'emplacement en **lecture seule**, comme l'éditeur local devant un script
  sans source.
- Un source de plus de 4 096 octets n'est pas remonté : l'emplacement porte
  `"sourceTropLongue": true` et reste en lecture seule dans l'éditeur en
  ligne. Il ne tiendrait pas dans une commande de retour, et le tronquer
  ferait recompiler un texte faux.
- `"sd": false` signale une carte SD absente ou illisible. Ce cas se
  distingue d'un script qui n'a simplement jamais eu de source. Dans ce
  cas, `phrases` est absent : l'éditeur en ligne ne doit pas proposer de
  remplacer le catalogue par une liste vide.
- Le bytecode ne remonte pas : l'éditeur recompile le source.
- Les noms des zones sont déjà dans `zones[]`. L'identifiant **stable** de
  chaque zone (`zoneId`), cité par les scripts et distinct de l'index `i`, y
  est ajouté (`"id"`). Le nom d'une entrée est son libellé de rôle
  (`roleName`), comme dans l'éditeur local.

### Descente (serveur → module)

Une commande porte **soit un emplacement, soit le catalogue de phrases**,
jamais plusieurs scripts. C'est ce qui permet de tenir dans la taille de
réponse.

```json
{ "type": "config.apply", "baseRevision": 57,
  "scripts": [ { "i": 2, "nom": "Arrosage cuve", "actif": true,
                 "declencheur": 2, "cible": 1,
                 "code": "1a02...", "source": "quand zone ... " } ] }

{ "type": "config.apply", "baseRevision": 57,
  "scripts": [ { "i": 4, "efface": true } ] }

{ "type": "config.apply", "baseRevision": 57,
  "phrases": [ { "code": 1, "texte": "Cuve pleine" } ] }
```

- `code` est le bytecode en hexadécimal (800 caractères au plus, contre
  ~1,6 Ko en tableau JSON). Il est compilé dans `app.html` par le même
  `script-lang.js` que l'éditeur local.
- Le module applique **les mêmes contrôles** que les routes locales, sans la
  signature : `validateScriptProgram` via `ScriptStore::save`, déclencheur
  connu, cible non nulle si un déclencheur est posé, taille ≤ 400 octets.
  Pour les phrases : codes 1-65535 uniques, ≤ 48 entrées, ≤ 80 octets, aucun
  caractère de contrôle. Le catalogue est remplacé en entier, comme en
  local.
- Le source suit le bytecode, jamais l'inverse : si l'écriture SD échoue, le
  script tourne quand même, et l'accusé le signale.
- Un compilateur cloud plus récent que le firmware peut produire un opcode
  inconnu du module. La revalidation le refuse avec son motif : l'échec est
  sûr et visible dans l'accusé.

### Taille de la réponse de sondage

`httpExchange()` tronquait le corps à 4 096 octets ; un JSON tronqué ne
s'analyse pas, et la commande était perdue sans message. Le plafond passe à
**16 Ko**. Il est mesuré sur S3 (environ 200 Ko de tas interne libre au
repos) et suffit pour un script complet ou le catalogue de phrases (~5 Ko).
Côté serveur, `app.html` refuse avant envoi une commande qui dépasserait ce
plafond.

### Points à vérifier à l'implémentation

- La lecture des six sources et du catalogue sur SD a lieu dans
  `buildConfigBody()`, donc dans la boucle principale. Il faut mesurer sa
  durée (`runtimeComponents`) et l'accès concurrent à la SD avec le serveur
  Web.
- Pic mémoire du miroir (règle F15) : corps `String` de l'ordre de 20 Ko
  au lieu de ~3 Ko, à relever avant et après sur `.141`.

## Ce que cette conception ne traite pas

- **Le cloisonnement multi-clients côté serveur.** Un client pourra à terme
  posséder plusieurs modules ; les espaces sécurisés côté serveur sont
  explicitement reportés.
- **Le sens serveur → module en temps réel.** La latence d'une commande
  reste celle de l'intervalle de sondage. C'est un choix d'architecture
  assumé : le module n'accepte jamais de connexion entrante.
- **L'interface d'administration.** Déposer une commande passe par
  `POST /admin/command`. Aucune interface graphique n'est prévue ici.
