# AquaLook V4 — Cycle de vie de la configuration générique

**Statut :** architecture de référence  
**Date :** 7 juillet 2026  
**Phase d’origine :** V4 Phase 1 — Run 1.1 corrigé

## 1. Objectif

Définir comment AquaLook V4 construit, valide, active, modifie et remplace une configuration générique sans dépendre de tableaux fonctionnels figés.

La configuration doit pouvoir évoluer par ajout, suppression, remplacement ou mise à niveau de :

- modèles de cartes ;
- cartes installées ;
- ports exposés ;
- équipements ;
- capteurs ;
- automatismes ;
- dépendances ;
- affectations entre fonctions logiques et ports physiques.

## 2. Principe supérieur

```text
Définitions génériques
+ configuration utilisateur
+ inventaire matériel
=
configuration candidate construite selon le besoin réel
```

La configuration active n’est jamais éditée directement.

Toute modification produit une nouvelle candidate complète.

## 3. Composants architecturaux

```text
BoardModelCatalog
ConfigurationSource
ConfigurationBuilder
CandidateConfiguration
ConfigurationValidator
ConfigurationActivator
ActiveConfiguration
ConfigurationDiagnostics
```

### BoardModelCatalog

Catalogue des définitions de cartes connues.

Un descripteur de modèle indique notamment :

```text
modelId
version
nom
famille de driver
bus supporté
description des ports
capacités de chaque port
paramètres techniques
règles de validation spécifiques
```

Le catalogue peut contenir :

- des modèles intégrés au firmware ;
- des modèles importés et validés ;
- des versions successives d’un même modèle.

La stratégie exacte d’import des descripteurs sera décidée ultérieurement. Aucun descripteur non validé ne devient actif.

### ConfigurationSource

Représentation d’entrée provenant plus tard de :

- NVS ;
- fichier d’import ;
- API ;
- migration ;
- configuration par défaut.

Cette source n’est jamais utilisée directement par le runtime.

### ConfigurationBuilder

Transforme la source en objets compacts dans l’arène candidate.

Il :

1. lit les en-têtes et compteurs ;
2. vérifie les limites absolues ;
3. résout les modèles de cartes ;
4. calcule les ports exposés ;
5. mesure la mémoire requise ;
6. réserve séquentiellement les objets ;
7. construit les registres et relations ;
8. produit un rapport de construction.

### CandidateConfiguration

Configuration complète mais non active.

Elle possède :

```text
revision
generation
memoryUsage
inventory
entities
relations
validationState
buildReport
```

Elle ne peut commander aucun matériel.

### ConfigurationValidator

Effectue une validation indépendante et complète.

### ConfigurationActivator

Bascule vers la candidate uniquement lorsqu’elle est valide et activable.

### ActiveConfiguration

Vue immuable utilisée par le runtime.

Les services consultent cette vue, mais ne changent jamais directement sa structure.

### ConfigurationDiagnostics

Expose les informations nécessaires au diagnostic :

```text
génération active
révision active
origine
mémoire utilisée
nombre de cartes et ports
nombre d’objets par catégorie
bindings valides et invalides
erreurs de la dernière candidate
motif du dernier refus
```

## 4. Construction de l’inventaire depuis les cartes

Exemple de source :

```text
board-1 : modèle relay-4, adresse 0x20
board-2 : modèle relay-2, adresse 0x21
board-3 : modèle acquisition-4di, adresse 0x30
```

Le catalogue fournit :

```text
relay-4
- 4 sorties TOR

relay-2
- 2 sorties TOR

acquisition-4di
- 4 entrées TOR
```

Le builder déduit :

```text
cartes = 3
ports = 10
sorties TOR = 6
entrées TOR = 4
```

Les ports peuvent être représentés :

- explicitement lorsqu’ils ont des paramètres d’instance ;
- implicitement par le descripteur lorsqu’ils sont fixes ;
- par une vue calculée lors des validations et consultations.

Le choix doit minimiser la RAM sans compliquer excessivement les drivers.

## 5. Affectations logiques

Un `PortBinding` relie une fonction logique à un port :

```text
logicalTargetId
functionId
BoardId
portIndex
bindingParameters
```

Exemples :

```text
vanne zone nord / commande TOR
-> board-1 / port 0

capteur débit nord / compteur impulsions
-> board-3 / port 0
```

La validation vérifie que le port possède les capacités nécessaires.

Un binding ne transforme pas la carte en objet métier. La carte reste indépendante de l’équipement ou du capteur qui l’utilise.

## 6. Cycle de modification

```text
ActiveConfiguration génération N
        ↓
commande de modification
        ↓
création d’une source N+1
        ↓
construction dans CandidateConfigurationArena
        ↓
validation complète
        ├── échec : abandon candidate, active N conservée
        └── succès : préparation activation
                         ↓
                    bascule atomique
                         ↓
                  ActiveConfiguration N+1
```

L’ancienne configuration doit rester disponible jusqu’à ce que la nouvelle soit effectivement activée ou qu’une stratégie de rollback sûre ait été préparée.

## 7. Ajout d’une carte

Lors de l’ajout :

1. vérifier que `modelId` existe ;
2. vérifier que le bus est supporté ;
3. vérifier l’adresse ou l’emplacement ;
4. déduire les ports ;
5. recalculer la mémoire ;
6. vérifier les conflits ;
7. conserver les bindings existants ;
8. ajouter les nouveaux bindings demandés ;
9. valider toute la candidate.

La simple détection physique d’une carte ne l’ajoute pas automatiquement à la configuration active sans politique explicite.

## 8. Suppression d’une carte

La suppression produit une candidate dans laquelle :

- la carte est absente ;
- ses bindings deviennent orphelins ou sont supprimés explicitement ;
- les équipements dépendants sont signalés ;
- l’activation est refusée si une fonction obligatoire n’a plus d’affectation sûre.

Aucune suppression silencieuse de binding n’est autorisée par défaut.

## 9. Remplacement ou upgrade

Un remplacement conserve le `BoardId` uniquement si l’opération est déclarée comme remplacement de la même instance logique.

Le validateur compare :

```text
anciens ports utilisés
nouveaux ports disponibles
capacités requises
compatibilité des bindings
paramètres de driver
```

Un upgrade de descripteur possède une version explicite. Toute incompatibilité exige une migration ou une intervention utilisateur.

## 10. Validation structurelle

La candidate doit respecter :

- format et version reconnus ;
- tailles et offsets valides ;
- absence de dépassement arithmétique ;
- identifiants uniques ;
- modèles connus ;
- index de ports valides ;
- références non orphelines ;
- types de paramètres cohérents ;
- chaînes correctement bornées.

## 11. Validation matérielle

Elle contrôle :

- compatibilité du bus ;
- unicité des adresses lorsque requise ;
- capacités des ports ;
- directions ;
- état sûr ;
- ressources de driver ;
- incompatibilités électriques déclarées ;
- bindings exclusifs.

La présence physique peut être une condition d’activation ou un avertissement selon la politique du modèle de carte et la criticité de l’équipement.

## 12. Validation métier

Elle contrôle :

- équipements obligatoires correctement définis ;
- capteurs requis présents ;
- dépendances résolues ;
- absence de cycles interdits ;
- automatismes référençant des cibles existantes ;
- fonctions de sécurité affectées à des ports compatibles.

## 13. Validation mémoire et temporelle

Elle contrôle :

- taille de l’arène candidate ;
- marge minimale ;
- taille des tables de résolution ;
- nombre maximal d’itérations de validation ;
- profondeur des graphes ;
- temps estimé de construction ;
- capacité du runtime d’exécution.

## 14. Activation atomique

L’activation doit présenter au runtime soit l’ancienne configuration complète, soit la nouvelle complète.

Aucun service ne doit observer un mélange des deux générations.

Mécanismes possibles à étudier :

- échange de pointeur vers l’arène active ;
- double buffer ;
- index de génération ;
- section critique courte ;
- arrêt contrôlé des exécutions incompatibles avant bascule.

Le mécanisme final sera décidé avant la persistance V4.

## 15. Comportement pendant une exécution active

Une modification de configuration ne doit pas invalider silencieusement une exécution en cours.

Les politiques possibles sont :

```text
refuser la bascule tant qu’une ressource concernée est active
arrêter proprement les exécutions concernées
programmer l’activation différée
```

La politique exacte appartient à l’orchestration et à la Phase 7, mais la configuration doit connaître sa génération.

## 16. Persistance future

La persistance devra stocker une représentation sérialisée compacte, pas l’image mémoire brute de l’arène.

La lecture suit :

```text
blob persistant
-> contrôle intégrité
-> ConfigurationSource
-> builder
-> candidate
-> validator
-> activator
```

Cela dissocie le format persistant de la disposition mémoire C++.

## 17. Compatibilité avec AquaLook actuel

Pendant la transition :

- le NVS actuel reste inchangé ;
- `RelayTopology` reste utilisé par le runtime ;
- un adaptateur pourra produire une source V4 depuis la configuration historique ;
- la configuration V4 ne commande rien avant les phases d’intégration prévues ;
- le fallback historique `Zone N -> carte 0 -> voie N` reste disponible.

## 18. Critères d’acceptation

L’architecture de configuration est respectée lorsque :

1. le nombre réel d’objets vient de la configuration ;
2. les ports viennent des descripteurs de cartes ;
3. la mémoire est calculée avant construction complète ou activation ;
4. une candidate invalide ne modifie pas l’active ;
5. la configuration active est immuable ;
6. les changements sont appliqués par génération complète ;
7. les diagnostics expliquent tout refus ;
8. aucune capacité fonctionnelle n’est dérivée d’un tableau relais historique.
