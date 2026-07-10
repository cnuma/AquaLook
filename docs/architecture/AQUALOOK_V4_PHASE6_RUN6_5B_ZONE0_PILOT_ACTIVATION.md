# AquaLook V4 — Phase 6 — Run 6.5B

## Objet

Activer uniquement la zone pilote 0 via le backend V4 dans le profil `ProgrammeArrosage_v4`, tout en conservant un fallback immédiat vers `RelaisManager`.

## Périmètre

- zone migrée : index 0 uniquement ;
- autres zones : backend historique ;
- profil nominal `ProgrammeArrosage` : backend historique uniquement ;
- profil `ProgrammeArrosage_v4` : tentative V4 sur zone 0, fallback historique en cas d'échec ;
- image XL9535 : propriétaire partagé introduit au Run 6.5A.

## Runtime pilote

`V4PilotRuntime` construit un inventaire minimal à partir de la carte 0 de `RelayTopology` :

- un contrôleur XL9535 ;
- une carte relais ;
- les ports relais correspondants ;
- un registre de drivers contenant le driver XL9535 ;
- un `V4RelayPhysicalBackend` avec le masque de migration `1 << 0`.

Le runtime refuse de s'activer si la carte 0 n'est pas valide, n'est pas un XL9535 ou présente un nombre de voies incohérent.

## Fallback

`EquipmentOutputRuntimeAdapter` tente le backend physique V4 en premier. Si la commande retourne `false`, la même commande est immédiatement appliquée par `RelaisManager`.

Quand la commande V4 réussit, l'état logique de `RelaisManager` est mis à jour sans réécriture matérielle afin de préserver :

- l'état exposé aux composants historiques ;
- le suivi du temps d'activation ;
- la coupure de sécurité par durée maximale.

## Journal de démarrage

Profil V4 prêt :

`Relais V4: zone pilote 1 active, fallback legacy conserve`

Profil V4 non prêt :

`Relais V4: pilote indisponible, backend legacy force`

Profil nominal :

`Relais: profil backend legacy`

## Validation attendue

1. compiler `ProgrammeArrosage_v4` ;
2. téléverser explicitement `ProgrammeArrosage_v4` ;
3. vérifier le journal de démarrage ;
4. activer et arrêter la zone 1 ;
5. activer une autre zone et vérifier son maintien ;
6. vérifier Web et LCD ;
7. confirmer qu'aucune activation inattendue n'apparaît.

## Retour arrière

Téléverser le profil nominal :

`pio run -e ProgrammeArrosage -t upload`

Ce profil ne raccorde pas le runtime pilote V4.
