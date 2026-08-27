# AquaLook Engineering Reference — Gestion mémoire et fragmentation

- Version documentaire : 0.3
- Statut : cartographie initiale, issue d'une inspection directe du code le 18 août 2026
- Maturité : D2 (architecture et constats vérifiés dans le code ; pas encore de tests de charge systématiques ni de suivi dans le temps)
- Déclencheur : incident du 17 août 2026 (échec d'allocation des sprites d'écran après une rafale de notifications, voir `docs/checkpoints/CHECKPOINT_2026-08-17_RESILIENCE_BOOTLOOP_ET_MAJ_RESSOURCES_WEB.md`) et préparation de l'arène mbedTLS (voir feature suivante sur la branche `agent/ota-3.1-stage-inactive`)

## Constat de départ

Le module tourne avec ~320 Ko de RAM interne, sans PSRAM (`psramSize:0` confirmé sur matériel). Le tas est le seul espace disponible pour toute allocation dynamique, partagé sans cloisonnement par défaut entre l'affichage, le réseau, le Web et les notifications. Il n'existe pas de mécanisme de compaction en C/C++ (voir §"Pourquoi pas un ramasse-miettes") : un total libre confortable ne garantit jamais qu'un bloc contigu suffisant existe au moment voulu.

Le seuil qui compte presque toujours est le **plus gros bloc libre contigu**, pas le total — c'est explicitement le sujet des gardes déjà en place (§ Garde-fous existants).

## A. Consommateurs permanents (jamais libérés)

| Poste | Taille | Fichier |
|---|---|---|
| Sprites `_sprTime` + `_sprSignal` | ~5 Ko (110×20 + 20×16, 2 o/px) | `DisplayManager.cpp:289-290` |
| Journal `EventLog` | ~5,2 Ko (60 × 88 o) — tableau statique en `.bss`, pas le tas | `EventLog.h:13-20` |
| Blob de configuration `ConfigManager` | ~4,9 Ko, NVS schéma 2 | `ConfigManager.cpp` |
| Pile de la tâche `notify-supervisor` | 8 Ko, allouée une fois au démarrage, jamais rendue | `NotificationManager.cpp:38` |

## B. Consommateurs dynamiques récurrents (churn à chaque cycle)

| Poste | Taille | Fréquence | Fichier |
|---|---|---|---|
| **Pile de la tâche `notify-sender`** | **4 Ko** | **à chaque notification envoyée** | `NotificationManager.cpp:421,442` |
| Sprites `_sprPlan` + `_sprBtn0` | ~92,3 Ko (56,25 + 36,1 Ko) | à chaque veille/réveil écran | `DisplayManager.h:113-150` |
| `JsonDocument` (ArduinoJson 7) | variable, non borné a priori | à chaque réponse `/api/*` (22 occurrences) | `WebManager.cpp` et autres |
| Réponse météo | ~17 Ko | toutes les 30 min | `WeatherManager.cpp` |

**Point notable** : `notify-sender` n'est pas une tâche permanente réutilisée — `startSender()` appelle `xTaskCreatePinnedToCore()` à chaque envoi, et la tâche se termine par `vTaskDelete(nullptr)`. Chaque notification (zone, incident, mise à jour firmware ou ressources Web) provoque donc un cycle alloc/free de 4 Ko sur le tas général, exactement le type de churn qui fragmente avec le temps. Non corrigé à ce jour — identifié comme piste pour une évolution future, hors périmètre de l'arène mbedTLS.

## C. Pics ponctuels — le sujet de la fragmentation critique

| Poste | Taille | Contexte |
|---|---|---|
| **Tampons I/O mbedTLS** | **16 Ko** (`CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN`, vérifié dans le sdkconfig du framework `espressif32 @ 6.13.0`) | chaque connexion TLS réserve ce tampon plein, même pour un manifeste de 868 octets |
| Contexte SSL, certificats DigiCert, calcul d'échange de clé | non mesuré précisément, s'ajoute au-dessus | idem |

`WiFiClientSecure::setBufferSizes()` n'existe pas sur ce cœur Arduino-ESP32 (2.0.17) — la macro est redéfinie vers `setTimeout()` par nécessité de compilation (`NotificationManager.h:8-14`), donc **impossible de réduire ces tampons par l'API publique**. Toute connexion `WiFiClientSecure` du firmware (OTA, vérification de version, ressources Web) paie ce coût plein.

Ceci explique a posteriori les mesures empiriques du 16 août 2026 (voir `DisplayManager.h:94-99`) : libérer un seul sprite (~37 Ko) avait amélioré l'erreur mbedTLS sans suffire ; libérer les deux (~95 Ko) a fonctionné. 16 Ko de tampon de base plus le contexte SSL/certificats situe le besoin réel quelque part entre les deux — cohérent avec l'observation.

## D. Garde-fous déjà en place

| Garde | Seuil | Fichier |
|---|---|---|
| Fetch météo | libre < 45 000 o ou plus gros bloc < 25 000 o → report 2 min | `WeatherManager.h:42-44` |
| Requêtes SD simultanées | 8 max, ou libre < 12 000 o → 503 + Retry-After | `SdStaticHandler.cpp:26-27` |
| Pages HTML embarquées simultanées | 1 max, ou libre < 12 000 o → 503 + Retry-After | `WebManager.cpp:54-55` |
| Mémoire basse (global, avec hystérésis) | libre < 10 000 o (déclenche), > 16 000 o (récupère) | `SystemDiagnostics.h:53-55` |

Tous ces gardes suivent le même principe : **refuser tôt et proprement plutôt que tenter puis s'effondrer**. C'est le sens de l'invariant applicatif déjà en place dans le projet.

## E. Trou identifié (hors périmètre de cette itération)

**`NotificationManager` n'a aucun garde mémoire avant l'envoi.** Contrairement à la météo et à la SD, il journalise le tas disponible à chaque étape (`heap=...`, `maxblock=...`) mais ne reporte ni ne refuse jamais un envoi faute de mémoire. C'est cohérent avec l'incident du 17 août (rafale de notifications, aucun frein, collision avec la recréation des sprites au réveil de l'écran). Non corrigé ici — l'arène mbedTLS ne couvre pas ce cas puisque les notifications passent en HTTP simple (port 80), pas en TLS.

## Pourquoi pas un ramasse-miettes

Un ramasse-miettes déplaçant (Java, JS) regroupe l'espace libre en repositionnant les objets encore utilisés, et met à jour toutes les références vers eux. En C/C++, un pointeur est une adresse brute : le déplacer sans réécrire tous les pointeurs qui le référencent (ici, par exemple, `TFT_eSprite` garde un pointeur direct vers son tampon) est indéfini et corromprait la mémoire. Une compaction sûre exigerait de faire transiter tout le firmware par un système de références indirectes (« poignées »), hors de proportion pour ce projet.

## Principe retenu pour la suite

Ne pas cloisonner tous les consommateurs par défaut — la mémoire totale (320 Ko) est trop restreinte pour que chaque réserve dédiée reste rentable. Cibler uniquement les collisions réelles entre gros consommateurs imprévisibles, identifiées par une cartographie comme celle-ci plutôt que par supposition. La première application de ce principe est l'arène mbedTLS dédiée (voir le commit associé), qui isole les tampons TLS du tas général sans toucher au mode maintenance (où l'allocateur standard reste utilisé, la mémoire y étant abondante).

## Écarts connus de cette documentation

Ce document n'a pas encore été relié à `35_CODE_TRACEABILITY_REGISTER.md` ni évalué dans `33_DOCUMENT_MATURITY_MATRIX.md` selon leur méthodologie propre — laissé explicitement en écart plutôt que complété par approximation.

## Tentative du 18 août 2026 — arène mbedTLS statique, résultat négatif

Implémentation testée sur matériel réel (COM7), puis abandonnée : une arène
statique et permanente (`.bss`, réservée au démarrage) n'a **aucune taille
qui fonctionne** sur cet ESP32 sans PSRAM.

| Taille | Résultat mesuré |
|---|---|
| 64 Ko | échec de lien : dépassement du segment DRAM de 15 712 octets (mesure du linker, pas une estimation) |
| 40 Ko | compile et démarre, mais le TLS échoue quand même : `[ssl_client.cpp] SSL - Memory allocation failed` (-32512), pic mesuré 25 720/40 960 juste avant l'échec |
| 48 Ko | **le pilote WiFi lui-même échoue à s'initialiser au démarrage** : `wifi:wifi nvs cfg alloc out of memory` — confirmé sur 2 tentatives consécutives, pas transitoire (à distinguer d'un échec `esp_wifi_init` isolé et transitoire, déjà observé par ailleurs sans arène, qui se résout en 2-3 tentatives) |

La marge DRAM disponible sur cette plateforme est trop étroite pour loger
une réserve permanente : le pilote WiFi en a besoin au démarrage, avant
même qu'une connexion TLS soit tentée. Le code (`MbedtlsArena.h/.cpp`) a
été retiré après ces tests ; le firmware restauré à l'état sans arène.

**Piste corrigée, non implémentée** : une arène **dynamique**, allouée
depuis le tas général uniquement au moment de l'appel TLS (après que
`DisplayManager::suspendForMemoryRelief()` ait déjà libéré ~92 Ko), et
rendue immédiatement après. Zéro coût permanent — aucun risque pour le
démarrage WiFi — tout en gardant l'isolement pendant l'usage (mbedTLS ne
fragmente pas le tas général avec ses nombreuses petites allocations
pendant la poignée de main). Même filet de sécurité qu'aujourd'hui en cas
d'échec de la seule grosse allocation initiale.

**Décision (18 août 2026)** : chantier mis en attente d'une carte avec
PSRAM, qui change la donne (budget mémoire nettement moins contraint,
plus besoin d'arbitrer DRAM statique contre démarrage WiFi). Reprendre
ici — piste dynamique ci-dessus — si une correction est nécessaire avant
l'arrivée de cette carte.

## Reprise le 27 août 2026 — carte PSRAM en main, analyse sans matériel

La carte ESP32-S3 JC4827W543C_I (8 Mo de PSRAM) est arrivée
(`docs/architecture/HW_JC4827W543_PORT_IMPACT.md`). Analyse du
`sdkconfig.h` réellement livré avec ce framework pour la configuration
mémoire retenue (`qio_opi`,
`framework-arduinoespressif32/tools/sdk/esp32s3/qio_opi/include/sdkconfig.h`),
**pas une supposition** :

```
CONFIG_SPIRAM_USE_MALLOC 1
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL 4096
CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN 16384
CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC 1
```

**Le tampon mbedTLS (16 Ko) reste force en RAM interne** —
`CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=1` est fige dans les bibliotheques
precompilees de ce framework, non modifiable par un simple `build_flags`
PlatformIO. La PSRAM n'absorbe donc **pas directement** le tampon
mbedTLS lui-meme, contrairement a ce qu'on aurait pu esperer un peu vite.

**Mais le concurrent historique a disparu par un autre mecanisme.** Sur
la carte actuelle, ce qui entrait en collision avec mbedTLS en RAM
interne etroite, c'etaient les sprites d'ecran (~92 Ko, §A/§C ci-dessus)
— d'ou `suspendForMemoryRelief()` avant toute connexion HTTPS. Sur la
carte S3, l'adaptateur d'affichage
(`lib/tft_espi_compat_s3/`, commit `feat(hw): socle de compilation
ESP32-S3 + adaptateur TFT_eSPI`) alloue les sprites/canvas via
`Arduino_Canvas`, qui vit **en PSRAM** (confirme par le test de
performance §4 de `HW_JC4827W543_PORT_IMPACT.md` — le framebuffer
mesure lui-meme depuis la PSRAM). Ces ~92 Ko n'occupent donc plus jamais
la RAM interne sur cette carte : le concurrent qui provoquait la
collision a demenage, pas mbedTLS. `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL
4096` confirme par ailleurs que tout le reste des allocations projet
au-dessus de 4 Ko (JsonDocument, reponses meteo, etc. - §B) peut migrer
vers la PSRAM sans configuration supplementaire.

**Consequence pratique** : l'arene mbedTLS (statique ou dynamique) n'est
vraisemblablement **plus necessaire** sur cette carte - la RAM interne
devrait rester largement disponible pour les 16 Ko de mbedTLS sans plus
jamais cotoyer un gros consommateur permanent. **Non confirme sur
materiel** : redige sans acces physique au module (voir
`HW_JC4827W543_PORT_IMPACT.md`, phase A). A verifier des que possible :
declencher une vraie poignee de main HTTPS (verification de version,
mise a jour des ressources Web, ou CloudSync si repasse en HTTPS) sur la
carte S3 en surveillant `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`
avant/pendant/apres — sans avoir besoin d'appeler
`suspendForMemoryRelief()` au prealable. Si la poignee de main echoue
quand meme, la piste d'arene dynamique documentee plus haut reste
disponible, adaptee a ce nouveau budget.

## Historique

### 0.3 — 27 août 2026

Reprise du chantier : carte PSRAM en main (ESP32-S3 JC4827W543C_I).
Analyse du sdkconfig reel du framework - mbedTLS reste force en RAM
interne (`CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=1`), mais le concurrent
historique (sprites d'ecran) a demenage en PSRAM via l'adaptateur
d'affichage, ce qui devrait suffire sans arene dediee. Non confirme sur
materiel (pas d'acces physique au module a la redaction).

### 0.2 — 18 août 2026

Tentative d'arène mbedTLS statique, résultat négatif documenté ci-dessus.
Mise en attente du chantier.

### 0.1 — 18 août 2026

Cartographie initiale, en préparation de l'arène mbedTLS.
