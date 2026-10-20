# AquaLook

Programmateur d'arrosage autonome sur ESP32-S3 : écran tactile, pages Web
locales, prévisions de pluie, scripts embarqués et espace en ligne
facultatif.

Version courante : fichier [`VERSION`](VERSION) (5.20.0 au 20 octobre 2026),
releases signées sur la page GitHub du dépôt.

**Principe directeur** : l'arrosage et les sécurités fonctionnent sans
Wi-Fi, sans Internet, sans serveur et sans carte SD. Tout le reste est une
couche optionnelle.

## Fonctions

- **1 à 8 zones**, planification par **jours fixes** ou par **intervalle**
  (tous les N jours), 5 créneaux par jour et par zone ; arrosage manuel ;
  durée maximale de sécurité sur chaque vanne.
- **Pluie** : annulation de l'arrosage programmé selon un seuil par zone,
  prévisions Open-Meteo (modèles Météo-France) ou OpenWeatherMap.
- **Moteur d'exécution V4** (zones → équipements → cartes → ports → voies) ;
  câblage décrit comme une donnée dans « Cartes interfaces », seule couche
  d'entrées/sorties (sorties et entrées TOR). Cartes relais I²C XL9535
  (validée sur matériel) et MCP23017 (déclarable, pas encore validée sur
  matériel).
- **Écran tactile** 480×272 : planning, arrosage manuel, administration
  protégée par code PIN, page Santé ; ruban lumineux (une LED d'état, une
  LED par zone).
- **Pages Web locales**, servies depuis la carte SD, responsives jusqu'à
  390 px ; écriture protégée par une session locale (mot de passe Web,
  défi-réponse HMAC) ; politique de sécurité de contenu stricte (aucun
  script inline, aucune ressource externe).
- **Scripts** : langage maison, éditeur en schéma dans le navigateur,
  exécution non bloquante sur le module (entrées, démarrages et arrêts de
  zones).
- **Notifications** ntfy.
- **Espace en ligne** facultatif (PHP + MySQL, hébergé sur AlwaysData) :
  rattachement d'un module par code court, suivi et réglages à distance,
  éditeur de scripts en ligne.
- **Mises à jour** du programme (releases GitHub) et des pages Web,
  **signées** et **toujours déclenchées par l'utilisateur**, jamais à
  distance.
- **Résilience** : garde de démarrage (mode dégradé après redémarrages
  répétés), retour arrière automatique d'une mise à jour non validée,
  journal d'événements, défauts et incidents suivis.

## Matériel de référence

| Élément | Valeur |
|---|---|
| Carte | Guition JC4827W543C_I (ESP32-S3, PSRAM 8 Mo) |
| Écran | NV3041A 480×272, tactile capacitif GT911 |
| Sorties | carte relais I²C XL9535 (adresse `0x20`, bus dédié) |
| Stockage | carte SD (pages Web), NVS (configuration), LittleFS (secours) |
| Voyant | ruban WS2812 |

Brochage : section `[jc4827w543c_i]` de [`platformio.ini`](platformio.ini).
L'ancienne carte CYD (ESP32-2432S028) est en cours d'abandon.

## Documentation utilisateur

- [`docs/guides/manuel/`](docs/guides/manuel/README.md) : manuel
  utilisateur (référence de ce que voit et fait l'utilisateur)
- [`docs/guides/MONTER_UN_MODULE_NEUF.md`](docs/guides/MONTER_UN_MODULE_NEUF.md) :
  installer un module neuf, de la carte nue au rattachement

## Documentation technique

Commencer par [`AGENTS.md`](AGENTS.md) (règles du projet), puis
[`docs/START_HERE.md`](docs/START_HERE.md) (ordre de lecture) :

- `docs/codex/` : contexte, architecture, décisions, invariants, règles,
  compilation et tests ;
- `docs/REPRISE_INGENIEUR.md` : dossier de reprise détaillé ;
- `docs/checkpoints/` : état daté du projet (le plus récent fait foi) ;
- `docs/TODO.md` : reste à faire, par priorité ;
- `docs/security/` : référentiel de cybersécurité.

## Compilation

Framework Arduino, plate-forme `espressif32 @ 6.13.0`, PlatformIO.

| Environnement | Usage |
|---|---|
| `ProgrammeArrosage_s3` | binaire de production |
| `ProgrammeArrosage_s3_banc` | même programme avec les routes de développement, pour le module de banc |
| `test_*_s3` | bancs matériels ciblés |

```powershell
# Compilation du binaire de production
pio run -e ProgrammeArrosage_s3

# Programme et moniteur sur le module de banc (port à confirmer : pio device list)
pio run -e ProgrammeArrosage_s3_banc -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0

# Image LittleFS (seulement si littlefs/ change)
pio run -e ProgrammeArrosage_s3 -t buildfs
```

Les pages Web (`data/`) ne passent pas par LittleFS : elles sont publiées en
paquet signé et installées sur la carte SD par la page de mise à jour du
module. Procédures complètes (release, publication, essais) :
[`docs/codex/05_BUILD_AND_TEST.md`](docs/codex/05_BUILD_AND_TEST.md).

GitHub Actions compile `ProgrammeArrosage` et `ProgrammeArrosage_s3` à
chaque envoi sur `main` (`.github/workflows/platformio-v4-domain.yml`) et
construit la release brouillon à chaque tag `vX.Y.Z`
(`ota-release.yml`).

## Sécurité

Le dépôt est public : la sécurité ne repose jamais sur le secret du code, et
aucun secret (mot de passe, jeton, clé) n'y est enregistré. Signaler une
faille au propriétaire du dépôt plutôt que dans une issue publique.
