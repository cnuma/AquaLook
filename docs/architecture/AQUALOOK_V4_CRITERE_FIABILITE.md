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

- **≥ 7 jours** d'uptime continu réel ;
- **≥ 100 cycles** réels (start+stop) toutes zones confondues ;
- cumul **désaccord = 0** ;
- **aucun** reboot / watchdog imputable au chemin shadow ;
- **heap stable** (pas de fuite : le plancher revient, cf. campagne de robustesse) ;
- parité **reproductible après ≥ 3 redémarrages** (config rechargée).

Un seul désaccord bloquant, un seul crash imputable → le compteur repart après
correction.

### Gate 2 — Autorité sur UNE zone, au banc, matériel réel
V4 devient autoritaire pour **une** zone (profil V4, sans repli) :

- **toi** confirmes que la vanne s'ouvre et se ferme réellement (je ne peux pas
  certifier le mouvement physique depuis un banc sans relais) ;
- parité maintenue sur les autres zones ;
- **≥ 24 h**, désaccord = 0, aucun incident.

### Gate 3 — Autorité complète, au banc
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
