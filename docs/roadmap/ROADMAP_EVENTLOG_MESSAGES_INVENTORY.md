# Inventaire EventLog::log(...) restants (hors conversion deja faite)

Genere le 26 septembre 2026 en clotant le chantier de lisibilite du journal
technique (voir `ROADMAP.md`, section "Journal technique"). Sert de base au
lot suivant de conversions -- **pas un engagement a tout convertir** : sur
328 sites restants, seuls **52 sont juges "obscur"** (jargon interne, code
brut sans explication) par la meme grille que celle qui a servi a
convertir les 8 premiers messages (SHDW-*, ORCH-*, EQUIP-MODEL). Les 276
"clair" restants n'ont pas besoin du traitement code+catalogue -- les
convertir quand meme serait de la sur-ingenierie sur des messages deja
comprehensibles.

## Plan de lots pour les 52 messages "obscur"

**Lot 1 -- finir la famille shadow/orchestrateur (11 sites)**
`EquipmentExecutionShadowRuntime.cpp` (les 9 restants : lignes 54, 92, 130,
263, 277, 283, 288, 294, 299 -- meme fichier, deja 3/12 converti) +
`main.cpp:290` (Shadow pump scenario) + `main.cpp:654` (Equipment config
runtime shadow force). Continuation directe et sans risque du travail deja
fait ce jour -- meme moteur, meme regle (`authority=no`/`passive=yes` =>
INFO), memes codes a etendre (`SHDW-PUMP`, nouveau `SHDW-ACTIVITY` pour
les lignes `[Activity#/Exec#]`).

**Lot 2 -- parite V4 / chemin d'execution equipement (14 sites)**
`main.cpp` (394 PARITE, 407 PARITE-KO, 421 echec fallback, 558 status
runtime, 757 plan V4) + `EquipmentManager.cpp` (277, 287, 299, 310,
`dry_run=yes`) + `EquipmentOutputRuntimeAdapter.cpp` (120, 169, 195,
`path=`) + `EquipmentRuntimeConfigStore.cpp:132` +
`V4PilotRuntime.cpp:119`.
**Attention, nuance importante** : contrairement au lot 1, ce ne sont PAS
des messages d'un moteur passif -- la parite V4 et le chemin
`EquipmentOutputRuntimeAdapter` pilotent les VRAIES sorties relais. Le
niveau (WARN/ERROR) est probablement deja justifie pour la plupart ; le
travail ici est seulement de traduire le jargon (`PARITE-KO`, `dry_run`,
`path=failed`), pas de recalibrer la gravite comme pour le lot 1. Ne pas
appliquer machinalement la regle "INFO si passif" -- elle ne s'applique
pas ici.

**Lot 3 -- codes de diagnostic bas niveau (15 sites)**
`OtaTlsProbe.cpp` (les 8 lignes du fichier, deja prefixees `OTA-1.1:` de
facon ad hoc -- le plus proche de la convention actuelle, juste a
formaliser en `[OTA-TLS-PROBE]` + catalogue) + `ScriptHostRuntime.cpp`
(90, 103, 114, 120, codes numeriques d'action/message/alerte de script) +
`IoExpanderManager.cpp:93` (registre `iodir` brut) +
`RuntimeProfiler.cpp:78` (`wallUs`/`schedSuspect` internes) +
`OtaStageUpdate.cpp:226`.

**Lot 4 -- divers isoles (13 sites)**
`WiFiManager.cpp` (233, 271, 343, codes `wl_status` bruts) +
`MaintenanceBoot.cpp` (89, 520, 583, 599) + `StorageManager.cpp:206`
(code `0x1E` brut, deja rencontre cette session -- voir
`checkpoint-2026-09-26-perf-web-part3`) + `NTPManager.cpp:70`
(`configDirty` interne) + `IncidentManager.cpp:43`
(`notifications=0x%02X` brut) + `WebManager.cpp:1299` ("ancre intervalle"
-- jargon) + `WeatherManager.cpp:366` (`annonce=`/`lecture=stream`).

Chaque lot est independant et peut se traiter dans une session separee,
avec verification materielle comme pour les 8 premiers messages (voir
`checkpoint-2026-09-26-perf-web-part3` pour la methode : build, flash,
verification API/log en direct, commit apres validation).

---


Perimetre : tous les `src/*.cpp` et `src/*.h`, sans `.pio` ni `src-V0` (aucun des
deux trouve dans le depot). Sont EXCLUS les 8 sites deja convertis au nouveau
style (7 codes : SHDW-ENGINE, SHDW-PUMP x2, ORCH-PREVIEW, ORCH-HANDOFF,
EQUIP-MODEL, SHDW-PUMP-CFG, ORCH-STATUS), situes dans
`src/EquipmentExecutionShadowRuntime.cpp` (lignes 33, 177, 205) et
`src/main.cpp` (lignes 318, 337, 640, 667, 684).

Note : `src/FaultManager.h` contient une occurrence textuelle de
"EventLog::log(LOG_ERROR, ...)" mais uniquement dans un commentaire (ligne 59,
qui explique le champ `message` de FaultManager) — ce n'est PAS un site
d'appel reel, donc absent du tableau ci-dessous et non compte dans le total.

Total reel de sites EventLog::log(...) dans src/ : 337 (dont 8 deja convertis
+ 328 restants + 1 faux positif dans un commentaire = 337).

## Sommaire (tri decroissant par nombre de sites restants)

| Fichier | Sites restants |
|---|---|
| src/ConfigManager.cpp | 30 |
| src/StorageManager.cpp | 27 |
| src/MaintenanceBoot.cpp | 26 |
| src/main.cpp | 21 |
| src/WiFiManager.cpp | 19 |
| src/WebManager.cpp | 17 |
| src/RelayTopologyStore.cpp | 16 |
| src/WebManager.h | 13 |
| src/BootLoopGuard.cpp | 12 |
| src/SystemDiagnostics.cpp | 10 |
| src/EquipmentExecutionShadowRuntime.cpp | 9 |
| src/IncidentManager.cpp | 9 |
| src/NTPManager.cpp | 9 |
| src/EquipmentRuntimeConfigStore.cpp | 8 |
| src/OtaTlsProbe.cpp | 8 |
| src/WeatherManager.cpp | 8 |
| src/RelaisManager.cpp | 7 |
| src/ScriptStore.cpp | 7 |
| src/UpdateCheckScheduler.cpp | 7 |
| src/ApiAuth.cpp | 6 |
| src/WebAssetsUpdater.cpp | 6 |
| src/IoExpanderConfig.cpp | 5 |
| src/OtaBootGuard.cpp | 5 |
| src/PausedWateringStore.cpp | 5 |
| src/ScriptHostRuntime.cpp | 5 |
| src/ScriptRunner.cpp | 5 |
| src/EquipmentManager.cpp | 4 |
| src/EquipmentOutputRuntimeAdapter.cpp | 3 |
| src/InputSampler.cpp | 3 |
| src/IoExpanderManager.cpp | 3 |
| src/ScheduleManager.cpp | 3 |
| src/V4PilotRuntime.cpp | 3 |
| src/MaintenanceSetupWrapper.cpp | 2 |
| src/V4RelayPhysicalBackend.cpp | 2 |
| src/DisplayManager.h | 1 |
| src/OtaDownloadTest.cpp | 1 |
| src/OtaStageUpdate.cpp | 1 |
| src/RuntimeProfiler.cpp | 1 |
| src/SdStaticHandler.cpp | 1 |
| **TOTAL** | **328** |

---

## src/ConfigManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 228 | LOG_ERROR | Config: mount LittleFS ECHEC — ressources Web indisponibles | clair |
| 230 | LOG_INFO | Config: LittleFS monte (lecture ressources) | clair |
| 258 | LOG_ERROR | Config: NVS invalide conserve sans ecrasement; defauts RAM actifs | clair |
| 264 | LOG_WARN | Config: NVS/JSON absents — valeurs par defaut | clair |
| 272 | LOG_ERROR | Config: ouverture NVS lecture impossible | clair |
| 291 | LOG_WARN | Config: taille NVS invalide (%u/%u) | clair |
| 299 | LOG_ERROR | Config: allocation lecture NVS impossible | clair |
| 319 | LOG_ERROR | Config: bloc NVS schema 4 invalide | clair |
| 345 | LOG_INFO | Config: migration NVS schema 4 -> 5, identifiants de zone attribues | clair |
| 364 | LOG_ERROR | Config: bloc NVS schema 3 invalide | clair |
| 399 | migrated ? LOG_INFO : LOG_ERROR | "Config: migration NVS schema 3 -> 4 reussie" / "Config: migration NVS schema 3 -> 4 non confirmee" | clair |
| 424 | LOG_ERROR | Config: bloc NVS schema 2 invalide | clair |
| 459 | migrated ? LOG_INFO : LOG_ERROR | "Config: migration NVS schema 2 -> 3 reussie, meteo=%s" / "Config: migration NVS schema 2 -> 3 non confirmee, meteo=%s" | clair |
| 475 | LOG_ERROR | Config: bloc NVS schema 1 invalide | clair |
| 504 | migrated ? LOG_INFO : LOG_ERROR | "Config: migration NVS schema 1 -> 2 reussie" / "Config: migration NVS schema 1 -> 2 non confirmee" | clair |
| 524 | LOG_ERROR | Config: bloc NVS invalide (entete/CRC) | clair |
| 574 | LOG_INFO | Config: couleur zone active passee au bleu (defaut historique) | clair |
| 588 | LOG_INFO | Config: charge depuis NVS (schema %u) | clair |
| 637 | LOG_INFO | Config: valeurs par defaut appliquees | clair |
| 677 | LOG_ERROR | Config: allocation sauvegarde NVS impossible | clair |
| 705 | LOG_ERROR | Config: ouverture NVS ecriture impossible | clair |
| 716 | LOG_ERROR | Config: ecriture NVS incomplete (%u/%u) | clair |
| 723 | LOG_INFO | Config: sauvegarde NVS OK (%u octets, schema %u) | clair |
| 771 | LOG_ERROR | Config: ouverture NVS reset impossible | clair |
| 776 | ok ? LOG_INFO : LOG_ERROR | "Config: NVS efface" / "Config: echec effacement NVS" | clair |
| 1048 | LOG_WARN | WebAssets: URL enregistree non HTTPS, defaut retabli | clair |
| 1065 | LOG_INFO | WebAssets: source = %s | clair |
| 1081 | LOG_ERROR | WebAssets: URL refusee, HTTPS obligatoire | clair |
| 1085 | LOG_ERROR | WebAssets: URL trop longue | clair |
| 1101 | LOG_INFO | WebAssets: source de mise a jour modifiee | clair |

## src/StorageManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 96 | LOG_WARN | Stockage: montage SD initial echoue raison=%s | clair |
| 108 | LOG_WARN | Stockage: fermeture ignoree pendant une tentative de remontage | clair |
| 191 | LOG_WARN | Stockage: /www/index.html absent, carte saine (ressources Web incompletes) | clair |
| 206 | LOG_WARN | Stockage: erreur E/S SD code=0x%02X confirmation=%u/%u | obscur |
| 388 | created ? LOG_INFO : LOG_ERROR | Deploiement: preparation du transit %s %s | clair |
| 418 | LOG_ERROR | Deploiement: transit absent, bascule annulee | clair |
| 443 | LOG_ERROR | Deploiement: deplacement echoue pour %s | clair |
| 450 | allMoved ? LOG_INFO : LOG_ERROR | Deploiement: bascule %s, %u fichier(s) deplace(s) | clair |
| 469 | LOG_WARN | Deploiement: transit restant, reprise de la bascule | clair |
| 479 | done ? LOG_WARN : LOG_ERROR | Deploiement: ancienne version %s apres interruption | clair |
| 491 | LOG_INFO | Deploiement: residus de bascule nettoyes | clair |
| 505 | LOG_WARN | Stockage: auto-test ecriture SD echoue (ouverture) | clair |
| 512 | LOG_WARN | Stockage: auto-test ecriture SD echoue (ecrit %ld/%u octets) | clair |
| 523 | LOG_WARN | Stockage: auto-test ecriture SD echoue (renommage) | clair |
| 539 | LOG_INFO | Stockage: auto-test ecriture/lecture SD OK | clair |
| 541 | LOG_WARN | Stockage: auto-test ecriture SD : relecture incoherente | clair |
| 710 | LOG_ERROR | Stockage: SD indisponible raison=%s chemin=%s | clair |
| 736 | LOG_ERROR | Stockage: echec apres %u essais, reprise lente toutes les 10min | clair |
| 750 | LOG_INFO | Stockage: remontage programme essai=%u/%u dans=%lus | clair |
| 764 | LOG_INFO | Stockage: prochaine tentative lente dans=10min | clair |
| 786 | LOG_INFO | Stockage: tentative lente=%lu tache=core0 | clair |
| 792 | LOG_INFO | Stockage: tentative remontage=%u/%u tache=core0 | clair |
| 850 | LOG_WARN | Stockage: tentative lente=%lu echouee raison=%s | clair |
| 860 | LOG_WARN | Stockage: remontage %u/%u echoue raison=%s | clair |
| 888 | LOG_INFO | Stockage: ressources Web SD validees dans /www | clair |
| 893 | LOG_INFO | Stockage: SD montee type=%s capacite=%llu Mo total=%llu Mo | clair |
| 902 | LOG_INFO | Stockage: SD recuperee essai=%u lentes=%lu indisponible=%lus | clair |

## src/MaintenanceBoot.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 59 | LOG_INFO | Maintenance: memory stage=%s heapFree=%lu heapMin=%lu largestBlock=%lu | clair |
| 74 | LOG_ERROR | Maintenance: WiFi impossible, SSID absent | clair |
| 89 | LOG_ERROR | Maintenance: WiFi echec ssid='%s' status=%d | obscur |
| 95 | LOG_INFO | Maintenance: WiFi connecte ip=%s rssi=%ddBm | clair |
| 139 | connected ? LOG_INFO : LOG_ERROR | Maintenance: GitHub TLS connected=%s durationMs=%lu | clair |
| 467 | LOG_ERROR | Maintenance: echec sauvegarde resultat NVS | clair |
| 500 | LOG_ERROR | Maintenance: INSTALL_UPDATE refuse, aucune partition validee | clair |
| 509 | LOG_ERROR | Maintenance: INSTALL_UPDATE echec armement de la garde de boot | clair |
| 520 | LOG_ERROR | Maintenance: INSTALL_UPDATE echec esp_ota_set_boot_partition err=%d | obscur |
| 531 | LOG_WARN | Maintenance: INSTALL_UPDATE partition activee label=%s address=0x%06lX, redemarrage vers le nouveau firmware | clair |
| 572 | LOG_WARN | Maintenance: demande detectee type=%s | clair |
| 575 | LOG_ERROR | Maintenance: impossible d'effacer la demande NVS | clair |
| 583 | LOG_WARN | Maintenance: mode minimal actif command=install_update otaWrite=no | obscur |
| 594 | LOG_WARN | Maintenance: commande refusee type=%s implementation=absente | clair |
| 599 | LOG_WARN | Maintenance: mode minimal actif command=%s otaWrite=no | obscur |
| 646 | webCheck.ok ? LOG_INFO : LOG_WARN | Maintenance: CHECK_VERSION (web-assets) ok=%s installed=%s available=%s update=%s detail=%s | clair |
| 654 | LOG_ERROR | Maintenance: echec sauvegarde resultat CHECK_VERSION | clair |
| 656 | result.success ? LOG_INFO : LOG_ERROR | Maintenance: CHECK_VERSION success=%s installed=%s available=%s update=%s detail=%s otaWrite=no | clair |
| 670 | r.ok ? LOG_INFO : LOG_ERROR | Maintenance: WEB_ASSETS_UPDATE success=%s version=%s fichiers=%u/%u detail=%s otaWrite=no | clair |
| 707 | LOG_ERROR | Maintenance: echec sauvegarde resultat WEB_ASSETS_UPDATE | clair |
| 734 | success ? LOG_INFO : LOG_ERROR | Maintenance: CLOUD_SYNC rapport=%s config=%s cmd=%s detail=%s | clair |
| 750 | LOG_ERROR | Maintenance: echec sauvegarde resultat DOWNLOAD_UPDATE_TEST | clair |
| 753 | result.success ? LOG_INFO : LOG_ERROR | Maintenance: DOWNLOAD_UPDATE_TEST success=%s bytes=%lu expected=%lu detail=%s otaWrite=no | clair |
| 763 | LOG_ERROR | Maintenance: echec sauvegarde resultat STAGE_UPDATE_TEST | clair |
| 766 | result.success ? LOG_INFO : LOG_ERROR | Maintenance: STAGE_UPDATE_TEST success=%s bytes=%lu expected=%lu detail=%s otaActivate=no | clair |
| 774 | success ? LOG_INFO : LOG_ERROR | Maintenance: resultat command=%s success=%s otaWrite=no | clair |

## src/main.cpp (hors ORCH-PREVIEW/ORCH-HANDOFF/EQUIP-MODEL/SHDW-PUMP-CFG/ORCH-STATUS deja convertis)

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 290 | LOG_INFO | Shadow pump: scenario pret equipment=%u assignment=%u board=%u channel=%u source=%s delays=%u/%u passive=yes | obscur |
| 394 | accord ? LOG_INFO : LOG_WARN | PARITE z%u %s L=%u.%u/%s V=%u.%u/%s ok=%lu ko=%lu %s | obscur |
| 407 | LOG_WARN | PARITE-KO z%u crit zone=%u une=%u etat=%u equip=%u voie=%u | obscur |
| 421 | LOG_WARN | Equipment: zone %u echec=%u, fallback adaptateur | obscur |
| 478 | LOG_WARN | Pause: zone %u abandonnee, duree d'attente inconnue | clair |
| 486 | LOG_WARN | Pause: zone %u abandonnee, suspendue depuis %lu min | clair |
| 493 | LOG_INFO | Pause: zone %u restauree, reste %us apres %lu min d'attente | clair |
| 503 | LOG_INFO | Pause: %u restauree(s), %u abandonnee(s) | clair |
| 522 | LOG_INFO | AquaLook v2.0 demarrage | clair |
| 541 | LOG_INFO | I2C: scan demarre | clair |
| 546 | LOG_INFO | I2C: peripherique trouve a 0x%02X | clair |
| 550 | found > 0 ? LOG_INFO : LOG_WARN | I2C: scan termine, %u peripherique(s) | clair |
| 558 | equipmentConfigStoreReady ? LOG_INFO : LOG_WARN | Equipment config runtime: status=%s enabled=%s mode=%s assignment=%u delays=%u/%u | obscur |
| 583 | LOG_INFO | Config: SSID='%s', mot de passe present=%s | clair |
| 607 | LOG_INFO | Relais V4: %u voie(s) configuree(s) au demarrage | clair |
| 612 | LOG_INFO | Relais V4: moteur V4 actif sur les sorties | clair |
| 619 | LOG_ERROR | Relais V4: pilote indisponible, aucune sortie pilotable | clair |
| 654 | LOG_WARN | Equipment config runtime: mode physical demande mais bloque, execution shadow forcee | obscur |
| 751 | LOG_INFO | Main: setup termine, boucle demarree | clair |
| 757 | LOG_INFO | Parite: plan V4 vs table de cablage a chaque decision | obscur |
| 759 | LOG_INFO | HW: PSRAM %u octets | clair |

## src/WiFiManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 34 | LOG_WARN | WiFi: pas de SSID, portail captif | clair |
| 37 | LOG_INFO | WiFi: SSID='%s', mot de passe present=%s | clair |
| 81 | LOG_INFO | WiFi: cible keepalive definie manuellement -> '%s' | clair |
| 164 | LOG_INFO | WiFi: AP '%s' IP=%s | clair |
| 207 | LOG_INFO | WiFi: connecte IP=%s RSSI=%ddBm, veille desactivee | clair |
| 233 | LOG_WARN | WiFi: echec #%u, %s, wl_status=%d, prochaine tentative dans %lus | obscur |
| 243 | LOG_WARN | WiFi: '%s' absent, verifier SSID ou portee | clair |
| 255 | LOG_WARN | WiFi: refus assoc: mot de passe, filtrage MAC ou AP sature ? | clair |
| 271 | LOG_WARN | WiFi: connexion perdue, wl_status=%d | obscur |
| 294 | LOG_INFO | WiFi: keepalive -> passerelle %s | clair |
| 324 | LOG_WARN | WiFi: cible keepalive '%s' non resolue | clair |
| 343 | LOG_WARN | WiFi: cible keepalive %s (%s) injoignable (%u/%u), wl_status=%d toujours 'connecte' | obscur |
| 355 | LOG_ERROR | WiFi: connexion zombie detectee (cible keepalive injoignable x%u malgre wl_status=connecte), reconnexion forcee | clair |
| 390 | LOG_ERROR | WiFi: instabilite recurrente (x%u cycles en <%lumin), signalement persistant | clair |
| 414 | LOG_ERROR | WiFi: %u echecs sur '%s', reprise espacee | clair |
| 436 | LOG_INFO | WiFi: tentative #%u sur '%s' | clair |
| 452 | LOG_INFO | WiFi: demarrage portail captif | clair |
| 474 | LOG_INFO | WiFi: portail arrete, reboot programme | clair |
| 516 | LOG_INFO | WiFi: scan reseau lance | clair |

## src/WebManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 111 | LOG_WARN | Web: compteur de pages debloque apres %lu ms sans fin d'envoi — securite anti-blocage | clair |
| 121 | LOG_WARN | Web: page refusee (libre=%lu max=%u refus=%lu) — mieux vaut recharger qu'une page incomplete | clair |
| 271 | LOG_INFO | Config: application differee (%u zones, manuel=%u min) | clair |
| 283 | LOG_INFO | Systeme: redemarrage programme apres sauvegarde | clair |
| 528 | LOG_WARN | WebAssets: mise a jour refusee, zone %u en arrosage | clair |
| 539 | LOG_WARN | WebAssets: mise a jour demandee, redemarrage en mode maintenance | clair |
| 1299 | LOG_INFO | Zone %u: ancre intervalle=%lu | obscur |
| 1314 | LOG_INFO | Zone %u: programmation intervalle supprimee | clair |
| 1431 | LOG_ERROR | WiFi: ConfigManager absent — identifiants non sauvegardes | clair |
| 1436 | LOG_INFO | WiFi: sauvegarde identifiants via NVS | clair |
| 1527 | LOG_INFO | Deploiement: fichier ecrit -> %s | clair |
| 1537 | LOG_INFO | Config: logs Timing %s | clair |
| 1583 | LOG_WARN | WebAssets: verification refusee, zone %u en arrosage | clair |
| 1656 | LOG_INFO | WebAssets: verification demarree url=%s taille=%lu | clair |
| 1956 | LOG_INFO | Redemarrage demande depuis l'interface | clair |
| 2069 | LOG_WARN | Scripts: source %u non enregistree, le programme tourne quand meme | clair |
| 2879 | LOG_INFO | IoExpander: configuration mise a jour (active=%u) | clair |

## src/RelayTopologyStore.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 175 | LOG_WARN | Topo: taille NVS %u inattendue (%u, %u ou %u), ignoree | clair |
| 187 | LOG_WARN | Topo: entete NVS schema 2 invalide, ignoree | clair |
| 193 | LOG_WARN | Topo: CRC NVS schema 2 invalide, ignoree | clair |
| 199 | LOG_WARN | Topo: schema 2 incoherent apres migration, ignoree | clair |
| 203 | LOG_INFO | Topo: migree du schema 2 vers 3 (voies en sortie, identifiants poses) | clair |
| 213 | LOG_WARN | Topo: entete NVS schema 1 invalide, ignoree | clair |
| 219 | LOG_WARN | Topo: CRC NVS schema 1 invalide, ignoree | clair |
| 225 | LOG_WARN | Topo: schema 1 incoherent apres migration, ignoree | clair |
| 229 | LOG_INFO | Topo: migree du schema 1 vers 2 (transport = I2C local) | clair |
| 239 | LOG_WARN | Topo: entete NVS invalide, ignoree | clair |
| 246 | LOG_WARN | Topo: CRC NVS invalide, ignoree | clair |
| 251 | LOG_WARN | Topo: contenu NVS incoherent, ignoree | clair |
| 261 | LOG_ERROR | Topo: refus d'enregistrer, topologie incoherente | clair |
| 276 | LOG_ERROR | Topo: ouverture NVS impossible | clair |
| 283 | ok ? LOG_INFO : LOG_ERROR | "Topo: topologie enregistree en NVS" / "Topo: echec d'ecriture NVS" | clair |
| 294 | LOG_INFO | Topo: topologie persistee effacee, retour legacy | clair |

## src/WebManager.h

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 97 | LOG_INFO | Erreurs acquittees depuis l'interface Web | clair |
| 360 | LOG_WARN | Maintenance Web: probe GitHub refuse, zone %u active | clair |
| 371 | LOG_ERROR | Maintenance Web: echec enregistrement demande NVS | clair |
| 376 | LOG_WARN | Maintenance Web: probe GitHub demande, redemarrage programme | clair |
| 392 | LOG_WARN | Maintenance Web: verification version refusee, zone %u active | clair |
| 403 | LOG_ERROR | Maintenance Web: echec enregistrement CHECK_VERSION NVS | clair |
| 408 | LOG_WARN | Maintenance Web: verification version demandee, redemarrage programme | clair |
| 423 | LOG_WARN | Maintenance Web: test telechargement refuse, zone %u active | clair |
| 441 | LOG_WARN | Maintenance Web: test telechargement demande, redemarrage programme | clair |
| 456 | LOG_WARN | Maintenance Web: staging refuse, zone %u active | clair |
| 474 | LOG_WARN | Maintenance Web: staging partition inactive demande, redemarrage programme | clair |
| 490 | LOG_WARN | Maintenance Web: installation refusee, zone %u active | clair |
| 510 | LOG_WARN | Maintenance Web: installation du firmware verifie demandee, redemarrage programme | clair |

## src/BootLoopGuard.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 52 | LOG_WARN | Garde anti-boucle: NVS indisponible, comptage inactif | clair |
| 72 | LOG_INFO | Garde anti-boucle: redemarrage volontaire pendant l'essai -- ni preuve ni echec, essai interrompu | clair |
| 89 | LOG_ERROR | Garde anti-boucle: RECHUTE pendant l'essai -- une fonction relancee (meteo, mise a jour ou notifications) a provoque ce redemarrage. Retour immediat en mode degrade, aucun nouvel essai automatique pour cet episode | clair |
| 113 | LOG_ERROR | Garde anti-boucle: MODE DEGRADE actif apres %u demarrages sans periode stable — arrosage, horloge, ecran et page Web conserves ; meteo, verification de mise a jour et notifications suspendues | clair |
| 119 | LOG_WARN | Garde anti-boucle: apres une periode stable, les fonctions suspendues seront relancees a l'essai et surveillees -- sortie manuelle toujours possible entre-temps | clair |
| 124 | count > 1U ? LOG_WARN : LOG_INFO | Garde anti-boucle: demarrage non planifie %u/%u (compteur remis a zero apres %lu s de fonctionnement) | clair |
| 158 | LOG_INFO | Garde anti-boucle: %lu s de fonctionnement stable, compteur de demarrages remis a zero | clair |
| 171 | LOG_WARN | Garde anti-boucle: stable, mais un essai a deja echoue pour cet episode -- reste degrade, sortie manuelle necessaire | clair |
| 193 | LOG_WARN | Garde anti-boucle: %lu s stables en mode degrade -- meteo, mise a jour et notifications relancees A L'ESSAI, sous surveillance %lu s de plus | clair |
| 216 | LOG_INFO | Garde anti-boucle: essai confirme -- fonctionnement normal retabli tout seul apres %u demarrage(s) sans stabilite | clair |
| 245 | LOG_WARN | Garde anti-boucle: mode degrade leve a la demande -- les fonctions suspendues reprennent immediatement | clair |
| 257 | LOG_WARN | Redemarrage voulu: %s | clair |

## src/SystemDiagnostics.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 44 | LOG_WARN | OTA diag: %s absent | clair |
| 48 | LOG_INFO | OTA diag: %s label=%s subtype=%s address=0x%06lX size=%lu | clair |
| 78 | LOG_INFO | OTA diag: heapFree=%lu heapMin=%lu largestBlock=%lu | clair |
| 94 | LOG_INFO | OTA diag: otadata label=%s address=0x%06lX size=%lu | clair |
| 102 | LOG_WARN | OTA diag: otadata absent | clair |
| 109 | layoutReady && sizesReady ? LOG_INFO : LOG_ERROR | OTA diag: layout=%s sizes=%s ready=%s | clair |
| 242 | LOG_ERROR | Memoire: seuil bas franchi libre=%lu plusGrosBloc=%lu (seuil=%lu) — risque d'echec d'allocation | clair |
| 256 | LOG_INFO | Memoire: retour a la normale libre=%lu plusGrosBloc=%lu | clair |
| 268 | LOG_WARN | Memoire: toujours basse libre=%lu plusGrosBloc=%lu | clair |
| 311 | LOG_WARN | Timing: boucle lente duration=%lu us threshold=%lu us count=%lu | clair |

## src/EquipmentExecutionShadowRuntime.cpp (hors SHDW-ENGINE/SHDW-PUMP deja convertis, lignes 33/177/205)

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 54 | LOG_WARN | [Activity#%u][Exec#%u] Shadow: zone %u plan remplace passive=yes | obscur |
| 92 | LOG_INFO | Shadow pump arbiter: zone %u %s users=%u->%u transition=%s consistent=%s passive=yes | obscur |
| 130 | loaded ? LOG_INFO : LOG_WARN | [Activity#%u][Exec#%u] Shadow: zone %u %s accepted=%s steps=%u source_steps=%u pump=%s users=%u consistent=%s passive=yes | obscur |
| 263 | LOG_INFO | [Activity#%u][Exec#%u] Shadow: zone %u step=%u/%u action=%s consumed passive=yes | obscur |
| 277 | LOG_INFO | [Activity#%u][Exec#%u] Shadow: zone %u state=RUNNING passive=yes | obscur |
| 283 | LOG_INFO | [Activity#%u][Exec#%u] Shadow: zone %u state=WAITING delay=%lu passive=yes | obscur |
| 288 | LOG_INFO | [Activity#%u][Exec#%u] Shadow: zone %u state=SUCCEEDED duration=%lu users=%u consistent=%s passive=yes | obscur |
| 294 | LOG_WARN | [Activity#%u][Exec#%u] Shadow: zone %u state=FAILED error=%u passive=yes | obscur |
| 299 | LOG_WARN | [Activity#%u][Exec#%u] Shadow: zone %u state=CANCELLED passive=yes | obscur |

## src/IncidentManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 43 | LOG_INFO | Incident SD: restaure etat=%s occurrences=%lu notifications=0x%02X | obscur |
| 78 | LOG_WARN | Incident SD: actif occurrences=%lu raison=%s | clair |
| 107 | LOG_ERROR | Incident SD: echec recuperation persiste en NVS | clair |
| 132 | LOG_INFO | Incident SD: recupere, acquittement utilisateur requis | clair |
| 146 | LOG_INFO | Incident SD: acquitte | clair |
| 189 | LOG_WARN | Incident SD: ouverture NVS lecture impossible | clair |
| 208 | LOG_WARN | Incident SD: enregistrement NVS invalide | clair |
| 236 | LOG_ERROR | Incident SD: ouverture NVS ecriture impossible | clair |
| 248 | LOG_ERROR | Incident SD: ecriture NVS incomplete | clair |

## src/NTPManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 56 | LOG_INFO | NTP: heure retenue au redemarrage, arrosage possible sans attendre la synchronisation | clair |
| 60 | LOG_INFO | NTP: synchronisation lancee | clair |
| 70 | LOG_INFO | NTP: reconfiguration suite a configDirty | obscur |
| 89 | LOG_INFO | NTP: synchronise %s | clair |
| 93 | LOG_INFO | NTP: heure retrouvee %s — les arrosages programmes reprennent | clair |
| 112 | LOG_ERROR | NTP: heure toujours inconnue apres %lu min — aucun arrosage programme ne peut demarrer tant que l'heure n'est pas connue (serveur=%s) | clair |
| 123 | LOG_ERROR | NTP: heure toujours inconnue depuis %lu min — arrosage programme toujours a l'arret | clair |
| 137 | LOG_INFO | NTP: config serveur=%s gmt=%ld dst=%ld | clair |
| 147 | LOG_INFO | NTP: config compile-time | clair |

## src/EquipmentRuntimeConfigStore.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 68 | LOG_ERROR | Equipment config: defauts surs actifs, persistance impossible | clair |
| 77 | LOG_WARN | Equipment config: absente/invalide, defauts surs persistes mode=disabled | clair |
| 123 | LOG_ERROR | Equipment config: bloc NVS invalide, mode sur disabled | clair |
| 132 | LOG_INFO | Equipment config: chargee mode=%s enabled=%s assignment=%u delays=%u/%u | obscur |
| 147 | LOG_WARN | Equipment config: sauvegarde refusee, configuration invalide | clair |
| 176 | LOG_ERROR | Equipment config: ecriture NVS incomplete (%u/%u) | clair |
| 189 | LOG_INFO | Equipment config: sauvegarde NVS OK (%u octets, schema %u) | clair |
| 209 | removed ? LOG_INFO : LOG_WARN | "Equipment config: cle NVS effacee, mode disabled" / "Equipment config: cle NVS absente, mode disabled" | clair |

## src/OtaTlsProbe.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 25 | LOG_INFO | OTA-1.1: memory stage=%s heapFree=%lu heapMin=%lu largestBlock=%lu stackFree=%u | obscur |
| 41 | LOG_WARN | OTA-1.1: probe start target=api.github.com method=HEAD insecure=yes otaWrite=no | obscur |
| 57 | connected ? LOG_INFO : LOG_ERROR | OTA-1.1: tls connected=%s durationMs=%lu | obscur |
| 68 | LOG_ERROR | OTA-1.1: result=failed phase=tls error=%d detail=%s | obscur |
| 95 | LOG_ERROR | OTA-1.1: result=failed phase=response connected=%s | obscur |
| 108 | LOG_INFO | OTA-1.1: http statusLine=%s | obscur |
| 115 | LOG_INFO | OTA-1.1: result=success tls=yes request=yes otaWrite=no | obscur |
| 144 | LOG_ERROR | OTA-1.1: task creation failed | obscur |

## src/WeatherManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 182 | LOG_WARN | Meteo: impossible de creer la tache de fetch | clair |
| 229 | LOG_WARN | Meteo: fetch reporte, memoire insuffisante (libre=%lu plusGrosBloc=%lu seuils=%lu/%lu) — nouvel essai dans %lu s | clair |
| 302 | LOG_INFO | Meteo: fetch asynchrone lance | clair |
| 366 | LOG_INFO | Meteo: HTTP 200 annonce=%ld lecture=stream type=%s | obscur |
| 594 | LOG_WARN | Meteo: fetch echoue code=%d taille=%ld retry=%lus erreur=%s | clair |
| 617 | LOG_INFO | Meteo: fetch termine taille=%ld pluie=%.1fmm temp=%.1fC statut=%s | clair |
| 736 | LOG_INFO | Meteo: %s resolu en %.4f / %.4f | clair |
| 854 | LOG_INFO | Meteo: Open-Meteo %u jour(s), %ld octets | clair |

## src/RelaisManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 61 | LOG_WARN | Relais: aucun cablage enregistre, aucune sortie pilotable | clair |
| 69 | LOG_INFO | Relais: cablage retenu, %u voie(s) declaree(s) | clair |
| 75 | LOG_ERROR | Relais: aucune carte relais I2C initialisee | clair |
| 101 | LOG_ERROR | Relais: cablage invalide, doublon de mapping | clair |
| 105 | LOG_INFO | Relais: cablage NVS, carte0=%s 0x%02X, voies=%u | clair |
| 114 | LOG_WARN | Relais: module non cable -- interface web, Zones > Cablage relais | clair |
| 139 | LOG_ERROR | Relais: securite, zone %u a couper apres %lus | clair |

## src/ScriptStore.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 50 | LOG_WARN | Scripts: taille NVS %u inattendue, ignoree | clair |
| 59 | LOG_WARN | Scripts: entete NVS invalide, ignoree | clair |
| 64 | LOG_WARN | Scripts: CRC NVS invalide, ignoree | clair |
| 85 | LOG_ERROR | Scripts: ouverture NVS impossible | clair |
| 91 | LOG_ERROR | Scripts: ecriture NVS incomplete | clair |
| 140 | LOG_WARN | Scripts: programme %u refuse (%s) | clair |
| 163 | LOG_INFO | Scripts: programme %u enregistre (%u octets) | clair |

## src/UpdateCheckScheduler.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 38 | LOG_INFO | Verif MAJ: %s a %02u:%02u tous les %u jour(s) | clair |
| 64 | LOG_ERROR | Verif MAJ: enregistrement du reglage impossible | clair |
| 92 | LOG_INFO | Verif MAJ: reglage change -> %s a %02u:%02u tous les %u jour(s) | clair |
| 105 | LOG_INFO | Verif MAJ: echeance atteinte mais report — %s | clair |
| 141 | LOG_INFO | Verif MAJ: premiere echeance fixee au %02u:%02u dans %u jour(s) | clair |
| 188 | LOG_ERROR | Verif MAJ: demande de maintenance non enregistree, verification abandonnee pour cette echeance | clair |
| 195 | LOG_WARN | Verif MAJ: echeance %02u:%02u atteinte, redemarrage en mode maintenance pour verifier les mises a jour | clair |

## src/ApiAuth.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 99 | LOG_WARN | API: remplacement du secret refuse | clair |
| 110 | LOG_INFO | API: secret d'authentification enregistre | clair |
| 117 | LOG_WARN | API: aucun secret enregistre, ecriture refusee | clair |
| 125 | LOG_WARN | API: nonce %lu deja vu (dernier %lu), refuse | clair |
| 135 | LOG_WARN | API: signature invalide, ecriture refusee | clair |
| 153 | LOG_WARN | API: secret efface depuis l'ecran du module (secret oublie) | clair |

## src/WebAssetsUpdater.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 277 | LOG_INFO | WebAssets: verification OK bytes=%lu durationMs=%lu | clair |
| 444 | LOG_INFO | WebAssets: verification installee=%s disponible=%s fichiers=%u -> %s | clair |
| 509 | LOG_INFO | WebAssets: deja a jour en version %s | clair |
| 551 | LOG_ERROR | WebAssets: %s echoue (%s) | clair |
| 556 | LOG_INFO | WebAssets: %s verifie (%lu octets) | clair |
| 588 | LOG_INFO | WebAssets: deploiement termine version=%s fichiers=%u | clair |

## src/IoExpanderConfig.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 117 | LOG_WARN | IoExpander: taille NVS invalide (%u/%u) | clair |
| 128 | LOG_WARN | IoExpander: entete NVS invalide | clair |
| 134 | LOG_ERROR | IoExpander: CRC NVS invalide | clair |
| 155 | LOG_ERROR | IoExpander: ouverture NVS ecriture impossible | clair |
| 161 | LOG_ERROR | IoExpander: ecriture NVS incomplete (%u/%u) | clair |

## src/OtaBootGuard.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 48 | LOG_WARN | OTA garde: armee, precedente=%s cible=%s attempts=0/%u delaiValidationMs=%lu | clair |
| 81 | LOG_ERROR | OTA garde: %u redemarrage(s) sans validation sur %s, retour vers %s | clair |
| 89 | LOG_ERROR | OTA garde: partition precedente %s introuvable, retour arriere impossible | clair |
| 102 | LOG_WARN | OTA garde: demarrage sur %s en attente de validation, tentative %u/%u | clair |
| 119 | LOG_INFO | OTA garde: firmware valide apres %lu ms de fonctionnement stable, retour arriere desarme | clair |

## src/PausedWateringStore.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 53 | LOG_ERROR | Pause: ouverture NVS impossible | clair |
| 60 | LOG_ERROR | Pause: ecriture NVS incomplete | clair |
| 76 | LOG_WARN | Pause: taille NVS %u inattendue, ignoree | clair |
| 88 | LOG_WARN | Pause: entete NVS invalide, ignoree | clair |
| 93 | LOG_WARN | Pause: CRC NVS invalide, ignoree | clair |

## src/ScriptHostRuntime.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 64 | LOG_WARN | Script: zone %u inconnue, action refusee | clair |
| 90 | LOG_WARN | Script: action %u non implementee | obscur |
| 103 | LOG_INFO | Script: message %u | obscur |
| 114 | LOG_WARN | Script: alerte %u NON envoyee, notifications non configurees | obscur |
| 120 | LOG_INFO | Script: alerte %u envoyee | obscur |

## src/ScriptRunner.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 76 | LOG_INFO | Script %u (%s) demarre | clair |
| 132 | LOG_WARN | Script %u non relance : moins de %lus depuis son dernier depart (boucle probable) | clair |
| 146 | LOG_WARN | Script %u non lance (%s) | clair |
| 158 | LOG_INFO | Script %u termine : %u lecture(s), %u action(s), %u refus | clair |
| 168 | LOG_ERROR | Script %u ARRETE : %s (apres %lu instructions) | clair |

## src/EquipmentManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 277 | LOG_WARN | Equipment plan: zone %u %s dry_run=yes error=%u | obscur |
| 287 | LOG_INFO | Equipment plan: zone %u %s steps=%u pump=%s dry_run=yes | obscur |
| 299 | LOG_INFO | Equipment plan: zone %u step=%u action=%s delay=%lu dry_run=yes | obscur |
| 310 | LOG_INFO | Equipment plan: zone %u step=%u action=%s equipment=%u dry_run=yes | obscur |

## src/EquipmentOutputRuntimeAdapter.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 120 | LOG_ERROR | Equipment: zone %u %s path=failed error=invalid_target | obscur |
| 169 | unmapped ? LOG_WARN : LOG_ERROR | "Equipment: zone %u %s ignore, aucune sortie affectee" / "Equipment: zone %u %s path=failed error=dependency_unavailable" | obscur |
| 195 | LOG_INFO | Equipment: zone %u %s path=%s exec=%u ok=%lu ko=%lu | obscur |

## src/InputSampler.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 24 | LOG_INFO | Entrees: %u surveillee(s), stable apres %u ms | clair |
| 50 | LOG_WARN | Entrees: entree %u ne repond plus | clair |
| 77 | LOG_INFO | Entrees: entree %u -> %s | clair |

## src/IoExpanderManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 25 | LOG_INFO | IoExpander: actif, %u binding(s) | clair |
| 93 | ok ? LOG_INFO : LOG_WARN | IoExpander: carte %u @0x%02X %s (iodir=0x%04X) | obscur |
| 162 | LOG_WARN | IoExpander: vanne ABSENTE zone=%u (binding %u) | clair |

## src/ScheduleManager.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 305 | LOG_INFO | Schedule: zone %u suspendue, reste %lus | clair |
| 332 | LOG_INFO | Schedule: zone %u reprise pour %lus | clair |
| 358 | LOG_WARN | Schedule: zone %u abandonnee, suspendue plus de %lu min | clair |

## src/V4PilotRuntime.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 119 | LOG_ERROR | Relais V4: carte %u refusee par le registre (erreur %u) | obscur |
| 177 | LOG_INFO | Relais V4: %u carte(s) pilotee(s), %u zone(s) raccordee(s) | clair |
| 188 | LOG_ERROR | Relais V4: zone %u sur carte %u sans pilote | clair |

## src/MaintenanceSetupWrapper.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 51 | LOG_WARN | Maintenance: entree sure avant setup type=%s | clair |
| 68 | LOG_ERROR | Maintenance: creation tache impossible, retour mode normal | clair |

## src/V4RelayPhysicalBackend.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 47 | LOG_ERROR | Relais V4: echec %s zone=%u | clair |
| 54 | LOG_INFO | Relais V4: pilotage retabli | clair |

## src/DisplayManager.h

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 176 | LOG_ERROR | Affichage: allocation des tampons impossible (bouton=%s planning=%s), rendu suspendu | clair |

## src/OtaDownloadTest.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 249 | LOG_INFO | Maintenance: DOWNLOAD_UPDATE_TEST bytes=%lu durationMs=%lu sha256=ok otaWrite=no | clair |

## src/OtaStageUpdate.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 226 | LOG_INFO | Maintenance: STAGE_UPDATE_TEST bytes=%lu durationMs=%lu sha256=ok partition=%s address=0x%06lX otaActivate=no | obscur |

## src/RuntimeProfiler.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 78 | LOG_WARN | Timing: n=%s wallUs=%lu count=%lu schedSuspect=%s schedCount=%lu core=%d | obscur |

## src/SdStaticHandler.cpp

| Line | Level | Message | Clair/Obscur |
|---|---|---|---|
| 171 | LOG_WARN | Web: requete SD refusee (libre=%lu enCours=%u total=%lu) — protection contre la saturation | clair |
