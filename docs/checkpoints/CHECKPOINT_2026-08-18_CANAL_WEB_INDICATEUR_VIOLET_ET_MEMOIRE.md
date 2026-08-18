# Checkpoint AquaLook — canal ressources Web périodique, indicateur violet unifié, cartographie mémoire

## Référence

- Dépôt : `cnuma/AquaLook`
- Branche : `agent/ota-3.1-stage-inactive`
- Commit de tête à la clôture : `cca9f41`
- `VERSION` du dépôt : `5.9.7` (inchangée, aucune release publiée cette session)
- Date : 2026-08-18 (nuit du 17 au 18, suite directe de `CHECKPOINT_2026-08-17_RESILIENCE_BOOTLOOP_ET_MAJ_RESSOURCES_WEB.md`)
- Documents associés : `docs/engineering/38_MEMORY_MANAGEMENT.md` (nouveau), le checkpoint du 17 août ci-dessus (référence opérationnelle : chemins d'outillage, cartographie NVS, garde-fous, routes HTTP — toujours valable, non répétée ici)

## Source de vérité

Reprendre exclusivement depuis la branche Git ci-dessus, et lire dans l'ordre :

1. `AGENTS.md`
2. `docs/checkpoints/CHECKPOINT_2026-08-17_RESILIENCE_BOOTLOOP_ET_MAJ_RESSOURCES_WEB.md` — référence opérationnelle toujours valable
3. le présent checkpoint
4. `docs/engineering/38_MEMORY_MANAGEMENT.md` — cartographie mémoire, avant toute nouvelle intervention touchant l'allocation dynamique
5. `platformio.ini`

Ne pas repartir d'un extrait de conversation ni d'une archive antérieure.

---

## État du matériel à la clôture

| Élément | Valeur |
|---|---|
| Firmware flashé | 5.9.7, build 923, `sha=270729f` (état sans arène mbedTLS — voir plus bas) |
| Adresse IP | `192.168.1.198` |
| Port série | `COM7` sur cette machine (différent de `COM3` utilisé la veille sur une autre machine, et de `COM9` déclaré dans `platformio.ini`) — toujours reconfirmer avec `pio device list` |
| Ressources Web sur SD | version `5.9.7`, alignées avec le firmware |
| Vérification auto | activée, 03:30, tous les jours — validée en conditions réelles cette nuit (voir plus bas) |
| Mode dégradé | inactif |
| Tas libre au repos | ~157 Ko (écran état variable), plus gros bloc 110 580 o |

Le module tourne sainement, WiFi connecté du premier coup, aucune arène mbedTLS active (retirée après tests négatifs — voir § dédiée).

---

## Ce qui a été livré et validé sur matériel

### 1. Déploiement des ressources Web en 5.9.7

Écart signalé par le checkpoint du 17 août (carte restée en 5.9.6) : comblé. Déploiement réel via `/api/webassets/update`, 10/10 fichiers, SHA-256 d'`index.html` recontrôlé indépendamment et conforme au manifeste publié.

### 2. Canal ressources Web dans la vérification périodique (commit `bed43e7`→`e610cc6`)

`WebAssetsUpdater::checkForUpdate()` existait déjà (comparaison manifeste vs version installée, sans rien télécharger) mais n'était jamais appelée. La vérification périodique `CHECK_VERSION` (3h30) déclenche désormais aussi ce contrôle, dans le même redémarrage de maintenance que le firmware — pas de redémarrage supplémentaire.

- `MaintenanceResult` : 4 nouveaux champs `webAssets*`, schéma NVS `aq_maint_res` porté de 1 à 2. Le blob est protégé taille+CRC : un ancien blob de taille différente est rejeté et réinitialisé automatiquement, aucune migration explicite nécessaire.
- Fusion symétrique au firmware dans `MaintenanceResult::save()` : un déploiement réussi consomme la notification en attente, toute autre commande préserve le dernier résultat connu, et une même version déjà notifiée ne redéclenche pas l'alerte à chaque vérification (anti-spam).
- `NotificationManager` : nouveau `WorkType::WEB_ASSETS_UPDATE_AVAILABLE`, même cycle détection/envoi/accusé que la notification firmware existante.

**Validé sur matériel réel** : cas à jour (aucune notification), cas d'écart simulé — version locale falsifiée à 5.9.6, `check_version` déclenché, notification livrée avec succès (`http=200` sur ntfy.sh) — puis anti-spam vérifié sur une 3ᵉ vérification identique (aucun renvoi), puis état réel restauré par un redéploiement authentique.

### 3. Indicateur violet unifié pour une mise à jour disponible (commit `7afae24`→`e610cc6`)

LED, icône LCD (déjà existante mais réservée au firmware jusqu'ici) et pastille Web utilisent désormais la même teinte violette (`#6633cc`, déjà `--purple` dans `style-base.css`) pour signaler une mise à jour disponible — firmware ou ressources Web, sans distinction : l'écran 240×320 n'a pas la place pour deux icônes, et le message est le même dans les deux cas ("va voir `/ota`").

- `Theme::PURPLE` réaligné sur `0x6199` (équivalent RGB565 de `#6633cc`). Constante non utilisée avant ce changement, aucun autre affichage impacté.
- `ScreenManager` : nouveau palier LED, sous arrosage/WiFi mais au-dessus du mode utilisateur normal ; rouge reste réservé aux pannes (invariant préservé).
- **Bug trouvé et corrigé en testant sur matériel réel** : `NotificationStatus` n'exposait que des champs `*Pending` (vrais seulement jusqu'à l'envoi de LA notification) — le badge Web se serait éteint dès la notification livrée, alors que la mise à jour restait non déployée. Ajout de `updateAvailable`/`webAssetsUpdateAvailable`, persistants, relus depuis l'état déjà en mémoire (`g_updateResult`) sans lecture NVS supplémentaire sur un `/api/status` interrogé fréquemment.

**Validé sur matériel réel** : compilation Legacy+V4, flash, état persistant confirmé par `/api/status` après redémarrage, avant et après correction du bug. Vérification visuelle LED/LCD **non faite** (utilisateur non présent devant l'appareil) — à confirmer à l'occasion.

### 4. Cartographie mémoire (`docs/engineering/38_MEMORY_MANAGEMENT.md`)

Inspection directe du code avant toute nouvelle intervention sur l'allocation dynamique. Trouvailles principales :

- `notify-sender` recrée une pile FreeRTOS de 4 Ko à *chaque* notification envoyée (`xTaskCreatePinnedToCore` puis `vTaskDelete`), jamais une tâche permanente réutilisée — churn récurrent non documenté jusqu'ici.
- Tampons I/O mbedTLS : 16 Ko de base (`CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN`, vérifié dans le sdkconfig du framework), et `WiFiClientSecure::setBufferSizes()` n'existe pas sur ce cœur — impossible de les réduire par l'API publique.
- `NotificationManager` n'a aucun garde mémoire avant l'envoi, contrairement à la météo et la SD — cohérent avec l'incident de boucle du 17 août, mais hors périmètre de toute correction cette session (les notifications passent en HTTP simple, pas en TLS).

### 5. Tentative d'arène mbedTLS statique — résultat négatif, documenté

Objectif : isoler les tampons TLS du tas général pour empêcher la collision avec les sprites d'écran (documentée depuis le 16 août). Implémentée (`MbedtlsArena.h/.cpp`, allocateur first-fit avec garde-fous anti-corruption), testée sur matériel réel à trois tailles :

| Taille | Résultat |
|---|---|
| 64 Ko | échec de lien (dépassement DRAM de 15 712 octets, mesure du linker) |
| 40 Ko | boote, mais le TLS échoue quand même (`SSL - Memory allocation failed`) |
| 48 Ko | **le pilote WiFi lui-même échoue à s'initialiser au démarrage** (`wifi nvs cfg alloc out of memory`), confirmé non transitoire sur 2 tentatives |

**Conclusion** : aucune taille statique ne fonctionne — la marge DRAM disponible sur cet ESP32 sans PSRAM est trop étroite. Code retiré, firmware restauré à l'état précédent. Piste corrigée documentée (arène **dynamique**, allouée depuis le tas général au moment de l'appel TLS plutôt qu'au démarrage) mais **non implémentée**. Chantier mis en pause, en attente d'une carte avec PSRAM.

---

## Incident de session — corruption locale du dépôt Git, récupérée

Pendant cette session, `git status`/`git add` ont commencé à échouer avec `fatal: mmap failed: Invalid argument`. Diagnostic : un objet Git local (l'arbre `docs/engineering` à `HEAD`) corrompu sur cette machine, probablement lié aux soucis d'écriture Windows/OneDrive rencontrés plus tôt dans la nuit (voir `git config windows.appendAtomically false`, déjà appliqué). Le contenu réel n'était pas perdu : une copie fraîche du dépôt distant le lisait sans problème.

**Récupération** : les 2 commits locaux non poussés au moment de l'incident ont été exportés en patches, le `.git` local corrompu remplacé par un clone frais vérifié sain (`git fsck --full` propre), puis le contenu recombiné en un commit unique (`e610cc6` — les deux fonctionnalités touchaient toutes deux `NotificationManager.cpp/h`, impossible de les re-séparer proprement sans risque). **Aucun code perdu**, seule la séparation fine en deux commits ne l'a pas été.

Dépôt revérifié sain (`git fsck --full` propre) après récupération et à la clôture de cette session.

---

## Limites connues, non corrigées

- Reprises de `CHECKPOINT_2026-08-17...` toujours valables : contrainte mémoire de fond au démarrage (~5 Ko libres avant remontée), défaut `ESPAsyncWebServer` non corrigé (contourné), branche `wip/step6-webassets-check` toujours parquée, routes `/api/debug/*` toujours ouvertes sans authentification, table de partitions non déployable par OTA.
- `NotificationManager` sans garde mémoire avant envoi (voir cartographie, § E) — non corrigé.
- Vérification visuelle de l'indicateur violet (LED + icône LCD) non faite.
- Arène mbedTLS : voir § dédiée ci-dessus, en attente de PSRAM.

---

## Procédure de reprise

```powershell
Set-Location "C:\Users\Emman\OneDrive\Documents\VsCode_travail\arrosage"

git fetch origin
git switch agent/ota-3.1-stage-inactive
git pull --ff-only origin agent/ota-3.1-stage-inactive

git log --oneline -8
Get-Content VERSION
Get-Content docs\checkpoints\CHECKPOINT_2026-08-18_CANAL_WEB_INDICATEUR_VIOLET_ET_MEMOIRE.md

pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

Avant tout upload, confirmer le port réel (a déjà changé plusieurs fois d'une machine/session à l'autre) :

```powershell
pio device list
```

Si `git status` échoue avec `fatal: mmap failed: Invalid argument` : corruption locale d'objet, voir § "Incident de session" ci-dessus pour la procédure de récupération (clone frais + `git fsck --full` pour vérifier, remplacement du `.git`, ré-application des commits locaux via patches).

---

## À faire en priorité à la reprise

1. **Confirmer visuellement l'indicateur violet** (LED + icône LCD) — jamais vu en direct cette session.
2. **Décider du sort de l'arène mbedTLS dynamique** (piste documentée dans `38_MEMORY_MANAGEMENT.md`) — soit l'implémenter, soit attendre la carte PSRAM comme prévu.
3. **Garde mémoire pour `NotificationManager`** — identifié par la cartographie, jamais traité, cohérent avec l'incident de boucle du 17 août.
4. Reste de la spécification UX d'origine, déjà réduite : la pastille Web est faite, mais vérifier son comportement en conditions réelles (plusieurs onglets, reconnexion réseau).
5. Nouveau chantier à cadrer : évaluation d'un cloud connecté au module (environnement de test local avant serveur dédié) — discussion démarrée en fin de session, pas encore de document dédié.
