# AquaLook V4 — Phase 6 — Run 6.6

## Objet

Consolider le modèle de domaine `Equipment` sans modifier le comportement d'arrosage.

Le Run 6.6 reste strictement isolé de :

- la NVS et `ConfigManager` ;
- `ScheduleManager` ;
- `RelaisManager` et les backends physiques ;
- l'interface Web ;
- l'affichage LCD ;
- la logique de démarrage et d'arrêt des zones.

## Base

Branche : `feature/aqualook-v4-domain`

Base de départ du Run 6.6 :

```text
ba2b0693dc15faa2abe4f16b27bcd331264833e8
```

Le Run 6.5B est validé matériellement : la zone pilote 0 utilise le backend V4 et les autres zones restent fonctionnelles via le backend historique.

## Modèle préexistant conservé

`EquipmentModel` fournit déjà :

- `EquipmentId` et `EquipmentTypeId` fortement typés ;
- un masque de capacités ;
- un mode d'exploitation ;
- un état sûr ;
- des références compactes vers une arène bornée ;
- un descripteur de type ;
- une validation sans allocation dynamique.

Cette architecture est conservée. Le Run 6.6 ne remplace pas ce modèle par une structure persistée ou par des chaînes fixes embarquées dans chaque équipement.

## Catalogue AquaLook ajouté

Fichiers :

- `src/domain/EquipmentCatalog.h`
- `src/domain/EquipmentCatalog.cpp`

Types officiels :

1. vanne de zone ;
2. pompe ;
3. contact auxiliaire ;
4. aération de serre ;
5. éclairage ;
6. brumisateur ;
7. ventilateur.

Chaque type possède :

- un `EquipmentTypeId` stable ;
- un nom technique ;
- des capacités obligatoires ;
- des capacités optionnelles autorisées ;
- une version de schéma de paramètres ;
- une taille minimale et maximale de paramètres.

## Paramètres compacts

### Sortie binaire générique

`BinaryOutputParameters` contient :

- la référence de sortie logique ;
- le temps minimum d'activation ;
- le temps minimum d'arrêt.

### Vanne de zone

`ZoneValveParameters` contient :

- la sortie `ZONE_VALVE` ;
- l'index de pompe potentiellement associée ;
- un drapeau indiquant si une pompe est requise.

La dépendance n'est pas exécutée dans ce run.

### Pompe

`PumpParameters` contient :

- la sortie `PUMP` ;
- le délai de démarrage ;
- le délai d'arrêt ;
- les durées minimales ON et OFF.

Aucune pompe n'est créée ni commandée dans ce run.

## Validation d'inventaire

`validateEquipmentInventory()` vérifie :

- la présence de la collection lorsque le nombre est non nul ;
- l'existence du type dans le catalogue ;
- la validité de chaque équipement avec son descripteur ;
- l'unicité des `EquipmentId`.

La fonction s'arrête sur la première erreur et retourne l'index, l'identifiant et le détail de validation.

## Invariants

1. Aucun changement du format NVS.
2. Aucun équipement construit au démarrage.
3. Aucun changement de callback du planning.
4. Aucun changement de routage relais.
5. Aucun changement Web ou LCD.
6. Aucune allocation dynamique.
7. Les structures de paramètres restent compactes et contrôlées par `static_assert`.
8. Les identifiants de type sont stables et ne doivent pas être réutilisés.

## Validation attendue

Compiler les deux profils :

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4
```

Aucun téléversement matériel n'est requis pour ce run purement domaine. Un smoke test du profil V4 reste recommandé avant le prochain raccord runtime.

## Suite prévue

Le Run 6.7 pourra introduire un `EquipmentManager` squelette :

- inventaire en RAM construit au démarrage ;
- API interne `startZone()` / `stopZone()` ;
- délégation immédiate vers l'adaptateur actuel ;
- aucun séquencement de pompe au premier palier ;
- fallback historique conservé.
