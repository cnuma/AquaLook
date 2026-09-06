# AquaLook V4 — Critère de fiabilité pour promouvoir le moteur

**Objet.** Définir, sans ambiguïté, à partir de quand le moteur V4 est jugé
**fiable** au point de piloter — d'abord au banc, puis en production. La fiabilité
est le critère premier ; ce document est le contrat que je m'impose. Aucune
promotion ne se fait sur une impression ou un compteur rond.

## Principe cardinal

**Zéro divergence inexpliquée.** Pas une moyenne, pas un pourcentage. Sur tout
l'échantillon, le nombre de désaccords non expliqués doit être **exactement 0**.

- Une divergence qui change **si** une zone s'arrose, ou **combien de temps**, ou
  **quelle sortie** est pilotée → **bloquante**, elle arrête la progression.
- Une divergence bénigne (détail sans effet sur l'arrosage) → **comprise et
  réconciliée**, ou consignée avec sa justification. Jamais moyennée, jamais
  ignorée.

## Prérequis — rendre « ACCORD » digne de confiance

Le test de parité actuel est grossier : il vérifie que V4 produit un plan
*valide*. Ce n'est pas assez. Avant toute mesure sérieuse, on renforce le
comparateur pour qu'« ACCORD » signifie **même résultat physique** : V4
piloterait **la même voie** (carte + canal) dans **le même état** (ouvert/fermé)
que le legacy, avec la **même durée**. Tant que ce comparateur n'est pas
strict, les gates ci-dessous ne comptent pas.

## Les gates, dans l'ordre

### Gate 0 — Couverture des décisions (parité shadow, au banc, par le série)
Chaque type de décision doit avoir été **exercé** ET **en ACCORD** :

- [ ] start / stop **manuel**, pour **chaque** zone ;
- [ ] start / stop **planifié** (créneaux jour), pour **chaque** zone ;
- [ ] mode **intervalle** (arrosage tous les N jours) ;
- [ ] **pluie** : la météo dit qu'il pleut → le legacy n'arrose pas → V4 doit
      être d'accord pour **NE PAS** arroser ;
- [ ] **coupure durée max** (garde-fou de durée) ;
- [ ] **multi-zones** simultanées ;
- [ ] **redémarrage** avec un arrosage en cours ;
- [ ] **mode dégradé** : V4 se comporte de façon aussi sûre que le legacy.

Chaque case cochée = **au moins 5 occurrences en ACCORD**, avec un cumul global
**désaccord = 0**.

### Gate 1 — Soak shadow (endurance)
V4 tourne en shadow, le legacy pilote, sur des cycles réels :

- **≥ 7 jours de campagne** ;
- **≥ 100 cycles** réels (start+stop) toutes zones confondues, **cumulés au
  travers des redémarrages** ;
- cumul **désaccord = 0** ;
- **aucun redémarrage inexpliqué** ;
- **heap stable** (pas de fuite : le plancher revient, cf. campagne de robustesse) ;
- parité **reproductible après ≥ 3 redémarrages** (config rechargée).

> **Correction du 5 septembre 2026.** Ce critère exigeait d'abord « ≥ 7 jours
> d'**uptime continu** ». C'était **impossible par construction** : la
> vérification quotidienne de mise à jour (`UpdateCheckScheduler`) redémarre
> délibérément le module en mode maintenance, chaque jour à l'heure réglée —
> `BootLoopGuard::restartDeliberately`. Le module ne peut donc jamais afficher
> 7 jours d'uptime tant que cette vérification est active.
>
> On ne desactive pas cette verification pour faire passer le test : ce serait
> mesurer un systeme qui n'existe pas. On mesure celui qui tourne vraiment.
> D'où la reformulation : **7 jours de campagne**, et **aucun redémarrage
> *inexpliqué*** — le redémarrage quotidien de maintenance étant attendu et
> légitime. Corollaire : le compteur de parité repartant à zéro à chaque
> redémarrage, les cycles se **cumulent hors du module** (`tools/soak/`).

Un seul désaccord bloquant, un seul crash imputable → le compteur repart après
correction.

### Gate 2 — Autorité sur UNE zone, au banc, matériel réel
V4 devient autoritaire pour **une** zone (profil V4, sans repli).

**Le mécanisme existe déjà**, découvert le 4 septembre 2026 : le backend V4
porte un `_migratedZoneMask`, et `V4PilotRuntime` y pose `1 << 0` — seule la
zone 1 passe par le modèle de ports V4, toutes les autres restent sur le
chemin legacy. La migration est donc **progressive par zone**, sans rien à
inventer. Concrètement :

- **Gate 2** = flasher le profil `ProgrammeArrosage_s3_v4` (compile déjà) ;
  la zone 1 bascule, le reste ne bouge pas.
- **Gate 3** = élargir le masque aux autres zones.
- **Rollback** = reflash du profil `ProgrammeArrosage_s3`.

Conditions :

> **Confirmation physique obtenue le 5 septembre 2026.** L'utilisateur, au
> module : « les relais claquent bien dans le bon ordre et les diodes
> respirent bien aussi ». Cela valide le point le plus delicat du montage :
> V4 pilote la zone 1 et le moteur historique les zones 2 a 8 **sur la meme
> puce XL9535**, chacun reecrivant un registre 16 bits complet. Un etat
> partage mal tenu (`Xl9535SharedOutputState`) aurait fait claquer les relais
> dans le desordre. La cohabitation des deux moteurs sur le meme composant
> est donc verifiee — ce qu'aucun log ne pouvait etablir.


- **toi** confirmes que la vanne s'ouvre et se ferme réellement (je ne peux pas
  certifier le mouvement physique depuis un banc sans relais) ;
- parité maintenue sur les autres zones ;
- **≥ 24 h**, désaccord = 0, aucun incident.

### Gate 3 — Autorité complète, au banc

> **Multi-cartes confirme le 6 septembre 2026, sur materiel reel.**
> L'utilisateur a raccorde une seconde carte relais : les deux puces
> repondent (0x20 et 0x21), la topologie route les zones 1-2 vers la premiere
> et 3-4 vers la seconde -- sur les MEMES numeros de canal, 0 et 1. Verdict de
> l'utilisateur : « nickel sur cette partie ».
>
> C'est exactement le cas qui, le matin meme, aurait envoye les commandes a la
> mauvaise adresse en silence : le pilote etait alors indexe par TYPE de
> controleur, et command() ne recoit pas le controleur -- configurer la
> seconde carte aurait redirige les commandes de la premiere. Le passage a un
> pilote par INSTANCE est donc valide par le materiel, pas seulement par la
> compilation.
Toutes les zones en V4, cycles complets :

- **≥ 3 jours**, désaccord = 0, comportement identique au legacy confirmé.

### Gate 4 — Promotion en production
- Legacy conservé en **rollback compile-time** (reflash immédiat) ;
- **période de surveillance ≥ 2 semaines** ;
- l'**observateur de parité reste actif en production** (voir ci-dessous).

## Le filet permanent : parité observée, même après promotion

L'abandon demandé, c'est le **repli runtime** (l'arbitrage complexe qui choisit
V4-ou-legacy à chaque décision). Ce n'est **pas** l'observation. On garde donc,
même une fois V4 autoritaire, un **observateur passif** : à chaque décision,
V4 calcule et journalise ce que le legacy **aurait** décidé, et **alerte** (log +
ntfy) sur toute divergence. C'est bon marché, ça n'arbitre rien, et ça reste un
garde-fou permanent — cohérent avec « la fiabilité d'abord ».

## Parité de *capacité* — une limite connue

La parité de comportement ne suffit pas : V4 doit aussi savoir piloter **tout
le matériel que la configuration autorise**, sinon le remplacer serait une
régression.

**Constat du 5 septembre 2026.** `RelayTopology` autorise deux contrôleurs de
relais, XL9535 et **MCP23017**, et le moteur historique (`RelaisManager`) sait
piloter les deux. Le domaine V4, lui, ne dispose que des pilotes `Gpio`,
`Simulated` et `Xl9535` — et son amorçage n'active que XL9535. **Une carte
relais MCP23017 serait donc impilotable sous le profil V4.**

Conséquences retenues :

- l'éditeur de câblage **avertit** désormais quand une carte MCP23017 est
  déclarée, plutôt que de laisser découvrir la panne après un flash ;
- le pilote MCP23017 reste **à écrire** : il serait structurellement identique
  au pilote XL9535 (mêmes opérations 16 bits ; registres `IODIR=0x00`,
  `GPIO=0x12`, `OLAT=0x14`). **Il n'a pas été écrit** faute de carte MCP23017
  sur le banc : livrer un pilote matériel non testé contredirait ce document ;
- l'installation actuelle utilisant une XL9535, cette limite **ne bloque pas**
  la promotion ici. Elle bloquerait une généralisation.

---

## Ce que je peux certifier, et ce qui reste à toi

- **Certifiable par moi, au série** : la **parité de décision** (V4 déciderait /
  exécuterait comme le legacy), la stabilité (pas de crash, heap sain), la
  couverture des cas.
- **À ta charge** : la confirmation du **mouvement physique réel** des vannes
  aux Gate 2+ (le banc S3 n'a pas de relais raccordé).

## En une phrase

V4 ne pilotera un vrai jardin que lorsque le série aura montré, sur la durée et
sur **tous** les cas, **zéro divergence** avec un legacy qui reste le rollback —
et l'observateur de parité restera le filet, même après.
