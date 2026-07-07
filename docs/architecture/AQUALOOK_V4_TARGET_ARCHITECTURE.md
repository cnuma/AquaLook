# AquaLook V4 — Architecture cible système et logicielle

**Statut :** document d’architecture de référence  
**Nature :** source de vérité pour les développements AquaLook V4  
**Dépôt :** `cnuma/AquaLook`  
**Branche de conception :** `feature/relay-board-mapping`  
**HEAD de référence :** `b40c63720bc95a362bced9f26f3f78afaac76804`  
**Tag de référence :** `relay-topology-v1`  
**Date :** 7 juillet 2026

## 1. Finalité du document

Ce document définit l’architecture cible complète d’AquaLook V4.

Il fixe :

- le modèle métier ;
- la séparation entre métier, automatismes et matériel ;
- les équipements et leurs relations ;
- les capteurs et la qualité des mesures ;
- les automatismes et leur arbitrage ;
- la topologie des relais et des cartes ;
- la persistance et les migrations ;
- les API internes et externes ;
- l’organisation logicielle ;
- les dépendances autorisées entre modules ;
- les invariants de sécurité et de compatibilité ;
- la stratégie de transition depuis l’architecture actuelle ;
- la roadmap d’implémentation.

Ce document ne décrit pas une implémentation ponctuelle. Il définit les frontières durables du système.

Toute évolution future doit soit respecter cette architecture, soit faire l’objet d’une décision d’architecture explicite, documentée et versionnée.

---

## 2. Vision d’AquaLook V4

AquaLook V4 est un contrôleur local d’équipements hydrauliques, agricoles et environnementaux.

Sa première fonction reste l’arrosage, mais son architecture doit permettre de gérer sans refonte structurelle :

- des zones d’arrosage ;
- des électrovannes ;
- une ou plusieurs pompes ;
- une vanne maîtresse ;
- des équipements auxiliaires ;
- des équipements de serre ;
- des éclairages ;
- des ventilateurs ;
- des brumisateurs ;
- des capteurs de température, humidité, pression, débit, niveau, pluie ou luminosité ;
- des automatismes temporels, conditionnels et de sécurité.

Le principe central est le suivant :

> Le métier demande une action sur un équipement.  
> L’équipement est orchestré selon ses dépendances et ses règles.  
> L’action physique est exécutée par un actionneur.  
> Le relais et la carte relais ne sont que des moyens matériels d’exécution.

Le flux cible est :

```text
Intention métier
    -> Automatisme ou commande manuelle
        -> Service d’orchestration
            -> Équipement logique
                -> Actionneur logique
                    -> Affectation matérielle
                        -> Carte physique
                            -> Canal physique
```

Le chemin inverse pour l’observation est :

```text
Capteur physique
    -> Canal d’entrée
        -> Mesure normalisée
            -> État observé
                -> Règle métier
                    -> Décision
```

---

## 3. Principes structurants

### 3.1 Séparation stricte des responsabilités

L’architecture V4 sépare cinq plans.

#### Plan métier

Il décrit ce que représente l’installation :

- zones ;
- équipements ;
- ressources ;
- dépendances ;
- politiques de fonctionnement ;
- autorisations ;
- contraintes.

#### Plan automatisation

Il décide quand une action doit être demandée :

- planning ;
- calendrier ;
- intervalle ;
- règle climatique ;
- seuil ;
- séquence ;
- commande utilisateur ;
- sécurité.

#### Plan orchestration

Il transforme une intention en séquence sûre :

- démarrage de pompe ;
- attente de montée en pression ;
- ouverture de vanne ;
- maintien ;
- fermeture ;
- arrêt différé ;
- arbitrage entre demandes concurrentes ;
- reprise après incident.

#### Plan matériel

Il exécute les commandes physiques :

- relais ;
- sorties numériques ;
- entrées numériques ;
- bus I²C ;
- cartes d’extension ;
- capteurs ;
- contrôleurs.

#### Plan présentation et intégration

Il expose le système :

- interface Web ;
- LCD ;
- API JSON ;
- journal d’événements ;
- diagnostic ;
- import/export de configuration.

Aucun plan ne doit contourner celui qui le suit.

### 3.2 Dépendance dirigée vers le domaine

Les modules de haut niveau ne doivent pas dépendre des détails matériels.

Les dépendances autorisées suivent la direction :

```text
Présentation
    -> Services applicatifs
        -> Domaine
            -> Ports abstraits
                <- Adaptateurs matériels et persistance
```

Le domaine ne dépend pas :

- du Web ;
- de LittleFS ou de la carte SD ;
- de NVS ;
- d’un contrôleur de relais particulier ;
- d’une adresse I²C ;
- de la structure d’une page HTML ;
- d’un format de transport JSON.

### 3.3 Échec sûr

Lorsqu’une décision ne peut pas être exécutée avec certitude :

- aucune sortie ne doit être activée par défaut ;
- l’équipement concerné passe en défaut ou en indisponibilité ;
- l’erreur est journalisée ;
- l’état demandé reste distinct de l’état effectivement appliqué ;
- le système n’invente jamais un succès matériel.

### 3.4 Compatibilité contrôlée

La transition V3 vers V4 doit préserver le comportement validé tant qu’une fonction V4 n’est pas activée.

La configuration historique :

```text
Zone N -> carte 0 -> voie N
```

reste le profil de compatibilité initial.

---

## 4. Vocabulaire normatif

### 4.1 Installation

Ensemble cohérent géré par une instance AquaLook :

- contrôleur principal ;
- cartes ;
- équipements ;
- capteurs ;
- zones ;
- automatismes ;
- paramètres ;
- état courant.

### 4.2 Zone

Une zone est un objet métier délimitant un usage d’arrosage.

Une zone contient notamment :

- une identité stable ;
- un nom ;
- un état activé ou désactivé ;
- une politique d’arrosage ;
- des programmes ou règles de déclenchement ;
- un équipement de distribution d’eau associé ;
- éventuellement une ressource hydraulique requise ;
- éventuellement des capteurs associés ;
- des limites de durée et de fréquence.

Une zone n’est pas :

- un relais ;
- une voie physique ;
- une adresse I²C ;
- une carte ;
- une électrovanne elle-même.

### 4.3 Équipement

Un équipement est une entité logique pilotable ou observable.

Exemples :

- électrovanne ;
- pompe ;
- vanne maîtresse ;
- volet ;
- ventilateur ;
- éclairage ;
- brumisateur ;
- alarme ;
- contact auxiliaire.

Un équipement possède :

- une identité stable ;
- un type ;
- un nom ;
- une disponibilité ;
- un mode ;
- un état demandé ;
- un état calculé ;
- un état appliqué ;
- un état observé lorsque disponible ;
- des capacités ;
- des paramètres propres à son type ;
- des dépendances ;
- un ou plusieurs actionneurs ;
- éventuellement des capteurs de retour.

### 4.4 Actionneur

Un actionneur est l’abstraction d’une commande physique.

Exemples :

- sortie relais binaire ;
- sortie PWM ;
- commande de position ;
- contact impulsionnel ;
- commande montée/descente ;
- sortie distante.

Dans la première phase V4, l’actionneur principal est binaire.

### 4.5 Relais

Un relais est une implémentation physique possible d’un actionneur binaire.

Le relais ne porte aucune signification métier.

Il ne sait pas s’il commande :

- une zone ;
- une pompe ;
- une lampe ;
- un ventilateur ;
- une alarme.

### 4.6 Carte matérielle

Une carte matérielle est un périphérique physique déclaré dans l’inventaire matériel.

Une carte possède :

- une identité logique stable ;
- un type de périphérique ;
- un pilote ;
- un bus ;
- une adresse ;
- des capacités ;
- des canaux ;
- un statut de présence ;
- un état de santé ;
- une configuration électrique.

Une carte relais est un cas particulier de carte matérielle.

### 4.7 Capteur

Un capteur produit une observation horodatée.

Il possède :

- une identité ;
- un type ;
- une unité ;
- une source matérielle ;
- une fréquence d’acquisition ;
- une politique de validation ;
- une valeur brute ;
- une valeur normalisée ;
- un état de qualité ;
- une date de dernière lecture valide ;
- des seuils de cohérence ;
- éventuellement un étalonnage.

### 4.8 Automatisme

Un automatisme produit une intention à partir :

- du temps ;
- d’un calendrier ;
- d’un état métier ;
- d’une mesure ;
- d’un événement ;
- d’une commande externe ;
- d’une combinaison de conditions.

Un automatisme ne pilote jamais directement un relais.

### 4.9 Intention

Une intention exprime une demande métier :

- démarrer l’arrosage d’une zone ;
- arrêter une zone ;
- activer un équipement ;
- changer un mode ;
- inhiber une fonction ;
- exécuter une séquence.

Une intention comporte :

- une origine ;
- une priorité ;
- une date de création ;
- une durée de validité ;
- une cible ;
- une action ;
- un contexte ;
- un identifiant de corrélation.

### 4.10 Défaut

Un défaut est une condition empêchant ou limitant un fonctionnement normal.

Un défaut comporte :

- un code stable ;
- une sévérité ;
- une source ;
- une date d’apparition ;
- un état actif ou acquitté ;
- un caractère bloquant ou non bloquant ;
- un message destiné à l’utilisateur ;
- des données de diagnostic.

---

## 5. Modèle métier cible

## 5.1 Agrégats principaux

L’architecture métier repose sur les agrégats suivants :

```text
Installation
├── Zones
├── Équipements
├── Ressources
├── Capteurs
├── Automatismes
├── Topologie matérielle
├── Politiques de sécurité
└── Configuration système
```

Chaque agrégat possède ses règles de cohérence et ne doit pas être modifié partiellement sans validation.

## 5.2 Identités stables

Les objets métier doivent être référencés par un identifiant stable, indépendant :

- de leur position dans un tableau ;
- de leur ordre d’affichage ;
- du nombre d’objets ;
- de la voie physique ;
- de l’adresse matérielle.

Les index de tableau restent des détails de stockage ou d’exécution.

Les références entre objets doivent logiquement cibler un identifiant, même si une représentation compacte par index est retenue dans une première implémentation embarquée.

Toute suppression doit gérer les références orphelines.

## 5.3 États distincts

Pour chaque équipement, V4 distingue au minimum :

- **état demandé** : volonté issue d’un automatisme ou d’une commande ;
- **état autorisé** : résultat après politiques et interverrouillages ;
- **état appliqué** : commande transmise à l’actionneur ;
- **état observé** : retour réel, s’il existe ;
- **état de santé** : disponible, dégradé, indisponible, en défaut.

Cette séparation évite de présenter une commande comme une réalité physique.

## 5.4 Modes de fonctionnement

Les équipements et zones peuvent utiliser les modes suivants :

- `DISABLED` : volontairement désactivé ;
- `AUTOMATIC` : piloté par les automatismes ;
- `MANUAL_TEMPORARY` : commande manuelle limitée dans le temps ;
- `MANUAL_FORCED` : commande forcée explicite ;
- `MAINTENANCE` : essais autorisés avec protections adaptées ;
- `SAFE` : état imposé par sécurité ;
- `FAULT` : fonctionnement bloqué par défaut.

Les transitions de mode doivent être tracées.

Un redémarrage ne doit pas réactiver silencieusement une commande manuelle forcée, sauf politique explicitement configurée.

---

## 6. Catalogue des équipements

## 6.1 Familles

### Équipements hydrauliques

- électrovanne de zone ;
- vanne maîtresse ;
- pompe ;
- pompe de secours ;
- vanne de remplissage ;
- vanne de vidange ;
- électrovanne de purge.

### Équipements climatiques

- volet ou ouvrant ;
- ventilateur ;
- chauffage ;
- brumisateur ;
- extracteur ;
- déshumidificateur.

### Équipements électriques

- éclairage ;
- alimentation auxiliaire ;
- contact sec ;
- signalisation ;
- sirène ou alarme.

### Équipements virtuels

- groupe hydraulique ;
- groupe de ventilation ;
- scène ;
- ressource partagée ;
- verrou logique ;
- équipement calculé.

## 6.2 Capacités

Le comportement ne doit pas être déduit uniquement du type.

Chaque équipement expose des capacités, par exemple :

- commutation binaire ;
- commande impulsionnelle ;
- maintien ;
- position ;
- retour d’état ;
- temporisation de démarrage ;
- temporisation d’arrêt ;
- durée minimale active ;
- durée minimale inactive ;
- activation exclusive ;
- partageable ;
- reprise autorisée après redémarrage ;
- sécurité normalement ouverte ou normalement fermée.

Les automatismes ciblent des capacités, puis vérifient la compatibilité de l’équipement.

## 6.3 Paramètres communs

Tout équipement possède au minimum :

- identité ;
- nom ;
- description optionnelle ;
- type ;
- activation ;
- mode par défaut ;
- état sûr ;
- délai de démarrage ;
- délai d’arrêt ;
- durée minimale de marche ;
- durée minimale d’arrêt ;
- durée maximale de marche ;
- comportement au boot ;
- comportement après perte de communication ;
- criticité ;
- actionneur associé ;
- capteur de retour optionnel.

## 6.4 Paramètres spécifiques

Les paramètres spécifiques ne doivent pas gonfler une structure universelle non maîtrisée.

Ils doivent être organisés par profil de type.

Exemples :

### Pompe

- délai de précharge ;
- délai de post-fonctionnement ;
- anti-cycles courts ;
- durée maximale continue ;
- pressostat attendu ;
- débit minimal attendu ;
- politique en cas de fonctionnement à sec ;
- nombre maximal de démarrages par heure.

### Électrovanne

- pompe ou ressource hydraulique requise ;
- vanne maîtresse requise ;
- temps d’ouverture estimé ;
- temps de fermeture estimé ;
- durée maximale d’activation ;
- débit nominal optionnel.

### Volet

- actionneur ouverture ;
- actionneur fermeture ;
- temps de course ;
- fins de course ;
- interdiction d’alimenter les deux directions simultanément.

### Éclairage

- durée maximale manuelle ;
- comportement après redémarrage ;
- inhibition horaire éventuelle.

---

## 7. Relations et dépendances entre équipements

## 7.1 Nature des dépendances

Une relation entre équipements possède une sémantique explicite.

Types principaux :

- `REQUIRES_BEFORE` : A doit être actif avant B ;
- `REQUIRES_WHILE` : A doit rester actif pendant B ;
- `REQUIRES_AFTER` : A reste actif après B ;
- `EXCLUDES` : A et B ne peuvent pas être actifs simultanément ;
- `SHARES_RESOURCE` : A et B utilisent une ressource commune ;
- `BACKUP_FOR` : A remplace B en cas de défaut ;
- `FEEDBACK_FROM` : un capteur confirme l’état de l’équipement ;
- `MEMBER_OF` : équipement membre d’un groupe logique.

## 7.2 Graphe orienté

Les dépendances forment un graphe orienté.

À la validation de configuration :

- les cycles interdits doivent être détectés ;
- les références absentes doivent être rejetées ;
- les dépendances incompatibles doivent être rejetées ;
- les séquences impossibles doivent être signalées ;
- les ressources partagées doivent avoir une politique d’arbitrage.

## 7.3 Exemple hydraulique

```text
Zone Pelouse
    -> utilise Électrovanne Pelouse
        -> requiert Vanne maîtresse
        -> requiert Pompe forage
            -> requiert niveau d’eau suffisant
            -> requiert absence de défaut thermique
```

L’automatisme d’arrosage ne connaît que la zone.

L’orchestrateur résout les dépendances et produit la séquence.

---

## 8. Modèle des zones d’arrosage

## 8.1 Responsabilité de la zone

La zone porte :

- l’intention agronomique ;
- le calendrier ;
- la durée ou la quantité cible ;
- la politique pluie ;
- l’équipement de distribution associé ;
- les limites ;
- l’historique fonctionnel.

La zone ne porte pas la topologie matérielle.

## 8.2 Stratégies d’arrosage

V4 doit préserver et formaliser :

- jours fixes ;
- intervalle ancré ;
- déclenchement manuel ;
- déclenchement conditionnel futur ;
- durée fixe ;
- quantité cible future ;
- ajustement météo futur.

Le mode intervalle conserve son ancrage durable.

Une occurrence sautée pour cause de pluie ne décale pas la séquence.

## 8.3 Exécution et reprise

Une exécution de zone est une instance distincte du programme.

Elle contient :

- origine ;
- programme ou règle source ;
- heure prévue ;
- heure réelle ;
- durée prévue ;
- durée écoulée ;
- état ;
- motif d’arrêt ;
- équipements mobilisés ;
- défauts rencontrés.

Après redémarrage, la politique de reprise doit être explicite :

- ne pas reprendre ;
- reprendre si l’occurrence est toujours dans sa fenêtre ;
- reprendre la durée restante ;
- demander confirmation.

Le choix par défaut recommandé est : **aucune activation physique implicite sans reconstruction validée de l’exécution**.

---

## 9. Capteurs et observations

## 9.1 Types de capteurs

V4 prévoit notamment :

- pluie ;
- température ;
- humidité de l’air ;
- humidité du sol ;
- luminosité ;
- pression ;
- débit ;
- niveau de cuve ;
- courant électrique ;
- tension ;
- état de contact ;
- fin de course ;
- présence d’une carte ;
- température interne du contrôleur.

## 9.2 Chaîne d’acquisition

```text
Source physique
    -> Pilote
        -> Lecture brute
            -> Conversion
                -> Étalonnage
                    -> Validation
                        -> Observation normalisée
                            -> Historique court
                                -> Consommateurs métier
```

## 9.3 Qualité de mesure

Toute observation doit porter une qualité :

- `VALID` ;
- `STALE` ;
- `OUT_OF_RANGE` ;
- `UNAVAILABLE` ;
- `COMMUNICATION_ERROR` ;
- `UNINITIALIZED` ;
- `ESTIMATED`.

Une valeur périmée ne doit jamais être traitée silencieusement comme une valeur valide.

## 9.4 Horodatage

Toute mesure possède :

- instant de lecture ;
- instant de dernière valeur valide ;
- âge maximal acceptable ;
- source d’horloge.

Les règles doivent pouvoir préciser leur comportement en absence de mesure valide :

- bloquer ;
- continuer en mode dégradé ;
- utiliser une valeur de repli ;
- lever une alarme.

## 9.5 Débitmètres

Les débitmètres nécessitent une abstraction distincte des capteurs lents.

Ils produisent :

- impulsions ;
- fréquence ;
- débit instantané ;
- volume cumulé ;
- volume par exécution ;
- détection de débit absent ;
- détection de débit anormal.

Le comptage critique peut être confié à un nœud d’extension dédié, mais AquaLook reste propriétaire du modèle métier et de l’interprétation.

---

## 10. Automatismes

## 10.1 Familles d’automatismes

### Temporels

- planning hebdomadaire ;
- intervalle ancré ;
- plage horaire ;
- calendrier ;
- minuterie ;
- durée après événement.

### Conditionnels

- seuil ;
- hystérésis ;
- combinaison booléenne ;
- fenêtre de validité ;
- absence ou présence d’un état.

### Séquentiels

- démarrage ordonné ;
- attente ;
- vérification ;
- action ;
- maintien ;
- arrêt ordonné.

### Sécurité

- durée maximale ;
- absence de débit ;
- surpression ;
- niveau bas ;
- conflit matériel ;
- perte de communication ;
- capteur incohérent.

### Manuels

- activation temporaire ;
- arrêt ;
- test ;
- acquittement ;
- neutralisation contrôlée.

## 10.2 Moteur de décision

Le moteur d’automatisation :

- évalue les règles ;
- produit des intentions ;
- ne commande pas le matériel ;
- ne gère pas directement les temporisations matérielles ;
- ne modifie pas directement l’état d’un relais.

## 10.3 Priorités

Les intentions sont arbitrées selon une priorité normative :

1. sécurité matérielle ;
2. arrêt d’urgence ;
3. protection de l’installation ;
4. maintenance ;
5. commande manuelle forcée autorisée ;
6. commande manuelle temporaire ;
7. automatisme métier ;
8. optimisation ;
9. confort.

Une priorité élevée peut refuser ou interrompre une intention inférieure.

Tout arbitrage est journalisé.

## 10.4 Cycle d’évaluation

Le système doit éviter les effets de bord d’une évaluation dispersée.

Cycle recommandé :

```text
Acquérir les observations
-> Mettre à jour les états
-> Évaluer les règles
-> Produire les intentions
-> Arbitrer
-> Construire les plans d’exécution
-> Appliquer les transitions
-> Vérifier les retours
-> Publier les événements
```

---

## 11. Orchestration des équipements

## 11.1 Rôle

L’orchestrateur est responsable :

- de la résolution des dépendances ;
- des séquences ;
- des temporisations ;
- des ressources partagées ;
- des interverrouillages ;
- de la cohérence des transitions ;
- de la compensation en cas d’échec ;
- de l’arrêt ordonné.

## 11.2 Machine d’états

Chaque exécution d’équipement suit une machine d’états explicite, par exemple :

```text
IDLE
-> REQUESTED
-> VALIDATING
-> WAITING_DEPENDENCY
-> STARTING
-> ACTIVE
-> STOPPING
-> COMPLETED
```

Sorties d’erreur possibles :

```text
REJECTED
FAILED
ABORTED
SAFE_STOP
```

Les temporisations sont non bloquantes.

## 11.3 Référencement des ressources

Une pompe partagée n’est pas simplement activée puis désactivée par chaque zone.

L’orchestrateur maintient une occupation logique ou un comptage de demandes.

La pompe reste active tant qu’au moins une exécution valide la requiert, sous réserve des protections.

## 11.4 Compensation

Si une séquence échoue :

- les équipements déjà activés sont remis dans un état sûr selon l’ordre inverse approprié ;
- l’exécution est marquée en échec ;
- le défaut est publié ;
- aucune étape suivante n’est exécutée.

---

## 12. Actionneurs, relais et topologie matérielle

## 12.1 Abstraction actionneur

Un équipement référence un actionneur logique.

L’actionneur référence une affectation matérielle.

```text
Equipment
    -> Actuator
        -> HardwareAssignment
            -> Board
                -> Channel
```

`RelayAssignment` constitue la première forme de `HardwareAssignment`.

## 12.2 RelayAssignment

Une affectation relais décrit :

- son activation ;
- son rôle logique historique ou sa cible actionneur ;
- la carte ;
- le canal ;
- la logique électrique ;
- l’état sûr ;
- éventuellement le partage autorisé ;
- éventuellement une politique d’impulsion.

À terme, le lien direct `role + targetIndex` doit devenir un mécanisme de compatibilité ou d’indexation, pas l’identité fondamentale de l’équipement.

## 12.3 Cartes relais

Chaque carte relais possède :

- identifiant logique ;
- activation ;
- type de contrôleur ;
- adresse I²C ;
- nombre de canaux déclarés ;
- nombre de canaux réellement utilisables ;
- logique globale ou par canal ;
- masque de canaux disponibles ;
- état sûr initial ;
- présence attendue ;
- présence détectée ;
- version ou variante matérielle ;
- diagnostic de communication.

## 12.4 Contrôleurs

Le modèle doit accepter plusieurs familles :

- XL9535 ;
- MCP23017 ;
- contrôleur GPIO natif ;
- nœud d’extension distant futur ;
- autre expander supporté.

Le pilote traduit les opérations génériques en accès matériels.

Le reste du système ne connaît pas les registres du contrôleur.

## 12.5 Bus

Le bus est une ressource matérielle explicite.

Il décrit :

- type : I²C, SPI, GPIO, bus distant futur ;
- instance ;
- broches ;
- fréquence ;
- politique de récupération ;
- état ;
- périphériques attendus ;
- erreurs.

Les cartes ne possèdent pas directement le bus ; elles le référencent.

## 12.6 Validation de topologie

La configuration est invalide si :

- une carte active a une adresse illégale ;
- deux cartes incompatibles partagent la même adresse sur le même bus ;
- un canal excède la capacité ;
- un canal indisponible est affecté ;
- deux actionneurs exclusifs utilisent le même canal ;
- un équipement actif n’a pas d’actionneur valide ;
- un équipement critique n’a pas d’état sûr défini ;
- une carte requise est absente ;
- un graphe de dépendances est incohérent.

## 12.7 Commande atomique

Le gestionnaire matériel maintient une image RAM par carte.

Une modification :

- valide la cible ;
- modifie l’image logique ;
- calcule l’état électrique ;
- écrit uniquement la carte concernée ;
- confirme le résultat logiciel ;
- publie un succès ou un défaut.

Une commande partielle ou ambiguë ne doit pas être masquée.

## 12.8 Démarrage

Au boot :

1. les sorties locales sont placées dans un état électriquement sûr ;
2. les bus sont initialisés ;
3. les cartes sont détectées ;
4. la topologie est validée ;
5. les états sûrs sont appliqués ;
6. la configuration métier est chargée ;
7. les automatismes restent inhibés jusqu’à la fin de l’initialisation ;
8. la reprise éventuelle est décidée ;
9. le système passe en service.

---

## 13. Nœuds d’extension futurs

L’architecture doit permettre un nœud distant, par exemple un microcontrôleur dédié aux relais ou débitmètres.

Le nœud distant est vu comme :

- une carte matérielle ;
- possédant des canaux ;
- exposée par un pilote de transport ;
- supervisée ;
- capable de signaler ses états et défauts.

Le domaine ne doit pas distinguer un relais local d’un relais distant.

Le protocole futur doit prévoir :

- identification ;
- version ;
- capacités ;
- commandes idempotentes ;
- numéro de séquence ;
- acquittement ;
- état courant ;
- timeout ;
- retour à l’état sûr ;
- détection de perte de liaison.

---

## 14. Persistance

## 14.1 Catégories de données

### Configuration durable

- identité de l’installation ;
- zones ;
- équipements ;
- dépendances ;
- capteurs ;
- automatismes ;
- topologie matérielle ;
- politiques ;
- paramètres réseau ;
- préférences d’interface.

### État runtime reconstructible

- état demandé ;
- occupations ;
- timers ;
- intentions en cours ;
- cache de mesures ;
- état de pages.

Cet état n’est pas nécessairement persisté.

### État runtime à reprendre

Certaines données peuvent être persistées dans un journal de reprise :

- exécution d’arrosage en cours ;
- heure de début ;
- durée prévue ;
- progression ;
- équipements mobilisés ;
- motif de redémarrage.

La reprise reste une décision métier, pas une restauration aveugle de sorties.

### Historique

- événements ;
- défauts ;
- exécutions ;
- volumes ;
- mesures agrégées ;
- changements de configuration.

L’historique volumineux doit privilégier la carte SD, avec stratégie de rotation.

## 14.2 Schéma versionné

Chaque bloc persistant doit posséder :

- un identifiant de format ;
- une version ;
- une longueur ;
- un contrôle d’intégrité ;
- éventuellement un numéro de génération ;
- une stratégie de migration.

Une structure binaire C++ brute ne doit pas être considérée comme un contrat durable sans enveloppe de version.

## 14.3 Transactions

Une mise à jour de configuration doit suivre :

```text
Recevoir
-> Valider syntaxiquement
-> Construire une configuration candidate
-> Valider les références
-> Valider les invariants métier
-> Valider la topologie
-> Persister la candidate
-> Relire et vérifier
-> Activer atomiquement
-> Publier l’événement
```

Une configuration partiellement écrite ne doit jamais devenir active.

## 14.4 Double copie

Pour les données critiques, la cible recommandée est :

- copie active ;
- copie précédente valide ;
- numéro de génération ;
- CRC ;
- sélection de la dernière génération valide.

## 14.5 Migration V3 vers V4

La migration doit :

- reconnaître l’ancien schéma ;
- conserver les zones et programmations ;
- créer les équipements électrovannes correspondants ;
- créer la carte relais historique ;
- créer les actionneurs ;
- créer les affectations `Zone N -> carte 0 -> voie N` ;
- reprendre la logique électrique existante ;
- ne pas inventer de pompe ;
- ne pas supprimer les données historiques ;
- enregistrer le résultat comme nouvelle génération ;
- conserver une possibilité de diagnostic de l’origine migrée.

Un échec de migration doit conserver l’ancien bloc intact.

## 14.6 Export et import

V4 doit prévoir un format d’échange humainement inspectable, distinct du stockage binaire interne.

L’export contient :

- version du format ;
- métadonnées ;
- configuration métier ;
- topologie ;
- politiques ;
- contrôles de cohérence ;
- aucune donnée secrète par défaut.

L’import est validé comme une configuration candidate.

---

## 15. Architecture logicielle cible

## 15.1 Couches

```text
Interface utilisateur et API
├── Web UI
├── LCD
├── API REST/JSON
└── Diagnostic

Services applicatifs
├── InstallationService
├── ZoneService
├── EquipmentService
├── AutomationService
├── ManualControlService
├── ConfigurationService
└── DiagnosticService

Domaine
├── ZoneModel
├── EquipmentModel
├── SensorModel
├── AutomationModel
├── DependencyModel
├── ExecutionModel
├── FaultModel
└── PolicyModel

Orchestration runtime
├── AutomationEngine
├── IntentArbiter
├── EquipmentOrchestrator
├── ResourceCoordinator
├── SafetySupervisor
└── StateReconciler

Ports
├── ActuatorPort
├── SensorPort
├── ClockPort
├── PersistencePort
├── EventLogPort
└── NetworkStatusPort

Adaptateurs
├── RelayActuatorAdapter
├── I2CSensorAdapter
├── NvsPersistenceAdapter
├── SdEventLogAdapter
├── WebAdapter
└── LcdAdapter

Infrastructure matérielle
├── BusManager
├── BoardRegistry
├── RelayTopology
├── RelaisManager
├── Drivers
└── Platform
```

## 15.2 Modules métier

### ZoneModel

Définit les zones et leurs politiques.

Ne dépend d’aucun module matériel.

### EquipmentModel

Définit les équipements, capacités, modes, paramètres et références.

### SensorModel

Définit les capteurs, observations, unités et qualité.

### AutomationModel

Définit les règles sans les exécuter.

### DependencyModel

Définit les relations et valide le graphe.

### ExecutionModel

Décrit les instances d’exécution et leur cycle de vie.

### FaultModel

Définit codes, sévérités et états de défaut.

## 15.3 Modules runtime

### AutomationEngine

Évalue les règles et produit des intentions.

### IntentArbiter

Résout les conflits et priorités.

### EquipmentOrchestrator

Construit et exécute les séquences.

### ResourceCoordinator

Gère les ressources partagées et l’exclusivité.

### SafetySupervisor

Peut interdire, arrêter ou forcer un état sûr.

### StateReconciler

Compare état demandé, appliqué et observé.

## 15.4 Modules matériels

### BoardRegistry

Connaît l’inventaire des cartes configurées et détectées.

### BusManager

Gère les bus, erreurs et récupération.

### RelayTopology

Traduit une affectation logique en carte/canal.

### RelaisManager

Exécute une commande binaire sur une affectation valide.

Il ne décide jamais pourquoi la commande existe.

### Drivers

Implémentent les contrôleurs.

## 15.5 Modules existants

Les noms existants peuvent être conservés temporairement, mais leurs responsabilités doivent converger vers cette architecture.

En particulier :

- `ScheduleManager` devient un producteur d’intentions d’arrosage ;
- `RelaisManager` reste matériel ;
- `ConfigManager` devient un adaptateur de persistance et non le propriétaire du métier ;
- les handlers Web appellent les services applicatifs ;
- l’affichage LCD lit des vues d’état, sans piloter le runtime.

---

## 16. Dépendances autorisées

## 16.1 Matrice

| Module | Peut dépendre de | Ne doit pas dépendre de |
|---|---|---|
| Web/LCD | Services applicatifs, vues | Drivers, registres, NVS direct |
| Services applicatifs | Domaine, ports | HTML, contrôleur I²C |
| Domaine | Types fondamentaux | Web, NVS, SD, matériel |
| AutomationEngine | Domaine, horloge, observations | RelaisManager |
| EquipmentOrchestrator | Domaine, ports actionneurs | Pages Web |
| SafetySupervisor | États, observations, ports d’arrêt | UI |
| RelayTopology | Inventaire matériel | Planning, météo |
| RelaisManager | Topologie, drivers | Zones, programmes |
| Drivers | Bus, plateforme | Équipements métier |
| Persistance | Schémas persistants | Décisions métier runtime |

## 16.2 Interdictions

Sont explicitement interdits :

- un handler Web écrivant directement un relais ;
- un programme d’arrosage contenant une adresse I²C ;
- une zone contenant un numéro de canal matériel ;
- un driver connaissant le nom d’une zone ;
- `ConfigManager` déclenchant une action physique ;
- une règle météo commandant directement `RelaisManager` ;
- une page LCD modifiant un état métier sans passer par un service ;
- un retour de capteur utilisé sans qualité ni fraîcheur ;
- une commande manuelle contournant le superviseur de sécurité.

---

## 17. API internes

## 17.1 Principes

Les API internes expriment des opérations métier.

Elles ne doivent pas exposer les détails de stockage ou de bus.

Exemples conceptuels :

```text
requestZoneStart(zoneId, origin, options)
requestZoneStop(zoneId, origin)
requestEquipmentState(equipmentId, desiredState, origin)
setEquipmentMode(equipmentId, mode)
acknowledgeFault(faultId)
validateConfiguration(candidate)
activateConfiguration(candidate)
getInstallationSnapshot()
```

## 17.2 Résultats structurés

Toute opération retourne un résultat distinguant :

- accepté ;
- rejeté ;
- mis en attente ;
- exécuté ;
- partiellement exécuté ;
- échec.

Le résultat contient :

- code ;
- message ;
- cible ;
- identifiant de corrélation ;
- défaut éventuel.

---

## 18. API externes

## 18.1 Versionnement

Les API V4 sont versionnées :

```text
/api/v4/...
```

Une rupture de contrat nécessite une nouvelle version majeure d’API.

## 18.2 Ressources principales

```text
/api/v4/system
/api/v4/installation
/api/v4/zones
/api/v4/equipments
/api/v4/sensors
/api/v4/automations
/api/v4/executions
/api/v4/faults
/api/v4/hardware/buses
/api/v4/hardware/boards
/api/v4/hardware/assignments
/api/v4/configuration
/api/v4/events
```

## 18.3 Lecture

Les réponses d’état distinguent :

- configuration ;
- état runtime ;
- santé ;
- dernière observation ;
- état demandé ;
- état appliqué ;
- état observé ;
- défauts.

## 18.4 Commandes

Les commandes sont des actions explicites, pas des modifications arbitraires d’un snapshot.

Exemples conceptuels :

```text
POST /zones/{id}/commands/start
POST /zones/{id}/commands/stop
POST /equipments/{id}/commands/activate
POST /equipments/{id}/commands/deactivate
POST /faults/{id}/commands/acknowledge
POST /configuration/validate
POST /configuration/activate
```

## 18.5 Concurrence

La configuration expose une révision.

Toute modification doit préciser la révision attendue afin d’éviter l’écrasement silencieux d’une modification concurrente.

## 18.6 Erreurs

Les erreurs API contiennent :

- code stable ;
- message ;
- détail ;
- champ concerné ;
- sévérité ;
- corrélation.

## 18.7 Compatibilité

Les endpoints historiques peuvent rester temporairement disponibles via un adaptateur V3.

Ils ne doivent pas devenir la base des nouvelles fonctions.

---

## 19. Interface Web V4

L’interface est une cliente des API.

Elle ne porte pas les règles de cohérence fondamentales.

Sections cibles :

- tableau de bord ;
- zones ;
- équipements ;
- automatismes ;
- capteurs ;
- matériel ;
- affectations ;
- défauts ;
- événements ;
- maintenance ;
- sauvegarde/restauration.

La configuration matérielle doit visualiser :

```text
Bus
└── Carte
    ├── Canal 0 -> Actionneur -> Équipement
    ├── Canal 1 -> Libre
    └── Canal 2 -> Conflit
```

L’interface doit distinguer :

- configuré ;
- détecté ;
- actif ;
- demandé ;
- réellement observé ;
- en défaut.

---

## 20. Événements, journaux et diagnostic

## 20.1 Bus d’événements interne

Les modules publient des événements structurés :

- configuration activée ;
- intention créée ;
- intention rejetée ;
- exécution démarrée ;
- équipement activé ;
- actionneur commandé ;
- observation reçue ;
- défaut levé ;
- défaut acquitté ;
- carte perdue ;
- reprise décidée.

Les consommateurs ne doivent pas modifier l’événement.

## 20.2 Journal

Chaque événement comporte :

- type ;
- niveau ;
- source ;
- cible ;
- horodatage ;
- corrélation ;
- données utiles.

## 20.3 Niveaux

- trace ;
- information ;
- avertissement ;
- erreur ;
- critique ;
- sécurité.

## 20.4 Diagnostic matériel

Le diagnostic doit exposer :

- bus initialisés ;
- périphériques attendus ;
- périphériques détectés ;
- adresses ;
- erreurs ;
- nombre de réinitialisations ;
- dernier succès ;
- image de sorties ;
- conflits de configuration.

## 20.5 Santé globale

La santé système est calculée :

- `HEALTHY` ;
- `DEGRADED` ;
- `FAULTED` ;
- `SAFE_MODE`.

Elle ne doit pas être déduite uniquement de la connectivité Wi-Fi.

---

## 21. Sécurité fonctionnelle et résilience

## 21.1 États sûrs

Chaque actionneur définit son état sûr.

Par défaut :

- électrovannes fermées ;
- pompe arrêtée ;
- chauffage arrêté ;
- éclairage selon politique ;
- volet selon stratégie spécifique, sans mouvement contradictoire.

## 21.2 Watchdogs et blocages

Les traitements doivent rester non bloquants.

Les longues opérations de stockage, réseau ou Web ne doivent pas empêcher :

- l’arrêt d’un équipement ;
- la surveillance d’une durée maximale ;
- la lecture des défauts critiques ;
- le maintien du watchdog.

## 21.3 Perte Wi-Fi

Le fonctionnement local continue selon la configuration valide.

Le Wi-Fi n’est pas une dépendance de l’arrosage.

## 21.4 Perte d’horloge

Les automatismes temporels sont inhibés ou dégradés selon politique.

Les équipements déjà actifs restent surveillés par une base monotone.

## 21.5 Perte d’une carte

- les commandes vers cette carte échouent ;
- les équipements concernés passent indisponibles ;
- les autres cartes peuvent continuer ;
- les dépendances sont réévaluées ;
- une pompe ne doit pas rester active si la vanne requise ne peut pas être commandée.

## 21.6 Reboot

Au redémarrage :

- aucune image mémoire précédente ne doit être appliquée aveuglément ;
- les sorties passent à l’état sûr ;
- le journal de reprise est analysé ;
- une décision explicite est prise ;
- la reprise éventuelle repasse par l’orchestrateur.

## 21.7 Durées maximales

Tout équipement capable de causer un dommage doit avoir une durée maximale indépendante de la règle qui l’a activé.

---

## 22. Invariants normatifs

Les invariants suivants sont obligatoires.

### Domaine

1. Une zone n’est jamais identifiée par une voie relais.
2. Un équipement n’est jamais identifié par son adresse matérielle.
3. Un relais n’est jamais considéré comme un équipement métier.
4. Les identités métier restent stables malgré les réordonnancements.
5. Une référence supprimée ne doit jamais être redirigée implicitement vers un autre objet.

### Commande

6. Tout pilotage matériel provient d’une intention validée ou d’une action de sécurité.
7. Une commande manuelle passe par le même orchestrateur que les automatismes.
8. Aucune couche de présentation ne commande directement le matériel.
9. L’état demandé, appliqué et observé reste distinct.
10. Une commande non confirmée n’est pas présentée comme réussie.

### Relais et cartes

11. `RelaisManager` ne connaît pas les zones, plannings ou règles.
12. `RelayTopology` ne prend aucune décision métier.
13. Une affectation invalide échoue sans activation physique.
14. Aucun doublon de canal n’est accepté sans politique de partage explicite.
15. Toute carte active possède une adresse, un contrôleur et une capacité valides.
16. L’absence d’une carte requise est un défaut visible.
17. L’initialisation applique des états sûrs avant l’activation des automatismes.

### Automatismes

18. Un automatisme produit une intention, jamais une écriture de sortie.
19. La pluie sautant une occurrence d’intervalle ne décale pas l’ancrage.
20. Les priorités et arbitrages sont déterministes.
21. Une mesure périmée n’est pas utilisée comme valide.
22. Une règle définit son comportement quand une donnée requise manque.

### Persistance

23. Toute configuration persistée est versionnée et contrôlée.
24. Une migration n’écrase jamais la dernière copie valide avant succès.
25. Une configuration candidate est entièrement validée avant activation.
26. Une mise à jour échouée conserve la configuration active.
27. Le stockage runtime ne peut pas réactiver directement une sortie au boot.

### Résilience

28. La perte du Web ou du Wi-Fi ne stoppe pas le moteur local.
29. Les temporisations critiques utilisent une base monotone.
30. Tout équipement critique possède un état sûr.
31. Toute activation potentiellement dangereuse possède une limite maximale.
32. Une dépendance défaillante provoque un arrêt ou un refus cohérent de la séquence.
33. La pompe n’est jamais maintenue uniquement parce qu’un état logiciel ancien le demandait.
34. Les erreurs matérielles sont visibles dans l’état système et le journal.

### Compatibilité et périmètre

35. Le profil initial V4 reproduit `Zone N -> carte 0 -> voie N`.
36. Les fonctions validées d’arrosage, météo, pluie et intervalle ne sont pas modifiées sans chantier dédié.
37. La migration V4 ne doit pas dépendre de l’interface Web.
38. SD et LittleFS restent des adaptateurs de stockage, pas des dépendances du domaine.
39. Toute rupture d’un invariant nécessite une décision d’architecture versionnée.
40. Aucun développement V4 ne doit réintroduire un couplage direct `zone -> relais`.

---

## 23. Décisions d’architecture

### ADR-V4-001 — Équipement comme objet central

**Décision :** l’équipement est l’objet piloté.  
**Conséquence :** le relais devient une ressource d’actionnement.

### ADR-V4-002 — Intentions avant commandes

**Décision :** les automatismes et interfaces produisent des intentions.  
**Conséquence :** l’orchestrateur et le superviseur gardent le contrôle.

### ADR-V4-003 — Topologie indépendante

**Décision :** la topologie matérielle est indépendante du modèle métier.  
**Conséquence :** un changement de carte ne change pas l’identité des zones ou équipements.

### ADR-V4-004 — Configuration candidate atomique

**Décision :** toute modification complexe est validée hors ligne puis activée atomiquement.  
**Conséquence :** aucune configuration partielle ne devient active.

### ADR-V4-005 — Reprise explicite

**Décision :** un reboot ne restaure pas directement les sorties.  
**Conséquence :** toute reprise repasse par une décision métier.

### ADR-V4-006 — Capteurs qualifiés

**Décision :** une mesure inclut qualité et fraîcheur.  
**Conséquence :** aucune règle ne consomme une simple valeur sans contexte.

---

## 24. Stratégie de transition

La transition ne doit pas être une réécriture totale.

Elle s’effectue par strangulation progressive de l’architecture existante.

### Étape de compatibilité

Le comportement actuel reste disponible derrière des adaptateurs.

### Étape de double représentation

Les objets V4 sont créés en mémoire à partir de la configuration actuelle, sans changer la persistance.

### Étape de double chemin contrôlé

Une zone pilote l’orchestrateur V4, qui utilise encore le chemin relais existant comme adaptateur.

### Étape de bascule

Le nouveau chemin devient la source de vérité runtime.

### Étape de persistance

La configuration V4 est persistée après stabilisation du modèle.

### Étape de retrait

Les anciens chemins directs sont supprimés après validation et migration.

---

## 25. Roadmap d’implémentation

Chaque phase doit produire :

- une décision claire ;
- un périmètre ;
- des tests ;
- un checkpoint ;
- une compilation validée ;
- une documentation mise à jour ;
- une stratégie de retour arrière.

## Phase 0 — Gel architectural

Objectif :

- valider ce document ;
- enregistrer les ADR ;
- définir le vocabulaire ;
- identifier les divergences avec l’existant ;
- interdire toute persistance prématurée du modèle incomplet.

Livrables :

- document V4 validé ;
- matrice des modules existants vers modules cibles ;
- liste des dettes et écarts ;
- tag d’architecture.

Aucun changement runtime.

## Phase 1 — Modèle de domaine isolé

Objectif :

- introduire les concepts V4 sans changer le comportement.

Périmètre :

- identités ;
- équipements ;
- capacités ;
- capteurs ;
- observations ;
- dépendances ;
- intentions ;
- défauts ;
- exécutions.

Contraintes :

- aucune écriture NVS ;
- aucune nouvelle commande matérielle ;
- aucun changement Web obligatoire.

Critère de sortie :

- modèle cohérent ;
- validation des graphes ;
- tests unitaires hôte si possible.

## Phase 2 — Inventaire matériel générique

Objectif :

- généraliser cartes, bus et canaux.

Périmètre :

- registre des bus ;
- registre des cartes ;
- capacités ;
- état détecté/configuré ;
- validation de topologie ;
- pilotes existants derrière une interface commune.

Compatibilité :

- profil carte unique historique.

Critère de sortie :

- carte unique inchangée ;
- plusieurs cartes représentables ;
- conflits détectés avant commande.

## Phase 3 — Abstraction actionneur

Objectif :

- séparer équipement et relais.

Périmètre :

- actionneur binaire ;
- affectation matérielle ;
- adaptateur relais ;
- état demandé/appliqué ;
- erreurs structurées.

Critère de sortie :

- aucun nouvel appel métier direct à `RelaisManager`.

## Phase 4 — Orchestrateur minimal

Objectif :

- faire passer une zone par l’orchestrateur sans modifier le résultat fonctionnel.

Périmètre :

- intentions ;
- machine d’états ;
- démarrage/arrêt d’une électrovanne ;
- temporisations non bloquantes ;
- événements ;
- compensation élémentaire.

Critère de sortie :

- comportement de zone identique au checkpoint ;
- état demandé/appliqué visible ;
- arrêt sûr après erreur.

## Phase 5 — Pompe et ressources partagées

Objectif :

- gérer la première dépendance réelle.

Périmètre :

- pompe ;
- vanne maîtresse optionnelle ;
- précharge ;
- post-fonctionnement ;
- anti-cycles courts ;
- comptage de demandes ;
- durée maximale ;
- échec de dépendance.

Critère de sortie :

- deux zones partageant une pompe ;
- transition sans coupure inutile ;
- arrêt sûr si la vanne échoue.

## Phase 6 — Capteurs et supervision hydraulique

Objectif :

- intégrer les observations qualifiées.

Périmètre :

- pression ;
- débit ;
- niveau ;
- fraîcheur ;
- seuils ;
- défaut absence de débit ;
- défaut fuite ou débit inattendu.

Critère de sortie :

- aucune valeur périmée traitée comme valide ;
- arrêt sûr sur défaut configuré.

## Phase 7 — Persistance V4 et migration

Objectif :

- rendre le modèle durable.

Périmètre :

- schéma versionné ;
- copie précédente ;
- migration V3 ;
- import/export ;
- validation candidate ;
- activation atomique.

Critère de sortie :

- migration sans perte ;
- retour à la dernière configuration valide ;
- reboot cohérent.

## Phase 8 — API V4

Objectif :

- exposer le domaine et le runtime.

Périmètre :

- endpoints versionnés ;
- snapshots ;
- commandes ;
- révisions ;
- erreurs structurées ;
- adaptateur de compatibilité V3.

Critère de sortie :

- aucun endpoint ne contourne les services ;
- commandes corrélées et traçables.

## Phase 9 — Interface Web V4

Objectif :

- administrer l’installation complète.

Périmètre :

- équipements ;
- cartes ;
- affectations ;
- capteurs ;
- automatismes ;
- défauts ;
- visualisation configuré/détecté/appliqué.

Critère de sortie :

- conflits bloqués avant activation ;
- configuration candidate vérifiable.

## Phase 10 — Automatismes climatiques

Objectif :

- généraliser au-delà de l’arrosage temporel.

Périmètre :

- règles à seuil ;
- hystérésis ;
- serre ;
- éclairage ;
- ventilation ;
- brumisation ;
- arbitrage.

Critère de sortie :

- aucune règle ne commande directement un actionneur.

## Phase 11 — Nœuds d’extension

Objectif :

- déporter relais et comptage.

Périmètre :

- transport ;
- découverte ;
- capacités ;
- watchdog distant ;
- commandes idempotentes ;
- perte de liaison ;
- état sûr.

Critère de sortie :

- un actionneur distant est interchangeable avec un actionneur local au niveau métier.

## Phase 12 — Durcissement

Objectif :

- rendre V4 exploitable durablement.

Périmètre :

- tests de panne ;
- endurance ;
- corruption de stockage ;
- perte I²C ;
- reboot en charge ;
- perte d’horloge ;
- montée de version ;
- diagnostic ;
- documentation de maintenance.

---

## 26. Stratégie de tests

### Tests de domaine

- validation des références ;
- cycles de dépendances ;
- priorités ;
- politiques ;
- qualité des mesures ;
- transitions d’état.

### Tests d’orchestration

- séquences normales ;
- dépendances ;
- partage de pompe ;
- échec d’une étape ;
- arrêt demandé ;
- sécurité ;
- reprise.

### Tests matériels

- carte unique ;
- cartes multiples ;
- adresse absente ;
- adresse dupliquée ;
- canal invalide ;
- logique inversée ;
- perte bus ;
- réinitialisation carte.

### Tests de persistance

- migration ;
- CRC invalide ;
- coupure pendant écriture ;
- copie précédente ;
- import invalide ;
- référence orpheline.

### Tests système

- reboot pendant arrosage ;
- routeur indisponible ;
- NTP indisponible ;
- SD absente ;
- capteur périmé ;
- pompe active avec vanne en défaut ;
- exécutions concurrentes ;
- durée maximale atteinte.

---

## 27. Critères d’acceptation de l’architecture V4

L’architecture V4 est considérée respectée lorsque :

- les zones ne contiennent aucun détail de relais ;
- les équipements sont les cibles des commandes ;
- les automatismes produisent des intentions ;
- l’orchestrateur gère les dépendances ;
- le superviseur peut bloquer ou arrêter ;
- les relais sont accessibles uniquement via un port actionneur ;
- les cartes sont décrites par un inventaire matériel ;
- les observations portent qualité et fraîcheur ;
- la configuration est versionnée et activée atomiquement ;
- l’API est versionnée ;
- l’état demandé est distinct de l’état réel ;
- la compatibilité historique est assurée par un profil de migration ;
- les défauts sont structurés et visibles ;
- le redémarrage revient d’abord à un état sûr.

---

## 28. Points volontairement ouverts

Les décisions suivantes restent à préciser par des ADR dédiées avant implémentation :

- format exact des identifiants stables sur microcontrôleur ;
- représentation compacte des paramètres spécifiques ;
- format d’export ;
- politique par défaut de reprise d’un arrosage interrompu ;
- stockage exact de l’historique ;
- protocole des nœuds distants ;
- nombre maximal d’équipements, capteurs, cartes et automatismes ;
- stratégie d’authentification de l’API locale ;
- niveau de redondance des pompes ;
- politique d’exécution simultanée des zones ;
- gestion des équipements à commande impulsionnelle ou bidirectionnelle.

Ces points ouverts ne remettent pas en cause la séparation architecturale définie ici.

---

## 29. Source de vérité et gouvernance

Ce document doit être placé dans :

```text
docs/architecture/AQUALOOK_V4_TARGET_ARCHITECTURE.md
```

Il devient la référence supérieure pour les chantiers V4.

Les documents antérieurs restent utiles pour l’historique et les détails de transition, notamment :

- `RELAY_TOPOLOGY.md` ;
- `EQUIPMENT_MODEL_ROADMAP.md` ;
- `CHECKPOINT_2026-07-06_relay-assignment-roles_COMPILE_OK.md`.

En cas de contradiction :

1. une décision explicitement validée et plus récente prévaut ;
2. l’écart doit être documenté ;
3. ce document doit être mis à jour dans le même chantier ;
4. aucun changement implicite d’architecture n’est accepté.

Chaque chantier V4 doit préciser :

- la phase de roadmap concernée ;
- les invariants impactés ;
- les modules touchés ;
- les migrations nécessaires ;
- les tests ;
- le checkpoint de sortie.

---

## 30. Conclusion

AquaLook V4 n’est pas une extension du nombre de relais.

C’est le passage :

```text
d’un programmateur directement lié à ses sorties
```

vers :

```text
une plateforme locale d’automatisation
centrée sur les équipements,
pilotée par des intentions,
orchestrée selon des dépendances,
observée par des capteurs qualifiés,
et exécutée sur une topologie matérielle interchangeable.
```

La priorité d’implémentation n’est donc pas d’ajouter rapidement de nouveaux champs NVS ou de nouvelles pages.

La priorité est de stabiliser successivement :

1. le domaine ;
2. les identités et relations ;
3. les actionneurs ;
4. l’orchestration ;
5. la sécurité ;
6. puis seulement la persistance et les interfaces.

Cette séquence évite d’ancrer AquaLook V4 sur une représentation encore trop proche du matériel et garantit une évolution durable vers les pompes, capteurs, serres, extensions et automatismes futurs.
