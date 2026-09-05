# AquaLook — architecture évolutive (boussole V4)

**Objet.** Ce document est une **boussole**, pas un plan d'implémentation. Il fixe
le modèle mental, les quelques interfaces (« coutures ») à garder propres, et un
menu des évolutions possibles rangées par couche. But : rendre l'ajout de
capacités **bon marché** et éviter de se fermer des portes — sans pour autant
tout construire d'avance.

**Historique.** Le modèle décrit ici EST l'intention première de V4. Le profil
`LEGACY` a été le repli pragmatique pour livrer un système qui arrose vraiment.
Aujourd'hui la base est saine (robustesse durcie, config migrable, invariants
posés) : on peut réancrer le développement sur V4.

---

## 1. Principe directeur

L'évolutivité sans limite ne vient pas de « tout prévoir ». Elle vient de
**rendre l'ajout local** : un capteur, un actionneur, un nœud distant, une règle
— sans toucher au cœur. Le piège inverse est tout aussi coûteux : bâtir un cadre
générique géant avant d'en avoir l'usage (l'état actuel de V4 en est un
avertissement — beaucoup de conception, câblée seulement sur la pompe).

> Règle : **coutures propres, remplissage incrémental.**
> On soigne les interfaces ; on ne remplit chaque couche que lorsqu'un besoin
> réel l'exige.

---

## 2. Le modèle en couches

```
5. UI / Cloud        LCD, web module, appli cloud (config, supervision, contrôle)
4. Automation        Scénarios : QUAND (temps, météo, capteur, débit, présence)
                     ALORS (arroser, éclairer, ventiler, alerter)
3. Ressources        Zones, pompes, débitmètres, éclairages — concepts MÉTIER
2. Endpoints (ports) Typés : RELAY, DIGITAL, COUNTER, ANALOG, SENSOR, VIRTUAL ;
                     direction INPUT / OUTPUT / BIDIRECTIONAL ; un rôle
1. Nœuds             Principal, satellites — chacun joignable par un transport
0. Transports        I2C local, GPIO local, RS485, ESP-NOW, WiFi
   ⊥ transversal     Sécurité / dégradation · persistance versionnée ·
                     notifications · OTA
```

Force du découpage : **chaque couche ignore le détail de celle du dessous.** Un
scénario (couche 4) parle de « débit zone 2 », jamais d'« impulsions sur le
GPIO 34 d'un satellite RS485 ». Déplacer un capteur du contrôleur vers un
satellite devient un changement de **configuration** aux couches 0-1, invisible
pour les couches 3-4.

---

## 3. Les quatre coutures à garder propres

Si l'on ne devait soigner que quatre interfaces, ce sont celles-ci. Tant
qu'elles restent nettes, on peut ajouter sans limite.

### 3.1 L'endpoint typé
Tout objet physique est un **port** avec un *type* (relais, TOR, compteur,
analogique, capteur), une *direction*, un *rôle*. Déjà présent dans le modèle V4
(`AQUALOOK_V4_BOARD_PORT_MODEL.md`). Le débit, c'est le type **`COUNTER`** — déjà
prévu. Ajouter une classe de matériel = **un pilote** derrière cette couture.

### 3.2 Le nœud + transport
Un endpoint « vit sur un nœud joignable par un transport ». Si cette
**localisation est une donnée**, « local » et « satellite » ne sont plus une
bifurcation d'architecture mais un champ de config. Les créneaux `REMOTE`,
`RS485`, `UART`, `CAN` réservés dans `platformio.ini` (éteints) sont cette
couture.

### 3.3 La ressource logique
Une « zone », une « pompe », un « débitmètre » est un concept **métier** lié à un
ou plusieurs endpoints. La logique ne connaît que la ressource ; on recâble le
matériel dessous sans réécrire la logique.

### 3.4 La règle sur les ressources
Le moteur de scénarios ne référence que des **ressources et des conditions**,
jamais du matériel. Le même scénario marche que le capteur soit local ou
distant, MCP23017 ou GPIO direct.

---

## 4. Principes d'évolutivité (déjà appliqués)

- **Config déclarative, jamais de code en dur.** Une nouvelle installation = de
  la configuration, pas un firmware. (Fait pour les E/S : `IoExpanderConfig`.)
- **Dégradation sûre à chaque couche.** Capteur manquant, satellite tombé,
  lecture périmée → comportement de repli *défini*. Briques existantes :
  `BootLoopGuard` / mode dégradé, invariant « le local gagne », intégrité NVS.
- **Persistance versionnée et migrable.** Le modèle grossit, le schéma évolue ;
  la discipline de migration (durcie en septembre 2026) permet d'évoluer sans
  casser les installations existantes. Chaque couche stocke dans son espace NVS.
- **Peu d'interfaces, mais stables.** Moins il y a de contrats internes et plus
  ils sont stables, plus on peut échanger les implémentations dessous.
- **L'utilisateur déclenche.** Aucune action irréversible ni mise à jour n'est
  imposée à distance ; le système assiste, l'utilisateur décide.

---

## 4bis. La couche transversale d'erreurs — règle non négociable

**Toute erreur constatée à n'importe quel étage doit remonter à la couche
transversale qui les gère.** La surveillance est ce qui permet de *comprendre*
le système ; elle doit être connectée à **tous** les étages, pas seulement aux
plus visibles.

Cette couche **existe déjà** et n'est pas à réinventer : `FaultManager`
(vocabulaire de défauts : `WIFI`, `FILESYSTEM`, `SOFTWARE`, `STORAGE_SD`,
`CONFIG_PERSIST`, `DISPLAY_ALLOC`, `MEMORY_LOW`, `TIME_UNSYNCED`, `BOOT_LOOP`,
`RELAY_I2C`), `EventLog`, `IncidentManager`, et les notifications. Le travail
consiste à **y raccorder V4**, pas à bâtir un étage de plus.

### Comment une couche rend compte, selon sa nature

| couche | façon de rendre compte |
|--------|------------------------|
| **pure** (domaine, moteur d'exécution) | rend un **résultat typé**. Elle ne connaît ni `FaultManager` ni le journal — l'y lier la rendrait dépendante de la plateforme et intestable |
| **adaptation** (runtime, backends) | **traduit** ces résultats en défaut et en trace : c'est elle qui parle à la couche transversale |
| **application** (managers, orchestration) | journalise la décision et son motif |

> **Ce qui est interdit : qu'une erreur meure entre les deux.** Un résultat
> d'échec rendu par une couche pure et ignoré par son appelant est un défaut
> silencieux — le pire des cas, puisqu'il ôte tout moyen de comprendre.

### Dette constatée le 5 septembre 2026

Audit du raccordement de V4 : sur onze composants, **un seul signalait un
défaut**. Les pannes I2C du chemin V4 étaient **entièrement silencieuses** —
là où le moteur historique allume `RELAY_I2C`, le journalise et refuse de
piloter une carte absente. Corrigé pour `V4RelayPhysicalBackend` ; le reste
des étages est à passer en revue au même titre, en distinguant le silence
**légitime** d'une couche pure du silence **fautif** d'un appelant.

---

## 5. État actuel des couches

| Couche | État |
|--------|------|
| 0 Transports | I2C + GPIO locaux en service. `RS485`/`REMOTE`/`UART`/`CAN` réservés, éteints. |
| 1 Nœuds | Un seul nœud (principal). Satellites : prévus, non implémentés. |
| 2 Endpoints | Squelette V4 (catalogue, modèle de ports). Pilote concret : **`IoExpander`** (TOR configurable). Topologie relais modélisée **et persistable** (`RelayTopologyStore`, NVS `aq_topo`) ; à défaut, dérivation legacy. |
| 3 Ressources | Zones : chemin **LEGACY** en production. `EquipmentManager` V4 : câblé seulement pour la pompe. |
| 4 Automation | **Rien.** Le planning d'arrosage existe (legacy) mais pas de moteur de règles généralisé. |
| 5 UI / Cloud | Web module + appli cloud + LCD en service ; sauvegarde cloud opérationnelle. |

---

## 6. Menu des évolutions possibles

Chaque évolution est rangée par couche, avec ce qu'elle exige et les coutures
qu'elle touche. On voit ainsi ce qu'on peut **repousser** sans se fermer de porte.

| Évolution | Couche(s) | Exige | Coutures |
|-----------|-----------|-------|----------|
| Présence des vannes | 2-3 | Pilote entrée MCP (fait) + rôle zone | 3.1, 3.3 |
| Débitmètres | 0-3 | GPIO direct + PCNT, type `COUNTER` | 3.1, 3.3 |
| Éclairage / ventilation | 2-3 | Pilote sortie (fait) + ressource | 3.1, 3.3 |
| Capteurs humidité / température | 2-3-4 | Pilote `ANALOG`/`SENSOR` + condition | 3.1, 3.3, 3.4 |
| Nœud satellite (station) | 0-1 | Transport RS485/ESP-NOW + protocole + repli de sécurité | 3.2 |
| Multi-stations | 1-3 | Plusieurs nœuds + ressources réparties | 3.2, 3.3 |
| Moteur de scénarios | 4 | Déclencheurs, conditions, actions, persistance, UI | 3.3, 3.4 |

Note débit : le comptage rapide se règle **sur le contrôleur principal** grâce
au PCNT (comptage matériel, insensible à la charge CPU). Un satellite ne
s'impose que pour le **manque de GPIO** ou la **distribution physique**, pas
pour la vitesse.

---

## 7. Stratégie de bascule LEGACY → V4

> Objectif : faire de V4 le moteur du développement **sans jamais risquer le
> jardin**, que le legacy arrose aujourd'hui.

### 7.1 Figer le legacy comme référence
Le profil `LEGACY` devient la **référence stable, non évolutive** : plus aucune
nouvelle fonctionnalité ne s'y ajoute. Il reste flashable et sert de **repli
instantané**. C'est le filet.

### 7.2 Ne PAS faire de bascule d'un bloc
Le legacy arrose un vrai jardin ; V4 est encore partiel et non éprouvé en
production. Un basculement d'un coup du contrôleur en service sur un moteur
partiel serait exactement le risque qu'on s'est interdit tout au long du projet.
On procède en **strangler-fig** (remplacement progressif, couche par couche),
pas en bascule.

### 7.3 Parité avant promotion
Avant que V4 ne pilote un vrai jardin, il doit **égaler le comportement éprouvé**
du legacy : planning, pluie, météo, manuel, contrôle relais, mode dégradé, tous
les invariants — puis être validé sur matériel réel dans la durée (banc, puis
carte de rechange, jamais la production d'abord).

### 7.4 Promotion validée, legacy en rollback
V4 ne passe en production que lorsqu'il a atteint la parité ET été validé sur
matériel réel. Le legacy reste le rollback immédiat.

### 7.5 Le premier domino (sans risque)
Sur la **carte de développement (S3)**, faire piloter les zones par le **modèle
de ports V4** au lieu du chemin legacy, et **persister la topologie relais** (la
brique qui manque). Zéro impact production. Cela transforme V4 de « squelette
conçu » en « chemin qui arrose pour de vrai », sur le banc. Dans la foulée,
**unifier `IoExpander` sous le modèle de ports V4** pour qu'il cesse d'être un
îlot parallèle.

---

## 8. En une phrase

On gèle le legacy comme filet, on soigne les quatre coutures, et on remplit V4
**une tranche validée à la fois** — le jardin n'étant jamais servi par du code
non éprouvé.
