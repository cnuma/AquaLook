# CHECKPOINT — AquaLook V4 — Phase 4 — Runs 4.11 à 4.13

**Date :** 10 juillet 2026  
**Branche :** `feature/aqualook-v4-domain`  
**Statut :** compilation et validation fonctionnelle utilisateur réussies

## Source de vérité

Dépôt : `cnuma/AquaLook`  
Branche : `feature/aqualook-v4-domain`  
HEAD avant création de ce checkpoint :

```text
e3bc23908dac5e26b29c8a60fd70d785e922120d
```

## Objet du checkpoint

Cette séquence introduit et valide la frontière physique relais suivante :

```text
EquipmentOutputRuntimeAdapter
  -> RelayPhysicalBackend
  -> RelaisManagerBackend
  -> RelaisManager
```

Le fallback direct vers `RelaisManager` reste conservé dans `EquipmentOutputRuntimeAdapter`.

## Runs couverts

### Run 4.11

Ajout de :

```text
src/RelayPhysicalBackend.h
src/RelaisManagerBackend.h
src/RelaisManagerBackend.cpp
```

### Run 4.12

Modification de :

```text
src/EquipmentOutputRuntimeAdapter.h
src/EquipmentOutputRuntimeAdapter.cpp
```

Le backend physique optionnel est essayé en premier, puis fallback direct vers `RelaisManager`.

### Run 4.13

Modification de :

```text
src/main.cpp
```

Câblage :

```cpp
relaisBackend.bind(&relaisMgr);
outputAdapter.setPhysicalBackend(&relaisBackend);
outputAdapter.bind(&relaisMgr);
```

## Validation PlatformIO

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:03:37.415
```

Métriques :

```text
RAM:   20.6% — 67,416 / 327,680 octets
Flash: 62.7% — 1,273,005 / 2,031,616 octets
```

Capacité restante :

```text
RAM:   260,264 octets
Flash: 758,611 octets
```

Delta depuis Run 4.9 :

```text
RAM:   +16 octets
Flash: +300 octets
```

## Validation fonctionnelle utilisateur

Données `/api/status` observées le 10 juillet 2026 à 16:35:39 :

```text
synced: true
uptime: 30 s
heap: 87,352 octets
weather.fetched: true
zones remontées: 4
zones actives: 3
manualDurationMin: 1
```

États observés :

```text
Zone Jardin   active=false
Zone Terrasse active=true
Zone 3        active=true
Zone 4        active=true
```

Le champ `reason` indique `Manuel 1min` pour les zones observées.

Validation utilisateur :

```text
page web et lcd semblent bon
```

Conclusion :

- `/api/status` opérationnel ;
- états actifs remontés correctement ;
- page Web fonctionnelle ;
- LCD fonctionnel ;
- cohérence globale Web/LCD/runtime jugée correcte par l’utilisateur.

## Invariants préservés

1. Aucun changement NVS.
2. Aucun changement JSON.
3. Aucun changement Web fonctionnel.
4. Aucun changement LCD fonctionnel.
5. Aucun driver V4 réel activé.
6. `RelaisManager` reste le backend physique effectif.
7. Fallback direct `RelaisManager` conservé.
8. Fallbacks Web/LCD conservés.
9. Mapping historique des zones conservé.
10. `RelaisManager::update()` reste actif.

## Risques et limites

- La validation fonctionnelle reste utilisateur et non automatisée.
- Toutes les branches de fallback ne sont pas encore testées systématiquement.
- Aucun test HIL exhaustif n’existe encore.
- Le retrait des fallbacks est interdit à ce stade.

## Étape suivante recommandée

```text
AquaLook V4 — Phase 5 — Tests et validation automatisée
Run 5.1 — Stratégie et matrice de tests
```

Cette phase devra couvrir au minimum :

- backend présent/absent ;
- backend succès/échec ;
- fallback utilisé/non utilisé ;
- zone valide/invalide ;
- lecture valide/inconnue ;
- génération d’un rapport automatique.
