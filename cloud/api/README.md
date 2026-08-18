# API AquaLook — HTTP/HTTPS à jeton porteur

Trajectoire retenue depuis le 18 août 2026 (voir `docs/architecture/SYSTEM_ARCHITECTURE.md` §5.0
et `docs/architecture/CLOUD_ENVIRONMENT_EVALUATION.md` §7) : pas de courtier permanent, le module
initie toujours la connexion, en connexions courtes. Ce dossier est le pendant HTTP de
`cloud/bridge` (MQTT, différé mais conservé) — même rôle, transport différent.

## Ce que fait ce service

Trois routes côté module (authentifiées par jeton porteur propre à chaque module) :

| Route | Sens | Rôle |
|---|---|---|
| `POST /v1/report` | module → serveur | télémétrie, état, événement ou diagnostic |
| `GET /v1/pending-command` | module → serveur | « as-tu quelque chose pour moi ? » — sondage |
| `POST /v1/command/ack` | module → serveur | accuse réception d'une commande, avec résultat |

Deux routes côté administration (jeton admin séparé) :

| Route | Rôle |
|---|---|
| `POST /admin/command` | dépose une commande en attente pour un module (ex. nouveaux créneaux) |
| `GET /admin/modules` | liste des modules connus et dernière présence |

Le module reste toujours celui qui ouvre la connexion — jamais de connexion entrante acceptée,
même posture que l'OTA. La latence d'une commande poussée dépend de l'intervalle de sondage du
module, pas de ce service.

## Schéma de données

Repris tel quel du schéma déjà conçu pour la piste MQTT (`cloud/db/init/01-schema.sql`) — les
tables `module`, `module_message` et `command` sont indépendantes du transport. Seule différence
ici : SQLite plutôt que PostgreSQL/TimescaleDB, pour démarrer sans service supplémentaire à
installer ou à administrer. Migration vers PostgreSQL possible plus tard sans changer la forme du
schéma (voir `cloud/db/init/01-schema.sql` pour l'équivalent).

- `module` : référentiel des modules connus (identifiant, étiquette, firmware, dernière présence).
- `module_message` : historique des messages reçus (`status`, `state`, `event`, `diag`), charge
  utile en JSON, jamais réécrite après coup (les contrats évoluent, versionnés).
- `command` : commandes déposées côté serveur, avec état (`pending` → `accepted`/`refused`/
  `failed`/`expired`), horodatage de règlement et résultat — traçabilité exigée par
  `SYSTEM_ARCHITECTURE.md` §7.
- `module_token` : jeton porteur par module. Un jeton = un module, jamais de secret partagé.

## Sécurité

- Jeton porteur par module (`module_token`), vérifié sur chaque requête `/v1/*`.
- Jeton admin distinct (variable d'environnement), pour `/admin/*` uniquement.
- Charge utile bornée à 64 Ko (même limite que la piste MQTT).
- Un accusé de réception sur une commande déjà réglée est un no-op, pas un retraitement —
  protection contre le rejeu.
- **HTTPS non fourni par ce service seul** : en local sur ce PC, HTTP suffit pour développer.
  Avant tout accès depuis l'extérieur, mettre un reverse proxy TLS devant (Caddy, déjà présent
  dans `cloud/caddy/` et indépendant du transport — obtient et renouvelle les certificats
  Let's Encrypt automatiquement).

## Démarrage local (Windows, sans Docker)

```powershell
cd cloud\api
python -m venv .venv
.venv\Scripts\pip install -r requirements.txt
Copy-Item .env.example .env
# éditer .env : générer un jeton par module et un jeton admin
.venv\Scripts\python -c "import secrets; print(secrets.token_urlsafe(32))"

.venv\Scripts\uvicorn app.main:app --host 0.0.0.0 --port 8000
```

La base SQLite (`data/aqualook.db`) et ses tables sont créées automatiquement au premier
démarrage si absentes.

Vérification rapide :

```powershell
curl.exe http://localhost:8000/health
```

## Migration future

- **Vers un hébergement Python géré** (Render, Railway, PythonAnywiere, VPS...) : ce service est
  un simple processus ASGI (`uvicorn`), sans dépendance à Docker. À noter : contrairement à un
  hébergement mutualisé PHP classique, il faut un hôte capable de faire tourner un processus
  Python — ce qui reste une catégorie large et peu coûteuse, mais pas *n'importe quel*
  hébergement à 2 €/mois.
- **Vers PostgreSQL/TimescaleDB** : remplacer les fonctions `db.py` (actuellement `sqlite3` de la
  bibliothèque standard) par `psycopg`, réutiliser directement `cloud/db/init/01-schema.sql`.
- **Vers MQTT** (si reconsidéré) : `cloud/bridge` existe déjà en parallèle, même schéma de
  données, à activer sans migration de données si la table `module`/`module_message`/`command`
  est partagée.
