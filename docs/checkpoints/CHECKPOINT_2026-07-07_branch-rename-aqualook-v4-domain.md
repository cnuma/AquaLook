# AquaLook V4 — Renommage de la branche de travail

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Ancienne branche :** `feature/relay-board-mapping`  
**Nouvelle branche :** `feature/aqualook-v4-domain`  
**Commit de départ commun :** `a82881298351246650150aca33fb3c6857b422a4`

## Décision

La branche de travail AquaLook V4 est désormais :

```text
feature/aqualook-v4-domain
```

Le nom précédent ne décrivait plus correctement le périmètre, désormais centré sur :

- le modèle `Equipment` ;
- les états runtime ;
- les intentions ;
- les exécutions ;
- les dépendances ;
- l’architecture mémoire bornée.

## Continuité Git

Aucun commit n’a été réécrit. La nouvelle branche a été créée sur le commit exact :

```text
a82881298351246650150aca33fb3c6857b422a4
```

Les checkpoints antérieurs conservent volontairement leur ancien nom de branche afin de préserver leur contexte historique.

## Règle de reprise

Tous les prochains runs V4 doivent utiliser :

```text
feature/aqualook-v4-domain
```

L’ancienne branche ne doit plus recevoir de nouveaux commits.
