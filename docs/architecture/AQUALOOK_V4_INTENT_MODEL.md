# AquaLook V4 — Modèle Intent

**Statut :** référence de Phase 1  
**Date :** 7 juillet 2026  
**Run :** Phase 1 — Run 1.4

## 1. Position dans l’architecture

```text
Automation / Manual / Safety / API / Recovery
        ↓ produit
EquipmentIntent
        ↓ validé et arbitré
IntentArbiter futur
        ↓ accepté
Execution future
        ↓ agit via
Actionneur
```

Une intention exprime une volonté. Elle ne prouve ni autorisation, ni application, ni observation.

## 2. Fichiers source

```text
src/domain/DomainIdentifiers.h
src/domain/IntentModel.h
src/domain/IntentModel.cpp
```

`DomainIdentifiers.h` reçoit deux nouveaux types forts :

```text
IntentId
CorrelationId
```

## 3. Structure

`EquipmentIntent` occupe exactement 32 octets :

```text
requestedState       8 octets
createdAtMs          4 octets
validUntilMs         4 octets
source               4 octets
IntentId             2 octets
EquipmentId          2 octets
CorrelationId        2 octets
rejectionReason      2 octets
priority             1 octet
status               1 octet
flags                1 octet
revision             1 octet
```

L’ordre des champs est volontairement optimisé pour éviter les octets de bourrage. Une première disposition testée produisait 36 octets ; elle a été corrigée avant validation.

## 4. Source

`IntentSourceRef` occupe 4 octets :

```text
IntentOrigin origin
uint8 reserved
uint16 sourceId
```

Le couple origine/source permet d’expliquer la provenance sans pointer vers un objet C++.

## 5. État demandé

L’intention réutilise `EquipmentStateValue` du Run 1.3.

La valeur doit être :

- d’un type différent de `UNKNOWN` ;
- marquée `VALID` ;
- compatible ultérieurement avec le type et les capacités de l’équipement.

La compatibilité métier complète n’est pas encore réalisée dans ce run.

## 6. Priorité et départage

Ordre initial :

```text
EMERGENCY
SAFETY
HIGH
NORMAL
BACKGROUND
```

À priorité égale :

```text
plus récente
puis IntentId le plus élevé
```

Ce départage rend le résultat stable et testable.

## 7. Durée de validité

```text
validUntilMs = 0
```

signifie aucune expiration automatique.

Sinon :

```text
createdAtMs < validUntilMs
```

est évalué avec l’arithmétique monotone compatible avec le rebouclage de `millis()`.

La durée maximale sûre est inférieure à `2^31` millisecondes, environ 24,8 jours.

## 8. Cycle de statut

Transitions primitives disponibles :

```text
PENDING -> ACCEPTED
PENDING -> REJECTED
PENDING/ACCEPTED -> SUPERSEDED
PENDING/ACCEPTED -> EXPIRED
PENDING/ACCEPTED -> CANCELLED
```

Fonctions :

```text
acceptIntent()
rejectIntent()
supersedeIntent()
expireIntent()
cancelIntent()
```

Chaque transition incrémente la révision 8 bits.

Le contrôle strict des transitions autorisées sera ajouté avec l’arbitre ou la file d’intentions.

## 9. Validation

`validateIntent()` vérifie :

- `IntentId` valide ;
- cible valide ;
- origine connue ;
- état demandé exploitable ;
- statut connu ;
- fenêtre temporelle valide.

Elle ne vérifie pas encore :

- existence de la cible dans la configuration active ;
- capacité de l’équipement ;
- mode ;
- dépendances ;
- défauts bloquants ;
- conflits avec d’autres intentions.

Ces contrôles appartiennent au futur arbitre.

## 10. Test hôte

Compilation :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Cas validés :

- intention normale valide ;
- expiration exacte à l’échéance ;
- priorité haute devant priorité normale ;
- départage par récence ;
- acceptation et rejet structuré ;
- fenêtre traversant le rebouclage de `millis()` ;
- taille exacte de 32 octets.

Résultat :

```text
Compilation hôte OK
EquipmentIntent = 32 octets
IntentSourceRef = 4 octets
```

## 11. Hors périmètre

- file bornée d’intentions ;
- arbitre complet ;
- politique de remplacement ;
- création d’exécution ;
- dépendances ;
- liaison au planning actuel ;
- API Web ;
- persistance ;
- matériel.

## 12. Prochaine étape

Le Run 1.5 devra définir le modèle `Execution` : cycle de vie, étapes, délais, annulation, compensation et association entre une intention acceptée et une opération suivie.
