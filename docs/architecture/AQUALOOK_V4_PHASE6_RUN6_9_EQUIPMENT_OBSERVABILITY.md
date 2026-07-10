# AquaLook V4 — Phase 6 — Run 6.9

## Objet

Ajouter une observabilité explicite au chemin de commande des équipements sans modifier la logique d’exécution, la persistance ou l’orchestration métier.

## Base

- branche : `feature/aqualook-v4-domain`
- base matérielle validée : `0cdf42b70479df6bbffbf48271a1ae36eb9bd2f9`
- profil matériel validé : `ProgrammeArrosage_v4`

## Périmètre

Le Run 6.9 ajoute :

- des compteurs cumulés par chemin d’exécution ;
- le dernier chemin d’exécution ;
- un journal unique pour chaque commande de vanne ;
- un journal explicite pour les échecs de cible ou de dépendance.

Aucune modification de :

- `ConfigManager` ;
- NVS ;
- `ScheduleManager` ;
- Web ;
- LCD ;
- `RelayTopology` ;
- orchestration de pompe ;
- masque de migration V4.

## Chemins observés

Les chemins possibles sont :

```text
physical_backend
relay_manager_fallback
failed
```

Le chemin `physical_backend` correspond au backend V4 lorsqu’il accepte la commande.

Le chemin `relay_manager_fallback` correspond au repli par `RelaisManager` lorsque le backend physique ne prend pas en charge la cible.

## Compteurs

`EquipmentOutputRuntimeAdapter::ExecutionCounters` expose trois compteurs RAM :

```text
physicalBackend
relayManagerFallback
failed
```

Ces compteurs :

- démarrent à zéro à chaque boot ;
- ne sont pas persistés ;
- sont incrémentés une seule fois par tentative terminale ;
- ne changent pas le résultat de la commande.

## Journal attendu

Exemple zone pilote V4 :

```text
Equipment: zone 1 ON path=physical_backend exec=1 totals=1/0/0
```

Exemple zone fallback legacy :

```text
Equipment: zone 2 ON path=relay_manager_fallback exec=2 totals=1/1/0
```

Exemple échec :

```text
Equipment: zone 17 ON path=failed error=invalid_target
```

L’ordre des totaux est :

```text
physical_backend / relay_manager_fallback / failed
```

## Validation attendue

1. compiler `ProgrammeArrosage_legacy` ;
2. compiler `ProgrammeArrosage_v4` ;
3. téléverser `ProgrammeArrosage_v4` ;
4. activer la zone 1 ;
5. vérifier `path=physical_backend` ;
6. activer une autre zone ;
7. vérifier `path=relay_manager_fallback` ;
8. arrêter les mêmes zones ;
9. vérifier l’incrément cohérent des compteurs ;
10. confirmer l’absence de changement fonctionnel.

## Retour arrière

Le retour au commit `0cdf42b70479df6bbffbf48271a1ae36eb9bd2f9` supprime uniquement cette télémétrie et conserve le raccordement runtime du Run 6.8.
