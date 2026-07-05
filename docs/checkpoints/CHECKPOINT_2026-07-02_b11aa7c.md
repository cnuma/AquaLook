# Checkpoint AquaLook — 2 juillet 2026

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `fix/encodage-web`
- Commit validé : `b11aa7ca2effa73215637529a88d90bcaa2e51b6`
- Commit parent / moteur d’ancrage : `5a656541428b0f201a5e0b623cacfd55fa6ce7dd`
- Message du commit : `Finalise le mode intervalle et corrige l affichage LCD 2 zones`
- État utilisateur : fonctionnement Web et LCD validé matériellement le 2 juillet 2026.

Ce checkpoint devient la base de reprise autorisée pour la suite du mode intervalle et de l’affichage LCD 2 zones.

## Objectifs couverts

1. Date de départ du cycle intervalle visible et modifiable.
2. Clic sur un jour du planning Web proposant explicitement un nouvel ancrage.
3. Aucune modification silencieuse du cycle.
4. Suppression complète de la programmation intervalle.
5. Une occurrence annulée par la pluie est sautée sans décaler la séquence.
6. Cohérence entre le moteur, le Web et le LCD.
7. Correction des zébrures de la carte de zone intervalle dans la partie haute du Web.
8. Correction du rendu actif et du retour inactif des boutons LCD en mode 2 zones.
9. Optimisation de `data/app.js` afin de conserver une image LittleFS constructible.

## Fichiers modifiés dans le commit

- `data/app.js`
- `data/style.css`
- `src/ConfigManager.cpp`
- `src/ConfigManager.h`
- `src/DisplayManager.cpp`
- `src/DisplayPlanningDecor.cpp`
- `src/ScheduleManager.cpp`
- `src/ScheduleManager.h`
- `src/WebManager.cpp`
- `src/WebManager.h`

## Comportement du mode intervalle

### Ancre durable

Chaque zone en mode intervalle possède un `intervalAnchorDay`, exprimé en jour epoch. Le moteur détermine une occurrence par la règle :

```text
jour >= ancre ET (jour - ancre) modulo intervalle == 0
```

L’exécution effective d’un arrosage ne modifie jamais l’ancre. Une annulation météo n’a donc aucun effet sur les occurrences suivantes.

### Modification explicite

L’ancre n’est modifiée que par une action utilisateur explicite. Une simple consultation du statut ou du planning ne crée pas et ne décale pas le cycle.

### Suppression complète

La suppression d’une programmation intervalle :

- remet la zone en mode jours fixes ;
- efface l’ancre ;
- remet l’intervalle à sa valeur par défaut ;
- désactive et réinitialise les créneaux intervalle ;
- persiste le résultat ;
- demande le rafraîchissement LCD.

### Compatibilité Web

Le frontend utilise `intervalAnchorDay` comme source fonctionnelle. Le champ historique `lastWateredDay`, s’il reste exposé temporairement par l’API, ne doit plus être utilisé pour calculer le cycle.

## Affichage Web validé

- La date de début est lisible dans la configuration intervalle.
- Un clic sur une case propose le jour exact comme nouvel ancrage.
- Les jours appartenant au cycle et les jours hors cycle sont clairement distingués.
- La pluie marque l’occurrence concernée sans déplacer les occurrences suivantes.
- Les zébrures de la carte supérieure sont limitées à la zone d’identification prévue et ne recouvrent plus toute la carte.
- `data/app.js` a été allégé pour respecter la très faible marge LittleFS.

## Affichage LCD 2 zones validé

Le rendu final corrige plusieurs interactions historiques entre `DisplayManager` et `DisplayPlanningDecor` :

- suppression du remplissage rouge parasite sous une carte active ;
- suppression du second dessin de « Appuyer pour arrêter » ;
- suppression de la bande de nettoyage qui coupait « Appuyer pour arroser » au retour inactif ;
- texte d’action légèrement descendu pour améliorer l’aération ;
- le sprite du bouton reste propriétaire du fond et du texte de la carte.

## Invariants à préserver

1. `ConfigManager` reste le propriétaire de la persistance.
2. Le moteur ne pilote jamais directement les relais.
3. Le planning automatique exige une heure NTP synchronisée.
4. La pluie ne modifie jamais l’ancre d’un cycle intervalle.
5. Aucun affichage ou endpoint de lecture ne doit modifier la configuration.
6. Les routes Web et les IDs HTML existants restent des contrats.
7. Toute modification de `data/` exige un `buildfs` réussi avant livraison.
8. En mode LCD 2 zones, éviter tout second rendu périodique sur la zone de texte ou de fond déjà dessinée par le sprite du bouton.
9. Ne pas réintroduire de calcul basé sur le dernier arrosage effectif.
10. Limite fonctionnelle : 1 à 8 zones actives.

## Validations effectuées

- `git diff --cached --check` : réussi.
- Commit Git : réussi.
- Push GitHub sur `fix/encodage-web` : réussi.
- Construction et envoi LittleFS : validés après optimisation de `app.js`.
- Firmware : compilé et envoyé par l’utilisateur pendant les validations LCD.
- Web mobile : validé visuellement et fonctionnellement.
- LCD 2 zones, zone active : validé matériellement.
- LCD 2 zones, retour à l’état inactif : validé matériellement.
- Séquence intervalle et interface : validées par l’utilisateur.

## Commandes de reprise

```powershell
git checkout fix/encodage-web
git pull origin fix/encodage-web
git rev-parse HEAD
```

Le SHA attendu est :

```text
b11aa7ca2effa73215637529a88d90bcaa2e51b6
```

Compilation et contrôle :

```powershell
git status
git diff --check
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t buildfs
```

Envoi :

```powershell
pio run -e ProgrammeArrosage -t upload
pio run -e ProgrammeArrosage -t uploadfs
```

## Risques et points de vigilance

- LittleFS est extrêmement proche de sa limite : mesurer systématiquement la taille de `data/` et lancer `buildfs` après toute modification Web.
- Les fins de ligne sont susceptibles d’être converties LF/CRLF sous Windows ; éviter qu’une conversion augmente inutilement la taille des ressources embarquées.
- `DisplayPlanningDecor` ne doit pas repeindre une zone déjà possédée par le sprite du bouton.
- Toute évolution de l’ancre persistante doit conserver la compatibilité des données existantes et être testée après redémarrage.

## État final

Checkpoint validé comme base stable de la branche `fix/encodage-web` au commit `b11aa7ca2effa73215637529a88d90bcaa2e51b6`.
