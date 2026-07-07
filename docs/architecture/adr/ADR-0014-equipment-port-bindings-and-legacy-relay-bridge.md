# ADR-0014 — Binding Equipment vers ports et passerelle relais historique

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 2 — Run 2.3

## Contexte

Le domaine V4 manipule des `EquipmentId`. L’inventaire matériel expose des `PortId`. Une relation stable est nécessaire entre les deux sans faire connaître les cartes, canaux ou drivers aux équipements métier.

Le firmware actuel utilise encore `RelayTopology::RelayAssignment` avec :

```text
role
targetIndex
boardIndex
channelIndex
```

La transition doit rester possible sans inclure `RelayTopology.h` dans le domaine V4, car ce fichier dépend d’Arduino et de `config.h`.

## Décision

Le modèle introduit `EquipmentPortBinding` :

```text
EquipmentId
PortId
requiredPortCapabilities
kind
flags
revision
```

Sa taille est verrouillée à 16 octets.

## Types de binding

```text
PRIMARY_ACTUATOR
SECONDARY_ACTUATOR
OBSERVER
SAFETY_INPUT
```

Une vanne ou une pompe utilisant un relais possède typiquement un `PRIMARY_ACTUATOR` exigeant `PORT_CAP_RELAY_OUTPUT`.

Un capteur de position ou un retour d’état utilise `OBSERVER` ou `SAFETY_INPUT`.

## Validation

Le validateur contrôle :

- identifiants valides ;
- existence de l’équipement ;
- existence du port ;
- type de binding connu ;
- capacité demandée non vide ;
- capacité présente sur le port ;
- compatibilité avec les capacités de l’équipement ;
- cohérence avec la direction du port ;
- absence de doublon ;
- un seul actionneur primaire par équipement ;
- absence de collision de port sauf partage explicite.

## Partage

Un port ne peut servir plusieurs bindings que si `BINDING_FLAG_SHARED_PORT` est déclaré explicitement.

Cette possibilité ne vaut pas autorisation générale : les politiques runtime devront encore vérifier que le partage est pertinent.

## Passerelle historique

Le domaine expose une vue neutre :

```text
LegacyRelayReference
```

Elle reprend les cinq valeurs utiles du `RelayAssignment` historique sans inclure son type C++.

Deux tables traduisent les anciennes coordonnées :

```text
(role, targetIndex)       -> EquipmentId
(boardIndex, channelIndex)-> PortId
```

La résolution produit un `EquipmentPortBinding` de type `PRIMARY_ACTUATOR` avec exigence `RELAY_OUTPUT`.

## Alignement des rôles

Les valeurs historiques restent :

```text
1 zone valve
2 pump
3 auxiliary
4 greenhouse vent
5 lighting
```

Cette correspondance n’est qu’une passerelle de migration. Les nouveaux composants ne doivent pas utiliser ces nombres comme identité principale.

## Options rejetées

### Inclure directement RelayTopology.h

Rejeté : dépendance Arduino et couplage au runtime historique.

### Stocker boardIndex et channelIndex dans Equipment

Rejeté : couplage métier-matériel et absence d’identité stable.

### Autoriser plusieurs actionneurs primaires sans règle

Rejeté : ambiguïté de commande.

### Déduire automatiquement toutes les capacités

Rejeté : certaines cartes exposent plusieurs modes sur un même canal. Le binding doit déclarer son besoin explicite.

## Conséquences

- les équipements peuvent cibler des ports génériques ;
- la topologie relais actuelle peut être traduite progressivement ;
- aucun manager historique n’est modifié ;
- une future migration pourra construire les tables de correspondance depuis la configuration candidate ;
- la couche actionneur utilisera le binding plutôt que les coordonnées de carte.

## Invariants

1. Un binding relie un `EquipmentId` à un `PortId`.
2. Les capacités demandées doivent être disponibles sur le port.
3. Un équipement ne possède qu’un actionneur primaire sauf évolution explicitement décidée.
4. Le domaine V4 n’inclut pas `RelayTopology.h`.
5. La passerelle historique ne commande aucun relais.
6. Le runtime actuel reste inchangé.
