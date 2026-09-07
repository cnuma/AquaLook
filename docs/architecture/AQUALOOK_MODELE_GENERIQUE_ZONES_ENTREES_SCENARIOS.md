# Rendre le modèle générique : zones, entrées, scénarios

*Rédigé le 7 septembre 2026, sur une question de l'utilisateur : « cette notion
de zone n'est pas totalement générique et demande réflexion […] dans un futur
proche nous allons travailler une nouvelle interface qui permettra de réaliser
des scénarios. Comment bien organiser tout ça en prenant en compte les futures
entrées ? »*

Ce document ne décide rien. Il pose ce que « zone » veut dire aujourd'hui, ce
qui casse quand on y ajoute des entrées et des scénarios, et propose une
trajectoire. **La décision qui bloque tout le reste est en section 5.**

---

## 1. Ce que « zone » désigne aujourd'hui — quatre choses à la fois

Le mot recouvre quatre notions distinctes, qui partagent le même index `0..7` :

| notion | où elle vit | exemple |
|---|---|---|
| **une clé de routage** | l'index `z`, partout | `setRelay(z, true)` |
| **une identité** | `CfgZone::name`, `_zoneColors[z]` | « Potager distant », magenta |
| **un programme d'arrosage** | `CfgZone::daySlots`, `rain`, `mode` | lun/mer/ven 06:30, 5 min |
| **un raccordement physique** | `RelayAssignment{ROLE_ZONE_VALVE, targetIndex=z}` | carte 1, voie 0 |

Le quatrième point a déjà été séparé : depuis la refonte V4, le câblage est une
donnée de configuration, et une zone peut parfaitement n'être raccordée à rien.

**Les trois autres restent soudés à l'index.** C'est là qu'est le problème.

---

## 2. Ce qui casse quand on ajoute des entrées

Une sonde d'humidité, un débitmètre, une sonde de température LoRa, un
bouton : aucun n'a de place dans le modèle actuel.

- `RelayTopology` décrit des **sorties** binaires. Une entrée a une direction,
  un type de mesure (binaire, impulsions, analogique), une unité, une période
  d'échantillonnage. Rien de tout cela n'existe.
- Une entrée n'appartient pas nécessairement à une zone. Un pluviomètre sert à
  tout le jardin ; un débitmètre peut être posé en amont de plusieurs vannes.
  Le modèle « une chose = une zone » ne sait pas exprimer ça.
- Le pilote V4 (`BinaryActuatorDriverOps`) est écrit pour agir, pas pour lire.
  Il lui manque un symétrique côté capteurs.

## 3. Ce qui casse quand on ajoute des scénarios

Un scénario s'énonce naturellement ainsi :

> **quand** l'humidité du potager passe sous 30 %
> **et si** il n'a pas plu depuis 2 jours et qu'il est entre 6 h et 9 h
> **alors** arroser le potager 10 minutes, et me le notifier

Trois observations :

1. Le sujet est **« le potager »**, pas « la zone 5 ». Le scénario parle
   d'identités, pas d'index.
2. L'action ne porte pas forcément sur une vanne de zone : « allumer
   l'éclairage », « démarrer la pompe » sont du même ordre.
3. **Un programme d'arrosage EST déjà un scénario**, avec un déclencheur
   horaire et une condition de pluie. Les deux mécanismes ne doivent pas
   coexister en s'ignorant.

---

## 4. Le modèle proposé — quatre couches au lieu d'une

```
  SCENARIOS      quand <déclencheur> si <conditions> alors <actions>
      |                (le programme d'arrosage en est un cas)
      v
  EQUIPEMENTS    identité stable : nom, couleur, type, unité
      |          vanne | pompe | éclairage | sonde | débitmètre
      v
  POINTS         une voie sur une carte : direction, type de signal
      |          (le câblage actuel, étendu aux entrées)
      v
  TRANSPORTS     I2C | RS485 | IP | LoRa      (déjà en place)
```

**Point** — ce que le câblage décrit déjà, augmenté d'une *direction* (sortie /
entrée) et d'un *type de signal* (binaire, impulsions, analogique). La table
existe, elle gagne deux colonnes.

**Équipement** — ce qu'un point pilote ou mesure. **C'est ici que vit
l'identité** : nom, couleur, type. Un équipement peut n'être lié à aucun point
— l'état « non affecté », que le module sait déjà nommer et afficher.

**Zone** — ne disparaît pas. Les gens pensent en zones de jardin, et c'est une
bonne abstraction pour l'utilisateur. Mais elle cesse d'être la clé technique :
une zone devient *un nom + une vanne + éventuellement des capteurs associés +
un programme*.

**Scénario** — la couche de règles. Le programme d'arrosage actuel s'y exprime
sans perte : déclencheur horaire, condition de pluie, action sur une vanne.

---

## 5. La décision qui bloque tout : des identifiants stables

**C'est le seul point qui doit être tranché avant d'écrire la moindre ligne
d'interface de scénarios.**

Aujourd'hui, tout référence une zone par son **index**. Un scénario qui dit
« arroser la zone 5 » et un index qui se décale — parce qu'une zone est
supprimée, insérée, réordonnée — donnent un scénario qui arrose silencieusement
autre chose. C'est la même famille de faute que le câblage deviné : une
référence qui a l'air juste et qui désigne le mauvais objet.

Il faut donc, **avant** les scénarios :

- un identifiant **stable** par zone et par équipement, attribué une fois,
  jamais réutilisé après suppression ;
- l'index conservé uniquement comme **position d'affichage** ;
- les références entre objets (scénario → équipement, zone → vanne) faites par
  identifiant.

C'est peu de code aujourd'hui — un `uint16` par zone, une migration de schéma
NVS de plus, du même type que celle des couleurs. C'en est beaucoup une fois
que des scénarios écrits par l'utilisateur pointent sur des index.

---

## 6. Trajectoire proposée — chaque étape livrable seule

| # | étape | ce qu'elle apporte | coût |
|---|---|---|---|
| 1 | **Identifiants stables** de zone | rend toute référence sûre | faible (schéma NVS +1) |
| 2 | **Points d'entrée** dans le câblage | les capteurs deviennent descriptibles | moyen |
| 3 | **Lecture** dans le domaine V4 | symétrique de `BinaryActuatorDriverOps` | moyen |
| 4 | **Équipements** porteurs de l'identité | zone = vanne + programme | moyen |
| 5 | **Scénarios** | la nouvelle interface | élevé |

L'ordre n'est pas négociable sur les étapes 1 et 5 : la première protège la
dernière. Les étapes 2 à 4 peuvent être réordonnées selon ce qui arrive en
premier sur le banc — une sonde LoRa pousserait 2 et 3, une carte multi-rôles
pousserait 4.

---

## 7. Ce qui est déjà acquis

La refonte V4 a déjà posé plusieurs fondations de ce modèle, ce qui rend la
suite moins lourde qu'elle n'en a l'air :

- le **transport est une donnée** (I2C, RS485, IP, LoRa) — étape 4 déjà faite
  pour les sorties ;
- le **câblage est explicite**, plus jamais déduit ;
- les **rôles** existent déjà (`ROLE_ZONE_VALVE`, pompe, auxiliaire) : la
  notion d'équipement est amorcée ;
- l'état **« non affecté »** est nommé, affiché et journalisé comme une
  configuration incomplète, pas comme une panne — un équipement sans point
  suivra exactement le même chemin ;
- la **couche transversale d'erreurs** existe et sait déjà distinguer un défaut
  d'une configuration incomplète.

---

## 8. Question ouverte, à trancher par l'utilisateur

Une zone doit-elle rester un **objet de premier rang** (un nom, une vanne, un
programme), ou devenir simplement une **étiquette** portée par un équipement de
type vanne ?

- *Objet de premier rang* : plus proche de la façon dont on parle d'un jardin,
  et l'interface actuelle continue de fonctionner presque telle quelle.
- *Étiquette* : plus générique, un seul type d'objet à gérer, mais l'interface
  d'arrosage devient un cas particulier à reconstruire.

Recommandation : **objet de premier rang**. La généricité se gagne en dessous
(points, équipements, scénarios) sans sacrifier le vocabulaire du jardin
au-dessus. Rien n'interdit plus tard de créer des zones qui ne sont pas des
zones d'arrosage.
