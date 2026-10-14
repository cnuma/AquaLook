# AquaLook — Situation du projet

Mis à jour le 14 octobre 2026 (version 5.16.0). Vue consolidée : l'état du
jour est dans le checkpoint le plus récent de `docs/checkpoints/`, le reste à
faire dans `docs/TODO.md` (liste unique), les décisions dans
`docs/codex/02_DECISIONS.md`. En cas d'écart, ces sources et le code priment.

## 1. Finalité

AquaLook est un programmateur d'arrosage autonome sur ESP32-S3 : planification
locale de 1 à 8 zones, pilotage de vannes par cartes relais I²C, écran tactile,
interface Web locale, moteur de scripts, et un espace en ligne optionnel
(service PHP sur AlwaysData) pour suivre et régler ses modules à distance.

Le produit doit continuer à exécuter les programmes d'arrosage et les sécurités
matérielles même lorsque le Wi-Fi, Internet, le serveur, la carte SD ou un
service distant sont indisponibles.

## 2. Plateforme matérielle de référence

- carte : Guition JC4827W543C_I, ESP32-S3 (module de banc `.141` : N4R8,
  4 Mo de flash, PSRAM OPI 8 Mo) ;
- écran : NV3041A 480 × 272 en QSPI ; tactile capacitif GT911 sur I²C dédié ;
- relais : XL9535 sur le second bus I²C (`Wire1`), adresse `0x20` ;
  MCP23017 pris en charge ; câblage décrit comme une donnée (topologie) ;
- SD : SPI dédié (SPI3) ; voyant : ruban WS2812 (une LED d'état + une par zone) ;
- partitions : deux emplacements OTA de 1 920 Kio, NVS 84 Kio, LittleFS 108 Kio.

Brochage centralisé dans la section `[jc4827w543c_i]` de `platformio.ini`.
L'ancienne carte CYD (ESP32-2432S028) est en cours d'abandon : compilée
seulement sur demande explicite.

## 3. Capacités validées sur matériel

- moteur d'exécution unique V4 (zones → équipements → cartes → ports → voies),
  le moteur historique ayant été supprimé le 8 septembre 2026 ;
- planification jours fixes et intervalle, décalage ou annulation sur pluie
  (OpenWeatherMap ou Open-Meteo) ;
- persistance en NVS versionnée (magic, schéma, taille, CRC) ;
- écran et tactile, page À propos (version et build), LED de zones ;
- ressources Web sur la SD avec secours embarqué ;
- moteur de scripts (compilé dans le navigateur, revalidé par le module),
  éditeur graphique, sur le module et dans l'espace en ligne (D014, D015) ;
- notifications ntfy en HTTPS ;
- synchronisation avec l'espace en ligne (CloudSync), enrôlement par code
  court, PIN sur l'écran, session Web locale obligatoire pour toute écriture
  (D016) ; nom local `http://<nom>.local` (D019) ;
- mises à jour des pages Web signées (ECDSA P-256) avec anti-rejeu (D018),
  progression affichée sur l'écran pendant la maintenance (P2.14) ;
- routes de développement absentes du binaire de production (D020).

## 4. État des chantiers majeurs

### 4.1 Cœur fonctionnel

Le moteur local reste la fonction critique. Les décisions de planification ne
dépendent jamais d'un service distant. Le planificateur ne commande pas
directement les sorties : la commande passe par le callback relais câblé dans
`main.cpp` et par les sécurités de `RelaisManager`.

### 4.2 Mise à jour du programme (OTA firmware)

Les partitions doubles et le chemin d'installation sont validés. Reste à faire
(P1.4 et P1.9 lot 2b) : une release GitHub qui contienne le binaire S3 (elle ne
contient aujourd'hui que des cibles CYD) et la signature du manifeste firmware,
faite localement par le propriétaire, la clé privée restant hors ligne.

### 4.3 Service en ligne

PHP et MySQL sur AlwaysData (`cloud/php-mutualized/`), déployé depuis Git
(`tools/deploy_cloud.py`). La trajectoire MQTT, HiveMQ, Flutter et VPS
envisagée en juillet 2026 a été remplacée par ce service et ne fait plus
partie du périmètre.

### 4.4 Cybersécurité

Pilier transverse permanent et référentiel vivant (D017) :
`docs/security/CYBERSECURITY_ARCHITECTURE.md`,
`docs/security/SECURITY_RISK_REGISTER.md` et
`docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md`. Ouverts : HTTPS sur le
réseau local (P0.5), secret Web en clair dans la NVS (P1.6), longueur du PIN
(P1.7), pages intégrées au firmware avec script inline (P3.13).

## 5. Invariants majeurs

1. Une panne réseau ou cloud ne bloque pas l'arrosage local.
2. Une panne de capteur ou d'extension ne provoque jamais une activation intempestive.
3. Le planificateur ne pilote jamais directement le matériel.
4. La durée maximale de sécurité des relais ne doit pas être supprimée.
5. La SD n'est jamais indispensable au démarrage minimal et à la récupération.
6. Une modification de schéma NVS est versionnée et migrée ou explicitement réinitialisée.
7. Les bus écran, tactile, SD et I²C conservent leurs contraintes matérielles validées.
8. Une recommandation intelligente ne modifie pas silencieusement les programmes.
9. Une fonction distante confirme l'exécution réelle au lieu de supposer qu'une commande reçue a réussi.
10. Aucune mise à jour n'est déclenchée à distance : le module signale, l'utilisateur déclenche (D014).
11. Toute évolution importante met à jour code, tests, risques et documentation.

## 6. Risques techniques principaux

- mise à jour du programme à distance encore impossible sur S3 (P1.4) ;
- binaire de production jamais observé longtemps : `.141` tourne le binaire
  de banc (D020) ;
- concurrence entre la boucle principale et la tâche réseau sur le même cœur ;
- complexité croissante de la configuration persistée ;
- peu de tests automatiques (pas de CI de compilation, P4.1 ; planning, P4.2) ;
- dépôt dans OneDrive (`.pio/` resynchronisé, P4.6).

## 7. Priorités

Voir `docs/TODO.md`, qui fait foi et est tenu à jour à chaque palier.
