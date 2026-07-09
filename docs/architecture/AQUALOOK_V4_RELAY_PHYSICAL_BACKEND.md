# AquaLook V4 — Phase 4 — Run 4.11 — RelayPhysicalBackend passif

**Date :** 9 juillet 2026  
**Statut :** ajout code passif, compilation à valider  
**Base :** Run 4.10 — plan du prochain raccord runtime

## 1. Objectif

Run 4.11 ajoute une frontière runtime passive entre la notion `EquipmentOutput` et le backend physique relais.

Cette frontière ne remplace pas encore `RelaisManager`.

Elle prépare une future évolution où plusieurs backends physiques pourront exister derrière la même interface :

- backend historique `RelaisManager` ;
- futur backend driver V4 ;
- futur backend MCP23017 ou autre extension.

## 2. Fichiers ajoutés

```text
src/RelayPhysicalBackend.h
src/RelaisManagerBackend.h
src/RelaisManagerBackend.cpp
```

Aucun fichier runtime existant n’est modifié dans ce run.

## 3. Interface ajoutée

Fichier :

```text
src/RelayPhysicalBackend.h
```

Interface :

```cpp
class RelayPhysicalBackend {
public:
    virtual ~RelayPhysicalBackend() = default;

    virtual bool setZoneValve(
        uint8_t zoneIndex,
        bool active,
        uint32_t nowMs = 0U
    ) = 0;

    virtual bool getZoneValveState(
        uint8_t zoneIndex,
        bool& active
    ) const = 0;
};
```

## 4. Adaptateur RelaisManager ajouté

Fichiers :

```text
src/RelaisManagerBackend.h
src/RelaisManagerBackend.cpp
```

Rôle : adapter `RelaisManager` à l’interface `RelayPhysicalBackend`.

Chaîne interne :

```text
RelaisManagerBackend::setZoneValve(zone, active)
  -> RelaisManager::setRelay(zone, active)

RelaisManagerBackend::getZoneValveState(zone, active)
  -> RelaisManager::getState(zone)
```

## 5. Caractère passif

Run 4.11 n’introduit aucun branchement actif.

Non modifiés :

```text
src/main.cpp
src/EquipmentOutputRuntimeAdapter.h
src/EquipmentOutputRuntimeAdapter.cpp
src/WebManager.h
src/DisplayManager.h
src/WebManager.cpp
src/DisplayManager.cpp
```

Donc la chaîne runtime active reste :

```text
ScheduleManager
  -> EquipmentOutputRuntimeAdapter
  -> RelaisManager
```

## 6. Pourquoi les fallbacks restent en place

Les fallbacks Web/LCD ne sont pas supprimés en Run 4.11.

Ils restent nécessaires tant que :

- le backend physique V4 n’est pas branché ;
- la nouvelle frontière n’est pas utilisée par le runtime actif ;
- les tests matériels n’ont pas validé le nouveau chemin complet ;
- le rollback vers `RelaisManager` doit rester immédiat.

La suppression des fallbacks sera une étape tardive, après validation réelle du chemin suivant :

```text
EquipmentOutputRuntimeAdapter
  -> RelayPhysicalBackend
  -> backend actif validé
```

## 7. Invariants préservés

1. Aucun changement NVS.
2. Aucun changement Web.
3. Aucun changement LCD.
4. Aucun changement JSON.
5. Aucun changement planning.
6. Aucun changement de comportement runtime actif.
7. Aucun driver V4 réel activé.
8. `RelaisManager` reste le backend physique actif.
9. Les fallbacks Web/LCD restent en place.
10. Compilation PlatformIO obligatoire avant suite.

## 8. Validation attendue

Commande :

```powershell
pio run -e ProgrammeArrosage
```

Critère : compilation réussie.

Comme les fichiers ne sont pas encore branchés au runtime actif, le test fonctionnel matériel attendu est simplement une non-régression rapide.

## 9. Suite recommandée

Après compilation OK :

```text
AquaLook V4 — Phase 4 — Run 4.12
Injection passive RelayPhysicalBackend dans EquipmentOutputRuntimeAdapter
```

Le Run 4.12 devra rester prudent :

- brancher l’interface avec `RelaisManagerBackend` comme implémentation ;
- conserver le fallback direct `RelaisManager` dans `EquipmentOutputRuntimeAdapter` ;
- ne pas brancher les drivers V4 réels ;
- ne pas toucher NVS.
