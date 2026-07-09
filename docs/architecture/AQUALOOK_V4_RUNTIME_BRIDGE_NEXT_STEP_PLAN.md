# AquaLook V4 — Phase 4 — Run 4.10 — Plan du prochain raccord runtime

**Date :** 9 juillet 2026  
**Statut :** documentaire, aucune modification runtime  
**Base :** checkpoint `CHECKPOINT_2026-07-09_v4-phase4-run4-9-web-lcd-state-read-validated.md`  
**Branche :** `feature/aqualook-v4-domain`

## 1. Contexte validé

La sous-séquence Web/LCD lectures d’état est validée :

```text
Commande : ScheduleManager -> EquipmentOutputRuntimeAdapter -> RelaisManager
Lecture Web : WebManager -> EquipmentOutputRuntimeAdapter -> fallback RelaisManager
Lecture LCD : DisplayManager -> EquipmentOutputRuntimeAdapter -> fallback RelaisManager
```

Le runtime actif reste donc stable : `RelaisManager` conserve le backend physique réel, tandis que le domaine/runtime peut déjà parler `EquipmentOutput` pour les commandes et les lectures d’état.

## 2. Objectif de Run 4.10

Run 4.10 ne modifie aucun code.

Objectif : choisir le prochain raccord propre avant tout branchement de driver V4 réel.

Il faut éviter de brancher trop vite les drivers Phase 3 dans le runtime actif, car cela toucherait le chemin matériel réel des électrovannes.

## 3. Options envisageables

### Option A — Factoriser les façades Web/LCD

Principe : extraire `OutputAwareRelayState` dans un helper commun.

Avantages :

- réduit la duplication entre `WebManager.h` et `DisplayManager.h` ;
- centralise le fallback ;
- facilite les prochaines migrations de lecture.

Inconvénients :

- changement structurel sans gain fonctionnel immédiat ;
- risque de toucher deux managers validés ;
- peut provoquer une régression alors que la séquence Web/LCD vient d’être stabilisée.

Décision : **différer**.

### Option B — Brancher directement un driver V4 réel dans le runtime

Principe : remplacer progressivement l’écriture physique de relais par un chemin `BinaryActuatorDriverRegistry` ou driver V4.

Avantages :

- avance vers la cible architecturale V4 ;
- prépare les futurs backends GPIO, XL9535, MCP23017 ou autres.

Inconvénients :

- touche le chemin critique d’arrosage ;
- risque matériel plus élevé ;
- besoin d’une stratégie de rollback très claire ;
- nécessite de clarifier la relation avec `RelayTopology` et `RelayAssignment`.

Décision : **trop tôt pour un branchement actif**.

### Option C — Ajouter une passerelle passive `RelayPhysicalBackend`

Principe : documenter puis ajouter ensuite une couche passive entre `EquipmentOutputRuntimeAdapter` et `RelaisManager`, sans changer encore le backend physique.

Chaîne cible intermédiaire :

```text
EquipmentOutputRuntimeAdapter
  -> RelayPhysicalBackend interface/adaptateur
     -> RelaisManager actuel
```

Avantages :

- prépare le remplacement du backend sans toucher immédiatement les drivers matériels ;
- garde `RelaisManager` comme implémentation active ;
- permet une future implémentation driver V4 derrière la même interface ;
- limite les changements dans `ScheduleManager`, Web et LCD.

Inconvénients :

- ajoute une couche supplémentaire ;
- demande de bien nommer la frontière pour ne pas recréer un `RelaisManager` bis.

Décision : **option recommandée**.

## 4. Décision Run 4.10

La prochaine étape code recommandée n’est pas de brancher les drivers V4 directement.

Décision : préparer une couche de transition passive appelée provisoirement :

```text
RelayPhysicalBackend
```

Rôle : isoler le backend physique historique derrière une petite interface, tout en gardant `RelaisManager` comme implémentation active.

Cette couche doit rester interne au runtime, hors domaine pur.

## 5. Frontière proposée

### Domaine/runtime

Parle en concepts :

```text
EquipmentOutput
EquipmentOutputCommand
EquipmentStateValue
OperationResult
```

### Backend relais physique

Parle en concepts :

```text
zone index
relay assignment
board/channel
physical write
readback logical state
```

### RelaisManager

Reste pour l’instant :

```text
implémentation active du backend physique relais
source de fallback
propriétaire actuel de RelayTopology et de l’écriture I2C historique
```

## 6. Run code recommandé après Run 4.10

```text
AquaLook V4 — Phase 4 — Run 4.11
Ajouter une interface passive RelayPhysicalBackend et un adaptateur RelaisManagerBackend
```

Portée Run 4.11 proposée :

- ajouter un header runtime isolé ;
- ajouter une interface minimale ;
- ajouter un adaptateur qui délègue à `RelaisManager` ;
- ne pas brancher l’adaptateur au runtime actif, ou le brancher passivement sans changer le comportement ;
- ne pas modifier NVS ;
- ne pas activer les drivers V4 Phase 3.

## 7. Interface minimale proposée

Nom provisoire :

```cpp
class RelayPhysicalBackend {
public:
    virtual ~RelayPhysicalBackend() = default;
    virtual bool setZoneValve(uint8_t zoneIndex, bool active, uint32_t nowMs) = 0;
    virtual bool getZoneValveState(uint8_t zoneIndex, bool& active) const = 0;
};
```

Adaptateur provisoire :

```cpp
class RelaisManagerBackend : public RelayPhysicalBackend {
public:
    explicit RelaisManagerBackend(RelaisManager* relais);
    bool setZoneValve(uint8_t zoneIndex, bool active, uint32_t nowMs) override;
    bool getZoneValveState(uint8_t zoneIndex, bool& active) const override;
private:
    RelaisManager* _relais = nullptr;
};
```

Ce n’est qu’une proposition : le code final devra être ajusté après inspection des fichiers courants.

## 8. Invariants pour Run 4.11

1. Aucun changement NVS.
2. Aucun changement Web.
3. Aucun changement LCD.
4. Aucun changement JSON.
5. Aucun changement de mapping relais.
6. Aucun driver V4 réel activé.
7. `RelaisManager` reste l’implémentation active.
8. Fallback `RelaisManager` conservé.
9. Compilation PlatformIO obligatoire.
10. Tests matériels limités à vérification non-régression.

## 9. Risques

- Trop abstraire trop tôt ;
- créer une couche redondante avec `RelaisManager` ;
- casser une zone critique si l’adaptateur est branché activement trop vite ;
- mélanger transition runtime et future configuration NVS des équipements.

Mesure de réduction du risque : Run 4.11 doit être passif ou quasi passif, avec un diff minimal.

## 10. Décision finale

Run 4.10 clôt la partie documentaire post Web/LCD.

Prochaine étape recommandée :

```text
Run 4.11 — Interface passive RelayPhysicalBackend + adaptateur RelaisManagerBackend
```

Ne pas brancher les drivers V4 réels avant validation de cette frontière.
