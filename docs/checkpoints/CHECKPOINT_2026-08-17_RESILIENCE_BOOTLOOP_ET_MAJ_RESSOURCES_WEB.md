# Checkpoint AquaLook — résilience, boucles de redémarrages et mise à jour des ressources Web

## Référence

- Dépôt : `cnuma/AquaLook`
- Branche : `agent/ota-3.1-stage-inactive`
- Commit de tête à la clôture : `723e4d1565bf7fa7f2eeeae3308b4b6c25f95dfc`
- `VERSION` du dépôt : `5.9.7`
- Releases publiées ce jour : `v5.9.6`, `v5.9.7`
- Date : 2026-08-17
- Documents associés : `ROADMAP.md` (sections « Mise à jour distante des ressources Web » et « Pages HTML servies tronquées »)

## Source de vérité

Reprendre exclusivement depuis la branche Git ci-dessus, et lire dans l'ordre :

1. `AGENTS.md`
2. `ROADMAP.md` — en particulier les étapes 5, 6 et 7 des ressources Web
3. le présent checkpoint
4. `platformio.ini`
5. `src/BootLoopGuard.h` — l'en-tête contient le raisonnement complet du mode dégradé
6. `src/UpdateCheckScheduler.h` — idem pour les conditions de déclenchement

Ne pas repartir d'un extrait de conversation ni d'une archive antérieure.

---

## État du matériel à la clôture

| Élément | Valeur |
|---|---|
| Firmware flashé | 5.9.7, build 917, `sha=d147802` puis reflashé avec `723e4d1` |
| Adresse IP | `192.168.1.198` |
| Port série | `COM3` (vérifier avec `pio device list`, il a déjà changé) |
| Ressources Web sur SD | version `5.9.6` — **en retard sur le firmware**, voir « à faire » |
| Vérification auto | activée, 03:30, tous les jours, `lastCheckEpochDay=20682` |
| Mode dégradé | inactif, `suspectBoots=1` |
| Tas libre au repos | ~160 Ko écran en veille, ~28 Ko écran allumé |

Le module tourne, ne boucle plus, et a été laissé armé pour la vérification
automatique de 03:30.

---

## Ce qui a été livré et validé sur matériel

### Mise à jour des ressources Web — étapes 5, 6 et 7 (canal firmware)

- Déploiement transactionnel par répertoire de transit `/www.new`, bascule
  fichier par fichier, rattrapage au montage. On n'écrit **jamais** directement
  dans `/www`.
- Déclenchement par l'utilisateur depuis `/ota` (section « Ressources Web »),
  exécuté en **mode maintenance** (~242 Ko de tas contre ~32 Ko en
  fonctionnement normal). C'est ce qui a débloqué l'étape 6 après deux échecs.
- Résultat persisté dans `MaintenanceResultStore` et affiché au retour.
- Vérification périodique configurable : `UpdateCheckScheduler`, section
  « Mises à jour » de la page de configuration, route `POST /api/updateCheck`,
  namespace NVS `aq_upd_chk`.

**Validation de bout en bout, release v5.9.6** : tag poussé, CI publie, bouton
pressé, 10 fichiers vérifiés empreinte par empreinte, bascule de 11 fichiers,
version des pages passée à 5.9.6. Le SHA-256 de quatre fichiers **servis par le
module** a été recalculé et comparé au manifeste publié — 4 conformes, 0 écart.

### Trois défauts de diffusion des pages HTML

1. `beginResponse(code, type, const char*)` recopiait la page entière dans une
   `String` du tas puis appelait `substring()` à chaque acquittement TCP. Sous
   pression mémoire, l'échec d'allocation n'étant vérifié nulle part, le module
   envoyait **neuf kilo-octets de son propre tas** à la place de la page.
   Corrigé par la surcharge `(const uint8_t*, size_t)`, qui diffuse depuis la
   flash sans copie.
2. `AsyncAbstractResponse::_ack()` ignore la valeur rendue par `write()` et
   avance son curseur de la taille demandée : les octets refusés sont perdus,
   sous un `Content-Length` complet. **Défaut de bibliothèque non corrigé** —
   une tentative de correction du contrôle de flux a été écrite, testée, puis
   abandonnée le même jour parce qu'elle dégradait le comportement.
3. Parade applicative : `WebManager::sendEmbeddedPage()` ne sert qu'**une page
   embarquée à la fois**, refus explicite au-delà (503 + `Retry-After`), avec un
   délai anti-blocage de dix secondes.

Mesures, écran allumé, page de 12 503 octets : avant, 8 chargements simultanés
donnaient 8 pages défectueuses ; après, 5 rafales de 8 donnent 10 pages servies
toutes intactes, 30 refus explicites, **0 corrompue**. Usage nominal : 40/40.

### Heure inconnue — arrosage suspendu en silence

`src/main.cpp` n'appelle `ScheduleManager::update()` que si le NTP est
synchronisé. Sans heure, aucun arrosage programmé ne démarre — et rien ne le
disait. Nouveau `FaultId::TIME_UNSYNCED`, levé après cinq minutes, avec rappel
toutes les trente minutes.

L'horloge **survit à un redémarrage logiciel** (vérifié : après `esp_restart()`,
la première ligne de journal est déjà horodatée). Le scénario à risque est donc
la coupure d'alimentation, pas le boot de maintenance.

Validé en provoquant la panne : serveur NTP pointé sur `192.0.2.1` (RFC 5737,
non routable) puis reset matériel. Alerte à 5 min 03 s, extinction et reprise
annoncée à la resynchronisation. Configuration NTP restaurée à l'identique.

### Deux boucles de redémarrages, et le garde-fou qui les arrête

**Boucle 1 — notification.** Notification envoyée avec succès (`http=200`), puis
`Stack canary watchpoint triggered (notify-supervis)` en écrivant l'accusé en
NVS. Redémarrage avant persistance → le module se croyait encore en attente →
renvoi → replantage. Cause mesurée : `MaintenanceResult` pèse ~750 octets et
`markUpdateNotificationDelivered()` en empilait plus de 2 Ko. Corrigé aux deux
niveaux — blocs déplacés sur le tas (précédent : `ConfigManager::save()`), pile
du superviseur portée de 4 Ko à 8 Ko.

*Prouvé* : release v5.9.7 publiée, vérification déclenchée, chemin exact rejoué —
`accuse update ecrit, marge de pile=2580 octets`, une seule notification, aucun
plantage. Le chemin consomme 5 612 octets : les deux correctifs étaient
nécessaires.

**Boucle 2 — météo.** `Meteo: HTTP 200 annonce=16739` suivi de `abort()`. Trace
décodée : `AsyncServer::_accepted` → `operator new` → `__cxa_throw` →
`std::terminate`. La réponse météo épuisait le tas, la connexion HTTP suivante ne
pouvait plus allouer, `bad_alloc` non rattrapée abattait le système. C'est très
probablement l'explication du symptôme signalé de longue date, « le module
redémarre plusieurs fois puis se stabilise ». Corrigé par un contrôle mémoire
**avant** le lancement du fetch, report de deux minutes.

**Garde-fou générique — `BootLoopGuard`.** Compte les démarrages non suivis de
trois minutes de fonctionnement stable ; au-delà de quatre, mode dégradé.
Survivent : arrosage, horloge, relais, écran, serveur Web. Suspendus : météo,
vérification de mise à jour, notifications réseau.

Deux décisions non évidentes, documentées dans `src/BootLoopGuard.h` :

- Les notifications sont suspendues **elles aussi**. Si la boucle vient de
  l'envoi d'une notification — cas réellement survenu — en émettre une pour
  signaler le mode dégradé relancerait ce que ce mode existe pour arrêter.
- **Pas de sortie automatique.** Survivre une heure avec la météo coupée ne
  prouve rien sur la météo. Sortie sur action explicite via
  `POST /api/bootguard/clear` ou le bandeau de la page d'accueil.

Anti-faux-positif : les huit `ESP.restart()` du projet passent par
`BootLoopGuard::restartDeliberately()`, qui pose une marque en NVS.
**L'absence de marque vaut « non voulu »** — un plantage ne peut pas légitimer
son propre redémarrage, et un `ESP.restart()` ajouté demain sans passer par ce
point sera compté comme suspect, jamais l'inverse.

Validé par resets matériels successifs : comptage 2/4 puis 3/4 sans réaction,
mode dégradé enclenché, les trois sous-systèmes vérifiés à zéro, l'essentiel
debout, **0 plantage et un seul démarrage ensuite**. Sortie testée.

---

## Limites connues, non corrigées

- **Contrainte mémoire de fond.** Au démarrage, après allocation des tampons
  d'affichage, le tas descend à **4 932 octets libres et 1 268 octets de plus
  gros bloc** avant de remonter. Les deux tampons `TFT_eSprite` permanents
  (~95 Ko) en sont la cause. Les correctifs du jour suppriment des
  déclencheurs, pas la cause. **La carte à PSRAM est la vraie réponse.**
- **Défaut ESPAsyncWebServer non corrigé** (perte d'octets sur écriture
  partielle). Contourné, pas résolu. Ne pas retenter de patch sans avoir compris
  la machine à états complète (crédits en vol, `_cache`, `_sentLength`).
- **`wip/step6-webassets-check`** : branche parquée, plantage `loopTask`
  toujours inexpliqué. Hypothèse crédible depuis : mbedTLS + ArduinoJson
  dépassent 16 Ko de pile, comme la tâche de maintenance qui a dû passer à 32 Ko.
- **Routes `/api/debug/*`** toujours ouvertes, à sécuriser ou retirer avant
  production.
- **Table de partitions** non déployable par OTA : à flasher par USB avant toute
  mise en service.
- Bogue cosmétique différé : sélecteurs de couleur des zones 3 et 4 affichés
  alors que deux zones seulement sont configurées.

---

## Procédure de reprise

```powershell
Set-Location "C:\Users\emman\OneDrive\Documents\VsCode_travail\arrosage"

git fetch origin
git switch agent/ota-3.1-stage-inactive
git pull --ff-only origin agent/ota-3.1-stage-inactive

git log --oneline -12
Get-Content VERSION
Get-Content docs\checkpoints\CHECKPOINT_2026-08-17_RESILIENCE_BOOTLOOP_ET_MAJ_RESSOURCES_WEB.md

pio run -e ProgrammeArrosage
```

Avant tout upload, confirmer le port réel :

```powershell
pio device list
```

Workflow série utilisé toute la journée (l'ouverture du moniteur provoque un
reset matériel par RTS, ce qui efface l'horloge interne) :

```powershell
pio run -e ProgrammeArrosage -t upload --upload-port COM3
pio device monitor --port COM3 --filter log2file --filter esp32_exception_decoder --filter time
```

Les journaux horodatés atterrissent dans `logs/device-monitor-*.log`. Décodage
d'une trace :

```powershell
& "C:/Users/emman/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line.exe" `
  -pfiaC -e .pio\build\ProgrammeArrosage\firmware.elf 0x401a021b
```

---

## À faire en priorité à la reprise

1. **Lire le journal de la nuit du 17 au 18 août** — voir la section
   « Campagne de la nuit » ci-dessous, qui contient la procédure complète et le
   scénario attendu.
2. **Déployer les ressources Web en 5.9.7.** La carte est restée en 5.9.6 : le
   bandeau de mode dégradé et le bouton de réactivation ajoutés en fin de
   journée ne sont **pas encore servis par le module**. Passer par le bouton de
   `/ota` après publication, ou publier une v5.9.8.
3. **Canal ressources Web dans la vérification périodique** — aujourd'hui seul
   le firmware est vérifié. Et notification ntfy quand une mise à jour de
   ressources Web est disponible.
4. Reste de la spécification UX d'origine : LED violette et icône LCD pour une
   mise à jour en attente, pastille dans la barre du haut renvoyant vers `/ota`.

## Campagne de la nuit du 17 au 18 août 2026

La vérification automatique de 03:30 n'a **jamais** été déclenchée en conditions
réelles à la clôture. Sa logique de décision n'est validée que par le
raisonnement. Le réseau de l'utilisateur redémarre à 03:29, une minute avant
l'échéance : la collision est fortuite mais exerce exactement le chemin de report
qu'on ne savait pas tester autrement.

### Lancer la capture

Le journal en RAM du module est effacé par le redémarrage de maintenance. **La
capture série est le seul témoin** des lignes antérieures au redémarrage.

```powershell
pio device list        # confirmer le port : platformio.ini declare COM9,
                       # la machine de developpement utilisait COM3

pio device monitor --port <PORT> --baud 115200 `
  --filter log2file --filter esp32_exception_decoder --filter time
```

Le fichier apparaît sous `logs\device-monitor-<AAMMJJ-HHMMSS>.log`.

### Vérifier que la capture écrit réellement

À faire systématiquement, dans une seconde fenêtre. Le 17 août, les processus du
moniteur tournaient alors que le fichier n'était plus alimenté depuis une demi-
heure : **« le moniteur est lancé » ne prouve rien**. C'est le même principe que
partout ailleurs dans ce projet — vérifier le résultat, pas l'état apparent.

```powershell
curl.exe -s -X POST -H "Content-Type: application/json" `
  -d '{\"enabled\":true,\"hour\":3,\"minute\":30,\"intervalDays\":1}' `
  http://192.168.1.198/api/updateCheck

Get-Content (Get-ChildItem logs\device-monitor-*.log |
  Sort-Object LastWriteTime -Desc | Select -First 1) -Tail 3
```

La ligne `Verif MAJ: reglage change` doit apparaître **horodatée à l'instant**.
Sinon la capture est morte : tout fermer et relancer.

### Deux précautions d'horaire

L'ouverture du moniteur provoque un reset matériel par RTS, et débrancher le
module pour le déplacer est une coupure d'alimentation. Dans les deux cas :

- **l'horloge interne est perdue** et doit être resynchronisée par NTP. Si le
  module n'a pas l'heure à 03:30, la vérification est reportée — correctement,
  mais la nuit est perdue. Contrôler `"synced": true` sur `/api/status` avant de
  laisser tourner ;
- le compteur anti-boucle monte à 1 ou 2 sur 4, sans conséquence : il repart de
  zéro après trois minutes de fonctionnement stable.

Lancer la capture **au moins quinze minutes avant l'échéance**.

### Dépouillement le lendemain

```powershell
$L = Get-ChildItem logs\device-monitor-*.log |
     Sort-Object LastWriteTime -Desc | Select -First 1
Select-String -Path $L -Pattern "Verif MAJ|Garde anti-boucle|CHECK_VERSION|abort|Guru"
(Select-String -Path $L -Pattern "demarrage target=").Count
```

Scénario attendu, écrit à l'avance pour permettre la comparaison :

| Vers | Ligne attendue |
|---|---|
| 03:30 | `Verif MAJ: echeance atteinte mais report — pas de connexion WiFi` |
| 03:3x | reconnexion WiFi, puis cinq minutes de stabilité exigées |
| 03:36-03:38 | `Verif MAJ: echeance 03:30 atteinte, redemarrage en mode maintenance` |
| — | `Maintenance: CHECK_VERSION success=yes installed=5.9.7 available=5.9.7` |

Trois contrôles, par ordre d'importance :

1. **exactement deux démarrages** — un en mode maintenance, un en production.
   Trois ou plus signifierait que la protection anti-boucle a eu à travailler,
   et il faudrait comprendre pourquoi ;
2. **`abort` et `Guru` à zéro** ;
3. le report journalisé **puis** le déclenchement effectif — c'est le
   comportement jamais vérifié autrement que par le raisonnement.

Aucune notification ntfy n'est attendue : le module est en 5.9.7 et la release
publiée aussi, donc il n'y a rien à signaler. C'est le résultat normal, pas un
échec.

## Principes de travail retenus ce jour

- **Un code de retour n'est pas une vérification.** Un `curl` qui rend `200` avec
  la taille annoncée a l'air d'un succès ; il a fallu comparer octet à octet
  contre une référence pour voir le trou.
- **Provoquer la panne avant de corriger, et la reprovoquer après.** Les trois
  correctifs majeurs du jour ont été validés en recréant délibérément la
  situation (NTP injoignable, version plus récente publiée, resets successifs).
- **Ne jamais laisser une ligne fausse dans un journal.** Une ligne absente coûte
  moins cher qu'une ligne qui envoie chercher au mauvais endroit.
