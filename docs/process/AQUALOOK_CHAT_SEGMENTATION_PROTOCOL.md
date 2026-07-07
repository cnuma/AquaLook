# AquaLook — Protocole de segmentation des chats et de transmission

**Statut :** règle de gouvernance du projet  
**Application :** tous les chantiers AquaLook, en particulier AquaLook V4  
**Date :** 7 juillet 2026

## 1. Objectif

Les conversations longues deviennent progressivement plus lentes, plus difficiles à relire et plus exposées aux pertes de contexte, aux confusions entre anciennes et nouvelles décisions et aux régressions documentaires.

Le projet AquaLook doit donc être conduit par chats segmentés, chacun correspondant à un périmètre cohérent et limité.

La segmentation d’un chat n’est pas un abandon du travail en cours. C’est une opération de transmission contrôlée reposant sur :

1. un état Git propre et synchronisé ;
2. un checkpoint autonome ;
3. un message de synthèse prêt à ouvrir un nouveau chat ;
4. l’identification explicite de la prochaine action.

---

## 2. Principe général

Un chat doit couvrir de préférence :

- un run technique ;
- une décision d’architecture importante ;
- une anomalie clairement délimitée ;
- une phase documentaire courte ;
- ou un ensemble de modifications fortement liées.

Un même chat ne doit pas accumuler sans limite :

- plusieurs phases de roadmap ;
- des sujets fonctionnels sans lien ;
- des branches Git différentes ;
- des bases de travail successives ;
- des versions de fichiers contradictoires ;
- plusieurs checkpoints stables non distingués.

---

## 3. Mesure opérationnelle de la taille du chat

Le nombre exact de tokens disponibles n’étant pas une mesure directement accessible et stable dans le déroulement du projet, la décision de segmentation repose sur un **indice de saturation du chat**.

Cet indice utilise quatre dimensions observables.

### 3.1 Volume des échanges

- **Niveau 0 — faible :** moins de 25 échanges substantiels ;
- **Niveau 1 — moyen :** de 25 à 50 échanges substantiels ;
- **Niveau 2 — élevé :** plus de 50 échanges substantiels.

Un échange substantiel correspond à une demande, une analyse, une modification, un test ou une décision. Les confirmations très courtes ne sont pas comptées comme des échanges substantiels.

### 3.2 Nombre de changements de base

Ajouter un point pour chacun des événements suivants :

- changement de branche ;
- changement de commit de référence ;
- nouveau tag de reprise ;
- nouveau checkpoint stable ;
- remplacement de la source de vérité ;
- import d’un nouveau jeu complet de fichiers.

### 3.3 Nombre de sous-sujets

Ajouter un point pour chaque sujet indépendant ouvert dans le même chat, par exemple :

- modèle métier ;
- relais ;
- persistance ;
- interface Web ;
- compilation ;
- matériel ;
- diagnostic ;
- documentation.

### 3.4 Signaux qualitatifs

La segmentation devient prioritaire dès qu’un des signaux suivants apparaît :

- lenteur sensible des réponses ou de l’interface ;
- nécessité répétée de rappeler la base de travail ;
- risque de confusion entre plusieurs versions ;
- réponses qui doivent reconstituer trop d’historique ;
- difficulté à identifier la prochaine action ;
- multiplication des fichiers et décisions ;
- changement de phase dans la roadmap ;
- fin d’un run compilé ou matériellement validé.

---

## 4. Niveaux de saturation

### Vert — chat sain

Conditions indicatives :

- moins de 25 échanges substantiels ;
- un seul sujet principal ;
- une seule base Git ;
- aucun signal qualitatif préoccupant.

Action : continuer le chat.

### Orange — segmentation à préparer

Conditions indicatives :

- entre 25 et 50 échanges substantiels ;
- au moins deux changements de base ;
- plusieurs sous-sujets actifs ;
- ou apparition d’un premier signal qualitatif.

Actions obligatoires :

- vérifier que les décisions importantes sont documentées ;
- préparer le prochain checkpoint ;
- éviter d’ouvrir un nouveau sous-sujet ;
- annoncer que le chat devra être segmenté à la fin du run courant.

### Rouge — segmentation obligatoire

La segmentation est obligatoire dans l’un des cas suivants :

- plus de 50 échanges substantiels ;
- changement de phase de roadmap ;
- fin d’un run validé ;
- nouvelle branche structurante ;
- nouvelle source de vérité ;
- lenteur importante ;
- risque explicite de confusion ou de régression ;
- trois sous-sujets indépendants ou davantage ;
- plusieurs fichiers structurants modifiés sans checkpoint récent.

Aucun nouveau chantier ne doit être ouvert dans ce chat.

---

## 5. Responsabilité de déclenchement

L’assistant doit surveiller la taille et la cohérence du chat pendant le travail.

Il doit proposer la segmentation :

- dès le niveau orange lorsque la fin du run approche ;
- immédiatement au niveau rouge ;
- avant tout changement de phase ;
- avant l’ouverture d’un sujet non directement lié au run courant.

L’utilisateur n’a pas à détecter lui-même la saturation.

La proposition doit être explicite et indiquer :

- pourquoi la segmentation est recommandée ;
- ce qui doit être terminé avant la coupure ;
- les documents et commits à produire ;
- le titre recommandé du nouveau chat.

---

## 6. Conditions obligatoires avant fermeture d’un chat

Avant de recommander l’ouverture du nouveau chat, les éléments suivants doivent être traités.

### 6.1 État Git

Vérifier et consigner :

- dépôt ;
- branche ;
- HEAD local de référence ;
- HEAD distant ;
- statut du working tree ;
- commits créés ;
- push réalisé ;
- tag éventuel ;
- état de synchronisation avec `origin`.

Un chat ne doit pas être clôturé sur une ambiguïté Git évitable.

Si des changements ne peuvent pas être commités ou poussés, le message de transmission doit le signaler clairement.

### 6.2 Validation

Consigner précisément :

- compilation effectuée ou non ;
- environnement utilisé ;
- tests exécutés ;
- résultats ;
- tests matériels restant à faire ;
- incertitudes et risques.

### 6.3 Documentation

Mettre à jour selon le périmètre :

- architecture ;
- ADR ;
- roadmap ;
- backlog ;
- checkpoint ;
- procédures ;
- liste des fichiers modifiés.

### 6.4 Prochaine action

La prochaine action doit être unique et formulée sans ambiguïté.

Exemple :

```text
Prochaine action : inventorier les dépendances actuelles de ConfigManager avant toute création du modèle Equipment V4.
```

---

## 7. Checkpoint obligatoire

Toute segmentation après un travail significatif doit produire ou mettre à jour un checkpoint dans :

```text
docs/checkpoints/
```

Nom recommandé :

```text
CHECKPOINT_YYYY-MM-DD_<sujet>_<statut>.md
```

Le checkpoint doit être autonome et contenir au minimum :

1. objet du chantier ;
2. dépôt, branche, commit et tag ;
3. source de vérité ;
4. état fonctionnel validé ;
5. décisions prises ;
6. invariants à préserver ;
7. fichiers modifiés ;
8. fichiers volontairement non modifiés ;
9. compilation et tests ;
10. défauts ou limites connus ;
11. prochaine action ;
12. commandes de reprise.

Le checkpoint doit permettre de reprendre le projet sans relire le chat précédent.

---

## 8. Message de synthèse pour ouvrir le nouveau chat

Le dernier livrable du chat est un message de reprise prêt à être copié dans une nouvelle conversation.

Le modèle obligatoire est le suivant :

```text
Projet AquaLook — <nom du chantier ou de la phase>

Base de travail :
- Dépôt : cnuma/AquaLook
- Branche : <branche>
- HEAD local et distant : <commit complet>
- Tag éventuel : <tag>
- Working tree : <propre / modifications restantes>

Documents de référence :
- <document 1>
- <document 2>
- <checkpoint de reprise>

État validé à préserver :
- <invariant ou comportement 1>
- <invariant ou comportement 2>
- <résultat de compilation ou test>

Travail réalisé dans le chat précédent :
- <résumé 1>
- <résumé 2>

Fichiers modifiés :
- <fichier>

Fichiers volontairement non modifiés :
- <fichier ou domaine>

Points ouverts et risques :
- <point 1>
- <point 2>

Objectif unique du nouveau chat :
<objectif précis>

Première action attendue :
<action précise, sans ouvrir de chantier parallèle>

Contraintes :
- respecter l’architecture V4 et les invariants documentés ;
- ne pas modifier la persistance avant décision dédiée ;
- modifier le minimum nécessaire ;
- fournir les fichiers complets dès que plus de deux fichiers sont modifiés ;
- produire un checkpoint et synchroniser Git avant la prochaine segmentation.
```

Le message doit mentionner les chemins exacts des documents et le commit complet, pas seulement un SHA abrégé lorsque la transmission devient la nouvelle source de vérité.

---

## 9. Mise à jour Git obligatoire

La segmentation doit normalement se terminer par :

```text
Documentation à jour
-> checkpoint créé ou mis à jour
-> fichiers ajoutés à Git
-> commit cohérent
-> push sur la branche de travail
-> vérification HEAD local = HEAD distant
-> message de reprise produit
-> ouverture du nouveau chat
```

Le commit de clôture doit être limité au périmètre du run.

Exemples de messages :

```text
docs: checkpoint V4 architecture baseline
docs: define V4 phase 1 execution plan
refactor: isolate V4 equipment domain model
```

Un nouveau dépôt ne doit pas être créé pour segmenter les chats. La continuité est assurée par le dépôt AquaLook, les branches, les commits, les tags et les checkpoints.

---

## 10. Règles de nommage des chats

Format recommandé :

```text
AquaLook V4 — <phase> — <run> — <sujet>
```

Exemples :

```text
AquaLook V4 — Phase 0 — Run 0 — Cartographie existant
AquaLook V4 — Phase 1 — Run 1.1 — Identités stables
AquaLook V4 — Phase 1 — Run 1.2 — Equipment Model
AquaLook V4 — Phase 2 — Run 2.1 — Inventaire matériel
```

Un titre doit permettre de comprendre immédiatement :

- la version ;
- la phase ;
- le run ;
- le sujet principal.

---

## 11. Cas imposant une segmentation immédiate

Même si le chat semble encore court, une nouvelle conversation doit être ouverte lorsque :

- le travail passe de l’architecture au code ;
- le travail passe du modèle métier à la persistance ;
- le travail passe du logiciel aux tests matériels ;
- une nouvelle branche devient la base officielle ;
- un checkpoint stable remplace la source de vérité précédente ;
- un sujet urgent sans rapport avec le run courant apparaît ;
- un nouveau jeu complet de fichiers devient la source de vérité.

---

## 12. Invariants de transmission

1. Git est la mémoire technique durable du projet.
2. Le checkpoint est la mémoire de reprise du run.
3. Le message de synthèse est le contrat d’entrée du nouveau chat.
4. Le nouveau chat ne doit pas dépendre de la relecture de l’ancien.
5. Aucune décision importante ne doit rester uniquement dans la conversation.
6. Une segmentation ne change pas implicitement la source de vérité.
7. La branche et le commit de reprise doivent être explicites.
8. Les tests non effectués doivent être signalés, jamais supposés réussis.
9. Les changements non poussés doivent être signalés comme tels.
10. Le prochain chat commence avec un objectif unique.

---

## 13. Application à AquaLook V4

Pour AquaLook V4, une segmentation est recommandée au minimum :

- à la fin de chaque run ;
- à la fin de chaque phase ;
- après validation d’une ADR structurante ;
- après validation de compilation ;
- après validation matérielle ;
- avant toute migration de persistance ;
- avant toute bascule de chemin runtime.

Le protocole fait partie de la gouvernance V4 et complète :

```text
docs/architecture/AQUALOOK_V4_TARGET_ARCHITECTURE.md
```

Il doit être référencé par les futurs plans de phase et checkpoints.
