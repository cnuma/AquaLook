# 3. Zones et planning

Tout se règle dans les pages Web du module (chapitre 1 pour les ouvrir).

## Combien de zones

« Paramètres » (bouton à trois traits, en haut à droite) > « Zones » :

| Réglage | Sens | Valeurs |
|---|---|---|
| « Nombre de zones » | zones pilotées par le module | 1 à 8 |
| « Duree max arrosage (min) » | sécurité : aucune vanne ne reste ouverte plus longtemps, quelle que soit l'origine de l'arrosage | 1 à 120, 60 par défaut |
| « Duree manuelle (min) » | durée d'un arrosage lancé à la main | 1 à 120, 10 par défaut |

Puis « Enregistrer la configuration ». **Changer le nombre de zones
redémarre le module** (une confirmation est demandée).

Une zone n'arrose que si une vanne lui est raccordée (chapitre 7). Sinon,
elle est marquée « N/C » et « Aucune sortie affectee ».

## Nom, couleur et mode d'une zone

Toujours dans « Zones », touchez une ligne de « Réglages par zone ». La
fenêtre « Config -- <nom> » s'ouvre :

| Champ | Sens |
|---|---|
| « Nom » | nom affiché partout (20 caractères au plus) |
| « Couleur » | couleur de la zone sur l'écran, le planning et le ruban |
| « Mode planification » | « Jours fixes » ou « Intervalle » (voir ci-dessous) |
| « Intervalle (jours) » | en mode intervalle : arroser tous les N jours (1 à 30) |
| « Date de debut du cycle » | en mode intervalle : premier jour du cycle |
| « Seuil pluie (mm) » | voir chapitre 4 |
| « Fenetre (heures) » | voir chapitre 4 |
| « Notifications ntfy » | « Notifier au démarrage », « Notifier à l'arrêt » (chapitre 9) |

Puis « Enregistrer ».

`[capture Web : réglages d'une zone]`

## Les deux modes de programmation

**Jours fixes** : vous choisissez les créneaux **de chaque jour de la
semaine**. Les créneaux du lundi valent pour tous les lundis.

**Intervalle** : la zone arrose **tous les N jours** à partir de la date de
début du cycle, quel que soit le jour de la semaine. Les mêmes créneaux
servent pour chaque jour arrosé. Pratique pour une pelouse (« tous les
3 jours ») ou des plantes en pot.

## Le planning de la semaine

Sur l'accueil des pages Web, « Planning semaine » montre les 7 prochains
jours, aujourd'hui en premier (marqué `<<`), une ligne par zone, avec la
météo de chaque jour au-dessus.

`[capture Web : planning de la semaine]`

Comment le lire :

| Aspect d'une case | Sens |
|---|---|
| barres colorées | créneaux prévus ce jour-là |
| **hachures** | arrosage prévu mais annulé par la pluie annoncée |
| **grand chevron gris** avec « prochain … » | mode intervalle : pas d'arrosage ce jour-là ; le chevron indique le prochain jour arrosé |
| ligne tramée « N/C » | zone sans vanne raccordée |

Survolez (ordinateur) ou touchez (téléphone) la météo d'un jour pour voir le
détail des prévisions.

## Programmer des créneaux

### En mode jours fixes

1. Dans le planning, touchez la case du **jour** et de la **zone** voulus.
2. La fenêtre « <zone> -- <jour> » affiche les **5 créneaux** possibles :
   - la première colonne (« Actif ») active ou désactive le créneau ;
   - « h » et « min » : heure de départ (0-23, 0-59) ;
   - « Duree » : durée en minutes (1 à 120).
3. « Enregistrer ».

Pour arrêter d'arroser un jour, désactivez ses créneaux.

### En mode intervalle

Touchez une case de la zone :

- si c'est **un autre jour que le début actuel du cycle**, le module propose
  d'en faire la **nouvelle date de départ** (« Definir … comme nouvelle date
  de depart ? ») ; le cycle est recalculé à partir de ce jour. Pratique pour
  faire partir un arrosage un jour précis ;
- sinon, la fenêtre « <zone> -- Intervalle » s'ouvre : date de début
  (« Enregistrer cette date »), les 5 créneaux (comme ci-dessus), et
  « Supprimer la programmation intervalle », qui efface horaires, date de
  départ et cycle.

## Ce qui empêche un arrosage programmé de partir

| Cause | Ce que vous voyez |
|---|---|
| le module n'a pas l'heure (Internet absent depuis le démarrage) | aucun arrosage programmé ; l'arrosage manuel reste possible |
| pluie annoncée au-dessus du seuil de la zone | « Reporté (pluie) », hachures, LED de zone orange |
| jour non prévu (mode et créneaux) | « Jour non prevu » |
| aucune vanne raccordée à la zone | « N/C », « Aucune sortie affectee » |
| mise à jour en cours | écran et ruban violets ; le module ne pilote aucune vanne pendant la mise à jour |

La tuile de chaque zone (accueil des pages Web) et l'écran d'une zone sur
le module affichent la dernière décision.
