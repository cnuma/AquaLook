# Checkpoint AquaLook — notifications de zones validées

Date : 2026-07-23

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/storage-sd-recovery`
- Commit fonctionnel validé matériellement : `f16be3c0279643e4c4ae09144484be9e54b4f499`
- Message du commit fonctionnel : `wip: save zone notification implementation`
- Le présent document est ajouté dans un commit documentaire séparé.

## Objet du checkpoint

Ce checkpoint fige la validation matérielle des notifications ntfy envoyées lors du démarrage et de l’arrêt réel d’une zone d’arrosage, tout en conservant les fonctions de récupération SD et les invariants de stabilité du contrôleur principal.

## État validé avant cette évolution

- récupération automatique de la carte SD opérationnelle ;
- incidents SD persistés en NVS ;
- journal technique enrichi ;
- configuration ntfy sauvegardée en NVS ;
- tactile opérationnel ;
- portail captif opérationnel ;
- aucune libération forcée des sprites graphiques ;
- contrôleur principal sans PSRAM.

## Diagnostic ntfy validé

Le transport HTTPS/TLS direct échouait avec :

```text
SSL - Memory allocation failed (-32512)
```

Mesures observées :

- heap libre autour de 74 Ko ;
- plus grand bloc contigu autour de 39 Ko ;
- DNS opérationnel ;
- TCP opérationnel ;
- cause identifiée : fragmentation mémoire nécessaire au handshake TLS.

La tentative antérieure de libération des sprites pour rendre TLS possible a provoqué une régression tactile et portail captif. Cette approche est interdite pour la suite.

## Architecture ntfy retenue à ce checkpoint

Le transport ntfy utilise temporairement HTTP sans TLS :

```text
AquaLook -> HTTP port 80 -> ntfy.sh -> téléphone
```

Validation matérielle :

```text
Notification: tcp host=ntfy.sh port=80 status=ok
Notification: reponse http=200
Notification: livree ... http=200
```

La réception effective sur le téléphone a été confirmée.

### Limite de sécurité

Le topic, le contenu et un éventuel jeton circulent en clair. Ne pas utiliser de jeton sensible avec ce transport. Une future passerelle locale, Home Assistant, MQTT ou ESP32-S2 reste l’architecture recommandée pour rétablir TLS hors du contrôleur principal.

## Notifications configurables par zone

Chaque zone possède deux options indépendantes :

- notifier au démarrage ;
- notifier à l’arrêt.

Les options sont désactivées par défaut pour les configurations existantes.

La persistance utilise un masque extensible :

```cpp
bit 0 = notification de démarrage
bit 1 = notification d’arrêt
```

## Migration NVS

La configuration principale passe du schéma 1 au schéma 2.

Le schéma 2 ajoute :

```text
zoneNotificationMasks[MAX_ZONES]
```

Comportement de migration :

1. reconnaissance stricte de la taille du schéma 1 ;
2. vérification du magic, du schéma, de `payloadSize` et du CRC ;
3. copie des anciens champs sans reconstruction ;
4. initialisation des masques de notification à `0x00` ;
5. sauvegarde immédiate au schéma 2 avec nouveau CRC ;
6. refus propre d’un bloc NVS invalide, sans écrasement silencieux.

Validation au démarrage réel :

```text
Config: charge depuis NVS (schema 2)
```

Aucune perte de configuration observée.

## Chaîne d’exécution validée

```text
Planning ou commande manuelle
        -> orchestrateur
        -> adaptateur runtime
        -> backend physique ou fallback Legacy
        -> retour de succès réel
        -> détection de transition ON/OFF
        -> file ntfy
        -> envoi HTTP
```

La détection de transition est réalisée après succès réel du backend physique.

Invariants :

- une intention Schedule seule ne déclenche pas de notification ;
- le shadow orchestrator ne déclenche pas de notification ;
- une commande répétée vers le même état ne crée pas de doublon ;
- un échec matériel ON/OFF ne doit pas créer de notification ;
- la commande de relais ne dépend jamais de la réussite ntfy.

## Validation matérielle des relais

Le contrôleur relais validé est le XL9535 à l’adresse I2C `0x20`.

Boot valide :

```text
I2C: peripherique trouve a 0x20
Relais: carte 0 XL9535 0x20 voies=2 OK
Relais: topologie init OK
```

Transition réelle validée :

```text
Equipment: zone 1 ON path=physical_backend
Equipment: zone 1 OFF path=physical_backend
```

Le premier diagnostic d’absence du relais provenait simplement de la carte relais non branchée.

## Validation fonctionnelle ntfy par zone

Validation utilisateur sur matériel réel :

- notification au démarrage reçue sur le téléphone ;
- notification à l’arrêt reçue sur le téléphone ;
- backend physique opérationnel ;
- aucun doublon observé lors du test ;
- options configurables depuis la fenêtre de chaque zone.

Messages fonctionnels :

```text
AquaLook - début d’arrosage
Zone <nom> démarrée
```

```text
AquaLook - fin d’arrosage
Zone <nom> arrêtée
```

## File de notifications

- capacité : 8 événements de zone ;
- FIFO en RAM ;
- copie de l’index, de l’état et du nom de la zone ;
- suppression après succès HTTP ;
- maintien de l’événement en tête en cas d’échec réseau ;
- événements de zone non persistés après reboot ;
- refus du nouvel événement et journalisation si la file est pleine.

Priorité :

1. incidents SD ;
2. test manuel ntfy ;
3. événements de zone.

Un envoi déjà démarré n’est pas interrompu.

## Journal Web

Une route texte dédiée est utilisée :

```text
GET /api/logs.txt
Content-Type: text/plain; charset=utf-8
```

La page `logs.html` consomme directement cette réponse texte.

## Ressources Web sur carte SD

Dans cette branche, les ressources Web principales sont servies depuis :

```text
/www
```

Les fichiers modifiés dans `data/`, notamment :

```text
data/app.js
data/logs.html
```

doivent être recopiés sur la carte SD dans :

```text
/www/app.js
/www/logs.html
```

La commande `uploadfs` construit et téléverse le dossier `littlefs/` ; elle ne met pas automatiquement à jour les ressources Web stockées sur la SD.

## Règle de diagnostic Web à conserver

Avant de conclure à une régression de l’interface :

1. ouvrir `/api/status` et vérifier HTTP 200 + JSON valide ;
2. ouvrir directement `/app.js` et confirmer que la version attendue est servie ;
3. vérifier la cohérence entre firmware, fichiers `/www` sur SD et cache navigateur ;
4. effectuer `Ctrl+F5` ;
5. tester en navigation privée ;
6. vérifier la console JavaScript ;
7. seulement ensuite modifier le code.

Une interface temporairement vide a été observée alors que `/api/status` était valide. Le comportement est rentré dans l’ordre après remise en cohérence/rechargement des ressources Web. Le cache ou un chargement transitoire de ressources SD doit donc être suspecté avant toute modification corrective.

## Compilations validées avant sauvegarde

- `ProgrammeArrosage` : SUCCESS ; RAM 73 256 octets, 22,4 % ; flash 1 320 129 octets, 65,0 % ;
- `ProgrammeArrosage_legacy` : SUCCESS ; RAM 22,4 % ; flash 1 320 153 octets, 65,0 % ;
- `ProgrammeArrosage_v4` : SUCCESS ; RAM 22,4 % ; flash 1 324 433 octets, 65,2 % ;
- `buildfs` : SUCCESS.

Avertissement non bloquant connu : SdFat / `FS.h`.

## Fichiers fonctionnels concernés

- `data/app.js`
- `data/logs.html`
- `src/ConfigManager.cpp`
- `src/ConfigManager.h`
- `src/EquipmentOutputRuntimeAdapter.cpp`
- `src/NotificationManager.cpp`
- `src/NotificationManager.h`
- `src/RelaisManager.cpp`
- `src/RelaisManager.h`
- `src/RelaisManagerBackend.cpp`
- `src/WebManager.cpp`
- `src/WebManager.h`
- `src/main.cpp`

## Points connus non bloquants

- message `Preferences.cpp: nvs_open failed: NOT_FOUND` possible au premier démarrage avant création du namespace ntfy ;
- avertissement `addApbChangeCallback(): duplicate` toujours présent ;
- absence de partition core dump dans la table OTA actuelle ;
- événements de zone ntfy non persistés après reboot ;
- transport HTTP non chiffré provisoire.

## Invariants à préserver

1. ne pas réintroduire TLS directement sur le contrôleur principal sans nouvelle architecture mémoire validée ;
2. ne pas libérer les sprites graphiques pour tenter TLS ;
3. préserver tactile et portail captif ;
4. préserver la récupération SD et la persistance des incidents ;
5. déclencher les notifications de zone uniquement après succès réel du backend ;
6. ne jamais bloquer l’arrosage sur une opération réseau ;
7. conserver les options de notification désactivées par défaut lors de toute migration ;
8. vérifier systématiquement les ressources Web SD et le cache avant de conclure à une régression front-end.

## Procédure de reprise

```powershell
cd C:\Users\emman\OneDrive\Documents\VsCode_travail\arrosage

git switch work/storage-sd-recovery
git fetch origin
git pull --ff-only origin work/storage-sd-recovery
git status
git rev-parse HEAD
git rev-parse origin/work/storage-sd-recovery
```

Le HEAD local et distant doivent être identiques au commit documentaire créé pour ce checkpoint. Le commit fonctionnel de référence sous-jacent reste `f16be3c0279643e4c4ae09144484be9e54b4f499`.

## Statut

Checkpoint matériel validé pour :

- récupération SD ;
- ntfy HTTP ;
- notifications configurables de démarrage et d’arrêt par zone ;
- déclenchement après transition physique réelle ;
- migration NVS schéma 2 ;
- compilation Legacy et V4.

Aucune fusion vers une autre branche n’est réalisée par ce checkpoint.
