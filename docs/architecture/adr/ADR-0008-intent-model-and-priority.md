# ADR-0008 — Modèle Intent et priorité

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.4

## Contexte

Un automatisme, une commande manuelle, une règle de sécurité ou une opération de reprise ne doit pas commander directement un équipement.

Ces sources expriment une intention. L’intention doit pouvoir être validée, comparée, refusée, expirée ou remplacée avant toute exécution.

## Décision

Le modèle introduit `EquipmentIntent` comme objet runtime compact et indépendant de l’exécution.

Il contient :

```text
IntentId
EquipmentId cible
CorrelationId
origine et identifiant de source
état demandé
création monotone
fin de validité monotone
priorité
statut
flags
révision
motif de refus
```

Sa taille est fixée à 32 octets.

## Origines

```text
AUTOMATION
MANUAL
SAFETY
RECOVERY
API
SYSTEM
```

`sourceId` est un identifiant numérique propre à l’origine : AutomationId, identifiant de session, règle de sécurité ou autre référence compacte.

La sémantique exacte de `sourceId` dépend de `IntentOrigin` et sera validée par la couche qui construit l’intention.

## Priorités

Les niveaux initiaux sont codés sur 8 bits :

```text
BACKGROUND = 0
NORMAL     = 64
HIGH       = 128
SAFETY     = 192
EMERGENCY  = 255
```

Les intervalles laissés libres permettent d’introduire des priorités intermédiaires sans changer le format.

## Arbitrage déterministe

`outranks(candidate, current)` applique :

1. priorité numérique la plus élevée ;
2. à priorité égale, intention la plus récente ;
3. à date égale, `IntentId` le plus élevé comme départage déterministe.

Cette fonction est une primitive de classement. Elle ne remplace pas les contrôles de mode, capacités, dépendances, interlocks et défauts bloquants.

## Validité temporelle

`validUntilMs == 0` signifie qu’aucune expiration automatique n’est déclarée.

Sinon, la date de fin doit être strictement postérieure à la date de création dans l’arithmétique monotone 32 bits.

`isIntentExpired()` utilise une comparaison signée de la différence afin de rester compatible avec le rebouclage de `millis()`.

Une fenêtre ne doit pas dépasser la demi-plage d’un compteur 32 bits, soit environ 24,8 jours. Les intentions de longue durée devront être renouvelées ou représentées par une configuration durable, pas par une intention runtime.

## Statuts

```text
PENDING
ACCEPTED
REJECTED
SUPERSEDED
EXPIRED
CANCELLED
```

Le statut décrit le traitement de l’intention, pas l’état physique de l’équipement.

Une intention acceptée produit plus tard une exécution distincte.

## Motifs de refus

```text
INVALID_ID
INVALID_TARGET
INVALID_STATE
INVALID_ORIGIN
EXPIRED
EQUIPMENT_DISABLED
LOWER_PRIORITY
INTERLOCKED
DEPENDENCY_UNAVAILABLE
BLOCKING_FAULT
CAPABILITY_NOT_SUPPORTED
CONFLICT
INTERNAL_ERROR
```

Un rejet avec raison `NONE` est transformé en `INTERNAL_ERROR` afin d’éviter un refus sans explication structurée.

## Corrélation

`CorrelationId` regroupe plusieurs intentions appartenant à la même action logique.

Exemples :

```text
ouvrir une zone + démarrer une pompe
arrêter plusieurs équipements après un défaut
séquence d’ouverture de serre
```

La corrélation n’est pas une exécution et n’impose pas que toutes les intentions soient acceptées.

## Flags

```text
REPLACEABLE
REQUIRES_OBSERVATION
STICKY
```

- `REPLACEABLE` indique qu’une intention concurrente peut la remplacer selon la politique d’arbitrage ;
- `REQUIRES_OBSERVATION` demande une confirmation observée avant résultat final ;
- `STICKY` indique que l’intention reste pertinente jusqu’à annulation explicite ou expiration définie.

Ces flags sont déclaratifs. Leur politique complète appartient aux futurs arbitre et orchestrateur.

## Options rejetées

### Commander directement depuis ScheduleManager

Rejeté : mélange décision temporelle, arbitrage et matériel.

### Utiliser uniquement une priorité sans origine

Rejeté : diagnostics et politiques impossibles à expliquer.

### Une date civile d’expiration

Rejetée pour le runtime : dépendance NTP et risques lors des corrections d’heure.

### Stocker un callback dans l’intention

Rejeté : non sérialisable, couplage fort et cycle de vie dangereux.

## Conséquences

- toutes les sources futures convergent vers un contrat commun ;
- l’arbitrage peut rester indépendant du matériel ;
- une intention acceptée ne signifie pas une opération réussie ;
- l’exécution et son résultat restent des objets séparés ;
- les files futures devront être bornées ;
- les politiques de remplacement et de corrélation restent à détailler.

## Invariants

1. Une intention ne commande jamais directement le matériel.
2. Une intention cible un `EquipmentId`, jamais une carte ou un port.
3. Le statut d’intention ne remplace pas `EquipmentRuntimeState`.
4. L’expiration utilise un temps monotone.
5. Une intention acceptée doit encore être transformée en exécution.
6. Aucun `String`, pointeur ou allocation dynamique n’est contenu dans le modèle.
