# AquaLook V4 — Phase 6 — Run 6.8

## Objet

Raccorder `EquipmentManager` au runtime d'arrosage en conservant un modèle transitoire en RAM et un fallback direct vers `EquipmentOutputRuntimeAdapter`.

## Périmètre

- construction au boot d'un modèle équipement pour les vannes de zones ;
- aucune pompe instanciée ;
- aucune persistance NVS ;
- aucune modification du planning, du Web ou du LCD ;
- aucune modification du masque de migration V4 ;
- fallback direct vers l'adaptateur si le modèle ou le manager n'est pas disponible.

## Construction du modèle transitoire

Pour chaque zone configurée :

1. rechercher l'affectation `ROLE_ZONE_VALVE` correspondante dans `RelayTopology` ;
2. créer une entrée `EquipmentConfig` de type `EQUIP_ZONE_VALVE` ;
3. conserver l'index réel de `RelayAssignment` ;
4. créer un `ZoneEquipmentLink` sans pompe ;
5. valider l'équipement et le lien.

Le modèle est reconstruit à chaque démarrage et n'est pas écrit dans `ConfigManager` ou la NVS.

## Chaîne d'exécution

```text
ScheduleManager
    -> onRelayRequest(zone, state)
        -> EquipmentManager::startZone() / stopZone()
            -> résolution zone -> équipement -> RelayAssignment
            -> EquipmentOutputRuntimeAdapter::setZoneValve()
                -> backend V4 si zone migrée
                -> fallback RelaisManager sinon
```

Si `EquipmentManager` retourne une erreur, `onRelayRequest()` appelle directement :

```text
EquipmentOutputRuntimeAdapter::setZoneValve()
```

Le comportement historique reste donc disponible même en cas d'échec du modèle transitoire.

## Profils

### ProgrammeArrosage_legacy

- `EquipmentManager` est actif ;
- l'adaptateur pointe sur `RelaisManagerBackend` ;
- le comportement physique reste legacy.

### ProgrammeArrosage_v4

- `EquipmentManager` est actif ;
- l'adaptateur tente le pilote V4 pour la zone 0 ;
- les autres zones utilisent le fallback historique ;
- si le pilote V4 n'est pas prêt, le backend legacy est forcé.

## Journaux attendus

Modèle prêt :

```text
Equipment: modele transitoire pret pour N zone(s)
```

Modèle indisponible :

```text
Equipment: modele indisponible, fallback adaptateur direct
```

Échec ponctuel du manager :

```text
Equipment: zone N echec=X, fallback adaptateur
```

## Invariants

- pas de modification de la NVS ;
- pas d'orchestration de pompe ;
- pas de délai bloquant ajouté ;
- sécurité de durée maximale toujours portée par `RelaisManager` ;
- la zone pilote V4 reste la zone 0 uniquement ;
- aucune activation relais au moment de la construction du modèle ;
- toutes les commandes gardent un chemin de repli.

## Validation

1. compiler `ProgrammeArrosage_legacy` ;
2. compiler `ProgrammeArrosage_v4` ;
3. téléverser `ProgrammeArrosage_v4` ;
4. vérifier le journal `Equipment: modele transitoire pret...` ;
5. tester zone 1 puis une autre zone ;
6. vérifier les démarrages et arrêts manuels ;
7. vérifier qu'aucune activation inattendue n'apparaît au boot.

## Retour arrière

Le retour arrière immédiat reste le profil :

```powershell
pio run -e ProgrammeArrosage -t upload
```

Le profil nominal conserve le backend legacy.
