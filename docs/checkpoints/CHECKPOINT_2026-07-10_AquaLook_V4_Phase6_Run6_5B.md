# CHECKPOINT — AquaLook V4 — Phase 6 — Run 6.5B

Date : 2026-07-10
Branche : `feature/aqualook-v4-domain`
Dépôt : `cnuma/AquaLook`
HEAD attendu après ce checkpoint : commit du présent fichier, basé sur `f33290e1612e366a8c6441941193d2e119f40611`

## 1. Source de vérité

La source de vérité est le dépôt GitHub sur la branche `feature/aqualook-v4-domain`.

Base fonctionnelle validée avant le Run 6.5B :

- compilation et fonctionnement legacy validés ;
- `RelaisManager` reste le backend historique de référence ;
- l’image de sortie XL9535 est centralisée via `Xl9535SharedOutputState` ;
- plusieurs zones peuvent être activées sans écrasement mutuel ;
- NVS, Web, LCD et JSON n’ont pas été modifiés par les Runs 6.4 à 6.5B.

## 2. État validé avant reprise

### Run 6.4 — Profils compile-time

Environnements PlatformIO disponibles :

- `ProgrammeArrosage` : profil nominal legacy ;
- `ProgrammeArrosage_legacy` : profil legacy explicite ;
- `ProgrammeArrosage_v4` : profil V4 explicite.

Macros attendues :

```ini
AQUALOOK_RELAY_BACKEND_LEGACY=1
AQUALOOK_RELAY_BACKEND_V4=0
```

pour le profil nominal/legacy, et :

```ini
AQUALOOK_RELAY_BACKEND_LEGACY=0
AQUALOOK_RELAY_BACKEND_V4=1
```

pour `ProgrammeArrosage_v4`.

Compilation validée avant le Run 6.5B :

```text
ProgrammeArrosage_legacy  SUCCESS
RAM   : 67 416 octets — 20,6 %
Flash : 1 273 141 octets — 62,7 %
```

```text
ProgrammeArrosage_v4  SUCCESS
RAM   : 67 416 octets — 20,6 %
Flash : 1 273 133 octets — 62,7 %
```

### Run 6.5A — Propriété partagée du registre XL9535

Problème identifié : `RelaisManager` et le driver XL9535 V4 maintenaient chacun leur propre image du registre de sortie 16 bits. Une commande V4 pouvait donc réécrire une image obsolète et écraser l’état d’une autre voie.

Solution mise en place :

- ajout de `src/domain/Xl9535SharedOutputState.h/.cpp` ;
- une seule image 16 bits est maintenue par adresse I2C ;
- `RelaisManager` utilise cette image partagée ;
- le driver V4 XL9535 peut utiliser la même image ;
- aucune zone V4 n’était encore migrée à ce stade.

Validation matérielle utilisateur :

```text
pas de problème entre les activations de zones
```

Mesures du profil nominal après raccord :

```text
RAM   : 67 456 octets — 20,6 %
Flash : 1 273 745 octets — 62,7 %
```

## 3. Run 6.5B — Objectif

Activer uniquement la zone pilote 0, affichée comme zone 1, via le backend V4 dans le profil `ProgrammeArrosage_v4`.

Invariants :

1. Le profil `ProgrammeArrosage` reste intégralement legacy.
2. Seule la zone 0 est marquée migrée dans le profil V4.
3. Toutes les autres zones restent legacy.
4. Toute erreur V4 provoque un fallback immédiat vers `RelaisManager`.
5. La sécurité de durée maximale reste assurée par `RelaisManager`.
6. Aucun changement NVS, Web, LCD ou JSON.
7. Retour arrière immédiat possible en reflashant `ProgrammeArrosage`.

## 4. Architecture introduite pour le pilote

### Fichiers ajoutés

- `src/V4PilotRuntime.h`
- `src/V4PilotRuntime.cpp`
- `docs/architecture/AQUALOOK_V4_PHASE6_RUN6_5B_ZONE0_PILOT_ACTIVATION.md`

### Fichiers modifiés

- `src/V4RelayPhysicalBackend.h`
- `src/RelaisManager.h`
- `src/EquipmentOutputRuntimeAdapter.cpp`
- `src/main.cpp`
- correctifs ultérieurs dans `src/RelaisManager.h` et `src/V4PilotRuntime.cpp`

### `V4PilotRuntime`

Le runtime pilote construit un inventaire matériel minimal correspondant à la carte relais 0 :

- un contrôleur XL9535 ;
- une carte relais ;
- les ports binaires nécessaires ;
- un registre de drivers de capacité 1 ;
- un contexte `Xl9535BinaryActuatorContext` raccordé à `Wire` ;
- le même `Xl9535SharedOutputState` que le backend historique.

Le masque de migration attendu est :

```cpp
1UL << 0U
```

Donc seule la zone 0 est migrée.

### Fallback

Dans `EquipmentOutputRuntimeAdapter::setZoneValve()` :

1. le backend physique V4 est essayé en premier ;
2. s’il retourne `false`, `RelaisManager::setRelay()` exécute immédiatement la commande ;
3. si V4 réussit, l’état logique de `RelaisManager` est synchronisé sans réécriture physique via `mirrorZoneState()`.

Cela permet au watchdog historique de couper la zone après la durée maximale.

## 5. Journal attendu

Profil V4 prêt :

```text
Relais V4: zone pilote 1 active, fallback legacy conserve
```

Initialisation V4 impossible :

```text
Relais V4: pilote indisponible, backend legacy force
```

Profil nominal :

```text
Relais: profil backend legacy
```

## 6. Correctifs de compilation déjà appliqués

### Collision Arduino `DISABLED`

Le domaine utilise des enums contenant `DISABLED`, mais Arduino expose une macro du même nom dans certains contextes.

Correctif : inclure les headers du domaine avant `RelayTopology.h` et avant Arduino lorsque nécessaire.

### Double définition du setter

Erreur observée :

```text
redefinition of 'void RelaisManager::setXl9535SharedOutputState(...)'
```

Cause : définition inline dans `RelaisManager.h` et définition dans `RelaisManager.cpp`.

Correctif commit :

```text
c8439507af0544c36e70b65b2831f97b53de73c6
fix: remove duplicate shared XL9535 setter definition
```

Le header ne contient plus que la déclaration.

### Collision Arduino `OUTPUT`

Dernière erreur observée lors de :

```powershell
pio run -e ProgrammeArrosage_v4 -t upload
```

Erreur :

```text
#define OUTPUT 0x03
Domain::PortDirection::OUTPUT
```

Le préprocesseur Arduino remplaçait le token `OUTPUT` même dans le nom qualifié de l’enum.

Correctif appliqué dans `src/V4PilotRuntime.cpp` :

```cpp
port.direction = static_cast<Domain::PortDirection>(2U);
```

Commit :

```text
f33290e1612e366a8c6441941193d2e119f40611
fix: avoid Arduino OUTPUT macro collision in V4 pilot
```

## 7. Statut exact à la fermeture de ce chat

- Le Run 6.5A est validé matériellement.
- Le Run 6.5B est implémenté.
- La compilation V4 a échoué une première fois sur la double définition du setter : corrigé.
- La compilation V4 a échoué une seconde fois sur la macro Arduino `OUTPUT` : corrigé par le commit `f33290e...`.
- La compilation et le téléversement après ce dernier correctif ne sont pas encore validés.
- Aucun test matériel de la zone pilote V4 n’a encore été effectué.

## 8. Commande de reprise immédiate

Dans PowerShell :

```powershell
git pull --ff-only
pio run -e ProgrammeArrosage_v4 -t upload
if ($LASTEXITCODE -eq 0) {
    pio device monitor -e ProgrammeArrosage_v4
}
```

Ne pas utiliser `&&` dans le PowerShell actuel de l’utilisateur.

## 9. Tests matériels à effectuer après compilation réussie

1. Vérifier le message de boot indiquant que la zone pilote 1 est active en V4.
2. Activer puis arrêter la zone 1.
3. Activer simultanément la zone 1 et une autre zone.
4. Vérifier que l’arrêt de l’une ne modifie pas l’autre.
5. Vérifier les états Web et LCD.
6. Vérifier l’absence d’activation intempestive au boot.
7. Vérifier le fallback en cas d’échec V4 si un moyen sûr de le provoquer est défini.
8. Vérifier la coupure de sécurité de durée maximale.

## 10. Retour arrière

À tout moment :

```powershell
pio run -e ProgrammeArrosage -t upload
if ($LASTEXITCODE -eq 0) {
    pio device monitor -e ProgrammeArrosage
}
```

Le profil nominal reste legacy.

## 11. Commits structurants de la séquence

```text
2f197666...  stratégie de bascule backend Phase 6
c2f0f756...  profils compile-time legacy/V4
61468320...  correction collision DISABLED du Run 6.3
84b5223e...  blocage activation partielle XL9535 non sûre
e8615bb1...  ajout propriétaire partagé XL9535
e7c30c08...  implémentation propriétaire partagé
25b26111...  driver XL9535 utilisant l’image partagée
3b4a9c5c...  documentation raccord partagé
def12953...  scaffold runtime pilote V4
dc2c051f...  configuration runtime pilote zone 0
c325a276...  masque contrôlé des zones migrées
0d88bc63...  synchronisation watchdog
_df3b2814...  fallback et watchdog dans l’adaptateur
a368ef00...  activation compile-time du pilote dans main.cpp
66cb0a1b...  ordre d’includes contre DISABLED
74433e4e...  include runtime pilote avant Arduino
c8439507...  suppression double définition du setter
f33290e1...  correction collision Arduino OUTPUT
```

## 12. Consigne pour le nouveau chat

Commencer par vérifier que la branche locale est bien `feature/aqualook-v4-domain` et que `git pull --ff-only` récupère le checkpoint et le correctif `f33290e...`.

Ne pas poursuivre vers une migration supplémentaire tant que :

- `ProgrammeArrosage_v4` ne compile pas ;
- le firmware V4 n’est pas téléversé ;
- la zone pilote 1 n’est pas validée matériellement ;
- le fonctionnement simultané avec une autre zone n’est pas validé ;
- le retour arrière nominal n’est pas confirmé.
