# AquaLook — Checkpoint stable compilé — RelayAssignment roles

Date : 2026-07-06  
Branche : `feature/relay-board-mapping`  
Base source : `refactor/static-assets-sd`  
Base commit : `9b7cf35bb8e35343ff5f5f19fabf073a722017d4`  
Validation utilisateur : compilation OK

## 1. Statut

Ce checkpoint fige l'état courant de la branche après validation de compilation par l'utilisateur.

Commande de test utilisée côté utilisateur : compilation PlatformIO de l'environnement principal.

Résultat annoncé :

```text
c'est ok, ca compile
```

## 2. État fonctionnel figé

La branche contient désormais :

- le modèle `RelayTopology` ;
- le modèle généralisé `RelayAssignment` ;
- les rôles logiques de relais ;
- l'intégration de `RelayTopology` dans `RelaisManager` ;
- la compatibilité runtime avec le modèle existant carte unique ;
- la documentation d'architecture et les checkpoints de run.

## 3. Décision importante

Le projet ne doit plus considérer qu'un relais physique correspond forcément à une zone d'arrosage.

Un relais physique peut être affecté à :

- une électrovanne de zone ;
- une pompe ;
- un contact sec auxiliaire ;
- un volet ou une aération de serre ;
- un éclairage ;
- un autre équipement futur.

Le relais est un moyen d'action. L'équipement est l'objet métier à modéliser dans la suite.

## 4. Invariant de reprise

À partir de ce checkpoint, ne pas revenir à un modèle `zone -> relais` direct.

Le flux cible reste :

```text
usage logique / équipement
    -> RelayAssignment
        -> carte relais I2C
            -> voie physique
```

Pour les zones d'arrosage, le flux compatible actuel est :

```text
zone logique
    -> RelayAssignment(role=ROLE_ZONE_VALVE, targetIndex=zoneIndex)
        -> carte 0
            -> voie zoneIndex
```

## 5. Fichiers déjà modifiés dans la branche

- `src/RelayTopology.h`
- `src/RelayTopology.cpp`
- `src/RelaisManager.h`
- `src/RelaisManager.cpp`
- `docs/architecture/RELAY_TOPOLOGY.md`
- `docs/checkpoints/CHECKPOINT_2026-07-06_relay-board-mapping-run1.md`
- `docs/checkpoints/CHECKPOINT_2026-07-06_relay-topology-code-run2.md`
- `docs/checkpoints/CHECKPOINT_2026-07-06_relay-manager-topology-run3.md`
- `docs/checkpoints/CHECKPOINT_2026-07-06_relay-assignment-roles-run4.md`

## 6. Fichiers non encore modifiés volontairement

- `src/ConfigManager.h`
- `src/ConfigManager.cpp`
- API Web de configuration
- interface Web de paramétrage
- moteur pompe / temporisations pression
- gestion serre / auxiliaires

## 7. Prochaine étape recommandée

Avant de toucher au NVS, créer un document d'architecture métier plus large :

```text
AquaLook Equipment Model / Roadmap V4
```

Objectif : éviter d'ancrer trop vite la persistance sur un modèle encore trop centré relais.

Le prochain modèle doit distinguer :

- zone d'arrosage ;
- équipement ;
- relais physique ;
- carte relais ;
- capteur ;
- automatisation ;
- dépendance entre équipements, par exemple pompe requise avant électrovanne.

## 8. Commande de reprise recommandée

```powershell
git fetch origin
git switch feature/relay-board-mapping
git pull --ff-only
pio run -e ProgrammeArrosage -t upload ; pio device monitor -e ProgrammeArrosage
```
