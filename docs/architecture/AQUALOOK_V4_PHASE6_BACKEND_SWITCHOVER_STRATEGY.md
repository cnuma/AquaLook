# AquaLook V4 — Phase 6 — Run 6.1 — Stratégie de bascule du backend physique

**Date :** 10 juillet 2026  
**Statut :** documentaire, aucune modification runtime  
**Base :** checkpoint validé des Runs 4.11 à 4.13

## 1. Contexte

La Phase 5 de tests automatisés est volontairement mise en pause.

La Phase 6 démarre donc avec un niveau de prudence renforcé. Les fallbacks existants deviennent des protections indispensables tant que les tests automatisés ne sont pas disponibles.

État actuel validé :

```text
ScheduleManager
  -> EquipmentOutputRuntimeAdapter
  -> RelayPhysicalBackend
  -> RelaisManagerBackend
  -> RelaisManager
```

Fallback direct conservé :

```text
EquipmentOutputRuntimeAdapter
  -> RelaisManager
```

Aucun driver V4 réel n’est encore actif dans le runtime.

## 2. Objectif de la Phase 6

La Phase 6 doit introduire progressivement un backend physique V4 capable de piloter les sorties via les drivers définis en Phase 3, sans basculement brutal et sans supprimer immédiatement le backend historique.

Objectifs :

- créer un backend V4 derrière `RelayPhysicalBackend` ;
- résoudre les affectations `RelayAssignment` vers un driver et un port ;
- activer le backend V4 sur un périmètre limité ;
- conserver un rollback immédiat ;
- journaliser toute utilisation du fallback ;
- étendre progressivement la migration après validation matérielle.

## 3. Backends concernés

### Backend historique

```text
RelaisManagerBackend
  -> RelaisManager
  -> écriture I2C historique
```

Rôle :

- backend stable de référence ;
- fallback principal ;
- solution de rollback ;
- backend actif pour toutes les zones non migrées.

### Backend V4 candidat

Nom provisoire :

```text
V4RelayPhysicalBackend
```

Chaîne cible :

```text
V4RelayPhysicalBackend
  -> RelayAssignment / RelayTopology
  -> BinaryActuatorDriverRegistry
  -> driver matériel V4
```

Le nom définitif devra être confirmé lors du Run 6.2 après inspection du code Phase 3.

## 4. Politique de priorité

Ordre recommandé pendant la migration :

```text
1. backend V4 si la zone est explicitement migrée et le backend prêt
2. backend historique RelaisManager si backend V4 indisponible ou en échec
3. rejet propre si aucun backend n’est disponible
```

Le backend V4 ne doit jamais être utilisé implicitement sur une zone non déclarée comme migrée.

## 5. Conditions d’utilisation du backend V4

Une commande peut être envoyée au backend V4 seulement si toutes les conditions suivantes sont vraies :

1. backend V4 initialisé ;
2. registre de drivers disponible ;
3. affectation `RelayAssignment` valide ;
4. type de driver supporté par le profil courant ;
5. port valide ;
6. zone explicitement autorisée pour migration ;
7. driver prêt ;
8. commande compatible avec le rôle de sortie.

Si une condition échoue, le backend V4 doit retourner un échec explicite et permettre le fallback historique.

## 6. Conditions de fallback

Le fallback vers `RelaisManager` doit être utilisé dans les cas suivants :

- backend V4 absent ;
- backend V4 non initialisé ;
- zone non migrée ;
- affectation absente ou invalide ;
- driver introuvable ;
- driver non prêt ;
- écriture physique refusée ;
- lecture d’état indisponible ;
- incohérence détectée entre commande et readback ;
- erreur de bus I2C ou autre transport ;
- protection compile-time désactivant le backend V4.

Le fallback ne doit pas masquer silencieusement l’erreur primaire.

## 7. Journalisation obligatoire

Chaque utilisation du fallback pendant la Phase 6 doit produire un événement identifiable.

Format logique recommandé :

```text
backend=v4
zone=2
operation=set
result=failed
reason=driver_unavailable
fallback=legacy
fallback_result=success
```

Niveaux recommandés :

- `LOG_INFO` pour bascule volontaire de profil ;
- `LOG_WARN` pour fallback utilisé ;
- `LOG_ERROR` si backend V4 et fallback échouent ;
- `LOG_CRITICAL` si l’état sûr ne peut pas être garanti.

## 8. Politique de rollback

Le rollback doit rester simple et immédiat.

### Rollback compile-time

Profil principal recommandé pendant les premiers runs :

```text
backend historique actif
backend V4 compilé mais non sélectionné
```

Un profil expérimental séparé activera le backend V4.

### Rollback runtime

Tant que le backend historique existe :

```text
EquipmentOutputRuntimeAdapter
  -> backend V4
  -> fallback direct RelaisManager
```

### Rollback Git

Chaque run de Phase 6 doit rester atomique, compilable et associé à un checkpoint ou commit clairement identifiable.

## 9. Stratégie d’activation progressive

### Étape A — backend V4 non actif

Le backend est créé et compilé, mais non injecté dans `main.cpp`.

### Étape B — backend V4 injecté mais aucune zone migrée

Le backend reçoit les appels mais renvoie volontairement “non géré” pour toutes les zones. Le fallback historique prend toutes les commandes.

### Étape C — une zone pilote

Une seule zone est autorisée sur le backend V4.

Recommandation : zone 0, sauf contrainte matérielle spécifique.

### Étape D — extension progressive

```text
1 zone
2 zones
4 zones
8 zones
```

Chaque extension exige :

- compilation OK ;
- test manuel commande ON/OFF ;
- état Web cohérent ;
- état LCD cohérent ;
- absence de sortie parasite ;
- absence de reboot ;
- fallback observé si panne simulée.

## 10. Politique d’état sûr

En cas d’incertitude :

```text
OFF est l’état sûr par défaut
```

Le backend V4 doit éviter :

- toute activation au boot avant initialisation complète ;
- toute double commande contradictoire ;
- toute activation d’un port non affecté ;
- toute conservation d’un état ON après perte du backend.

## 11. Lecture d’état

La lecture d’état doit distinguer :

```text
état commandé
état logique mémorisé
état matériel relu
état inconnu
```

Pendant la migration, une lecture V4 non fiable doit provoquer un fallback vers `RelaisManager` ou un état `UNKNOWN`, mais ne doit pas être présentée comme `VALID` sans preuve.

## 12. Critères de validation avant extension

Une zone migrée ne peut être étendue à d’autres zones que si :

1. commande ON réussie ;
2. commande OFF réussie ;
3. lecture d’état cohérente ;
4. Web cohérent ;
5. LCD cohérent ;
6. fallback testé ;
7. redémarrage sans activation parasite ;
8. logs explicites ;
9. mémoire stable ;
10. aucun changement NVS non prévu.

## 13. Critères de retrait des fallbacks

Aucun fallback ne sera supprimé pendant les premiers runs de Phase 6.

La suppression ne pourra être étudiée qu’après :

- migration complète de toutes les zones ;
- validation matérielle longue durée ;
- stratégie de test automatisée disponible ou reprise de la Phase 5 ;
- mesure des erreurs et fallbacks sur une période significative ;
- décision explicite de l’utilisateur.

Certains fallbacks pourront rester définitivement comme mode dégradé.

## 14. Runs proposés pour la Phase 6

```text
Run 6.1  Stratégie de bascule du backend physique
Run 6.2  Backend V4 derrière RelayPhysicalBackend
Run 6.3  Résolution RelayAssignment vers driver/port
Run 6.4  Profils compile-time legacy / V4
Run 6.5  Activation sur une zone pilote
Run 6.6  Comparaison ancien / nouveau backend
Run 6.7  Migration progressive des zones
Run 6.8  Tests de panne et fallback
Run 6.9  Stabilisation du backend V4
Run 6.10 Décision de retrait des fallbacks
```

## 15. Invariants du prochain run

Pour Run 6.2 :

1. aucun changement NVS ;
2. aucun changement Web ;
3. aucun changement LCD ;
4. aucun changement JSON ;
5. aucun backend V4 actif dans `main.cpp` ;
6. backend historique inchangé ;
7. fallback direct conservé ;
8. compilation PlatformIO obligatoire ;
9. diff minimal ;
10. documentation complète.

## 16. Décision finale

Le Run 6.1 valide une migration progressive, réversible et instrumentée.

Prochaine étape :

```text
AquaLook V4 — Phase 6 — Run 6.2
Créer V4RelayPhysicalBackend derrière RelayPhysicalBackend, sans activation runtime
```
