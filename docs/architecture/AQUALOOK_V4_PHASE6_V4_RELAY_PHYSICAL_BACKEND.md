# AquaLook V4 — Phase 6 — Run 6.2 — V4RelayPhysicalBackend inactif

**Date :** 10 juillet 2026  
**Statut :** code ajouté, compilation à valider  
**Base :** Run 6.1 — stratégie de bascule du backend physique

## 1. Objectif

Ajouter une implémentation concrète de `RelayPhysicalBackend` destinée au futur backend matériel V4, sans activation runtime.

Le Run 6.2 ne doit :

- modifier ni `main.cpp` ni `EquipmentOutputRuntimeAdapter` ;
- activer aucune zone ;
- appeler aucun driver Phase 3 ;
- modifier aucune donnée NVS ;
- modifier ni Web, ni LCD, ni JSON.

## 2. Fichiers ajoutés

```text
src/V4RelayPhysicalBackend.h
src/V4RelayPhysicalBackend.cpp
```

## 3. Classe ajoutée

```cpp
class V4RelayPhysicalBackend : public RelayPhysicalBackend
```

Méthodes exposées :

```cpp
bool setZoneValve(uint8_t zoneIndex, bool active, uint32_t nowMs) override;
bool getZoneValveState(uint8_t zoneIndex, bool& active) const override;
bool isZoneMigrated(uint8_t zoneIndex) const;
bool hasAnyMigratedZone() const;
```

## 4. État initial sûr

Le backend contient un masque interne :

```cpp
uint32_t _migratedZoneMask = 0U;
```

Conséquence :

```text
aucune zone migrée
aucune commande V4 appliquée
aucune lecture V4 considérée valide
```

`setZoneValve(...)` renvoie `false` dans tous les cas.

`getZoneValveState(...)` force `active=false` puis renvoie `false`.

Ce comportement est volontaire : si ce backend était injecté accidentellement, `EquipmentOutputRuntimeAdapter` utiliserait encore son fallback direct `RelaisManager`.

## 5. Absence d’activation runtime

Non modifiés :

```text
src/main.cpp
src/EquipmentOutputRuntimeAdapter.h
src/EquipmentOutputRuntimeAdapter.cpp
src/RelaisManagerBackend.h
src/RelaisManagerBackend.cpp
```

Aucune instance globale de `V4RelayPhysicalBackend` n’est créée.

Aucun appel suivant n’existe :

```cpp
outputAdapter.setPhysicalBackend(&v4RelayBackend);
```

Le backend actif reste donc :

```text
EquipmentOutputRuntimeAdapter
  -> RelaisManagerBackend
  -> RelaisManager
```

avec fallback direct `RelaisManager` toujours conservé.

## 6. Pourquoi le registre de drivers n’est pas encore raccordé

Le Run 6.2 ne doit pas inventer ni supposer l’API du registre Phase 3.

Le raccord réel au registre et la résolution :

```text
zone -> RelayAssignment -> driver -> port
```

sont réservés au Run 6.3, après inspection précise des fichiers et contrats existants.

## 7. Invariants préservés

1. Aucune zone migrée.
2. Aucun driver V4 appelé.
3. Aucun changement NVS.
4. Aucun changement Web.
5. Aucun changement LCD.
6. Aucun changement JSON.
7. Aucun changement `main.cpp`.
8. `RelaisManagerBackend` reste le backend actif.
9. Le fallback direct `RelaisManager` reste présent.
10. Compilation PlatformIO requise avant activation ultérieure.

## 8. Validation attendue

```powershell
pio run -e ProgrammeArrosage
```

Le test matériel attendu est une simple non-régression, car le nouveau backend n’est pas instancié.

## 9. Suite recommandée

```text
AquaLook V4 — Phase 6 — Run 6.3
Résolution RelayAssignment vers registre de drivers et port physique,
sans activation de zone
```

Le Run 6.3 devra inspecter et utiliser les contrats Phase 3 réels, sans approximation.
