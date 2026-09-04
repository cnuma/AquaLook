# AquaLook V4 — Reprise de migration : décision de simplification

**Date :** 4 septembre 2026
**Branche :** `refonte/v4-moteur`
**Statut :** décision d'orientation, cadre les runs suivants.

---

## 1. Constat de départ

La migration V4 n'est pas à ouvrir : elle est **mature, en Phase 6** (bascule du
backend physique), menée par phases/runs documentés. État validé :

- le domaine/runtime V4 parle déjà `EquipmentOutput` (commandes + lectures) ;
- les interfaces **Web et LCD lisent déjà l'état via l'adaptateur** avec repli
  `RelaisManager` — elles sont donc **réutilisées telles quelles** ;
- le **chemin matériel réel des électrovannes reste sur le legacy** (`RelaisManager`),
  volontairement, tant que V4 n'est pas prouvé ;
- l'orchestrateur produit un `ZoneExecutionPlan` et un **mode d'autorité** décide
  V4-vs-legacy, avec repli runtime explicite.

On **reprend depuis cette frontière**, on ne recommence pas.

## 2. Décision — simplification demandée par l'utilisateur

Le **repli runtime** (arbitrage V4-vs-legacy à chaque décision) est puissant mais
alourdit la gestion. Décision : **le retirer** au profit d'un **découpage par
profil de compilation**.

- **Profil `LEGACY`** — firmware actuel, prouvé, **gelé**. Référence et rollback.
  Plus aucune fonctionnalité ne s'y ajoute. S'appuie sur le flag existant
  `AQUALOOK_RELAY_BACKEND_LEGACY`.
- **Profil `V4`** — le moteur qui avance, **autoritaire dans son profil**, sans
  co-pilotage legacy. Dans ce profil, V4 décide et pilote, point.

Le **rollback** n'est plus une bascule runtime : c'est **git + reflash du profil
legacy**. C'est l'option « rollback compile-time » déjà prévue par la stratégie
de bascule (`AQUALOOK_V4_PHASE6_BACKEND_SWITCHOVER_STRATEGY.md`).

## 3. Contrepartie assumée

Le shadow/repli servait à valider la parité **en production sans risque**. En le
retirant, la validation **se déplace au banc** : le profil V4 doit faire tourner
des cycles d'arrosage complets sur la carte de développement avant de devenir le
défaut. Le runtime en shadow peut subsister comme **outil de banc**, plus dans le
firmware livré. Ce choix privilégie la simplicité de gestion, conformément à la
demande.

## 4. Méthode de validation (utilisateur absent du module)

L'utilisateur n'est pas au module pour une validation visuelle. Chaque run est
donc validé par la **couche série**, sur-exploitée à cette fin :

- compilation locale des deux profils (aucune régression du legacy) ;
- flash du profil V4 sur le S3, capture série au démarrage et en fonctionnement ;
- vérification par le log : démarrage propre, **décisions du moteur** (plan de
  zone, ouverture/fermeture de sortie), état relais, absence de crash/watchdog ;
- au besoin, journalisation V4 renforcée (traces de décision explicites) pour
  rendre le comportement lisible sans écran ni vannes réelles.

Les **interfaces LCD restent celles validées sur le legacy** : V4 ne touche pas
au rendu écran, qui lit déjà l'état via l'adaptateur et reste donc agnostique au
moteur.

## 5. Chemin vers la parité

1. Retirer l'arbitrage/repli runtime ; rendre le profil V4 **autoritaire**.
2. Piloter **une seule zone au banc** en V4 pur (sans repli), prouvée au série.
3. Étendre à toutes les zones, puis aux comportements : planning, pluie, météo,
   arrosage manuel, mode intervalle, mode dégradé — run par run, validés au série.
4. **Promotion** : quand le profil V4 exécute un cycle complet aussi proprement
   que le legacy sur la vraie carte → il devient le défaut ; le legacy reste le
   rollback compile-time.
5. **Réintégrer `IoExpander`** comme driver V4 (le modèle prévoit « futurs
   backends GPIO, XL9535, MCP23017 ») ; la détection de bobine y reviendra
   proprement, mise de côté sur le legacy en attendant.

## 6. Principe directeur — réutiliser le legacy au maximum

V4 ne réécrit pas ce qui est prouvé. Il **remplace le strict nécessaire** — la
couche de **pilotage matériel** (via le modèle de ports / driver binaire) — et
**reprend le reste tel quel** :

- la **logique de décision** (planning des créneaux, pluie, météo, manuel,
  intervalle) reste celle de `ScheduleManager`, éprouvée par des mois d'usage ;
- le **rendu LCD** et les **pages Web** restent ceux du legacy (ils lisent l'état
  via l'adaptateur, agnostiques au moteur) ;
- même au niveau matériel, on **réutilise le code registre I2C éprouvé** de
  `RelaisManager` plutôt que de le réécrire.

Autrement dit : V4 change *comment la commande atteint le relais*, pas *quand ni
pourquoi une zone s'arrose*. Toute divergence par rapport au legacy doit être
justifiée par un besoin réel, jamais par principe.

## 7. Invariants conservés

Rien de la refonte ne remet en cause les invariants du projet :
« le local gagne », l'utilisateur déclenche, dégradation sûre, intégrité et
migration NVS, un commit par changement validé.
