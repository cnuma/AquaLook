# Manuel utilisateur AquaLook

Manuel destiné à la personne qui utilise AquaLook pour arroser son jardin,
sans connaissance du projet ni de la programmation. Il décrit la version
**5.20.0** ; les libellés entre guillemets sont ceux de l'écran et des pages,
relevés dans le code de cette version (les écrans du module n'affichent pas
d'accents : « Systeme », « Arreter »).

L'installation d'un module neuf (matériel, carte SD, programme, Wi-Fi,
mot de passe) fait l'objet d'un guide séparé :
[Monter un module AquaLook neuf](../MONTER_UN_MODULE_NEUF.md).

## Sommaire

| # | Chapitre | État |
|---|---|---|
| 0 | [Présentation](00_PRESENTATION.md) | rédigé |
| 1 | [Premiers pas](01_PREMIERS_PAS.md) | rédigé |
| 2 | [L'écran tactile](02_ECRAN_TACTILE.md) | rédigé |
| 3 | [Zones et planning](03_ZONES_ET_PLANNING.md) | rédigé |
| 4 | [Pluie et météo](04_PLUIE_ET_METEO.md) | rédigé |
| 5 | [Arroser à la main](05_ARROSER_A_LA_MAIN.md) | rédigé |
| 6 | [Les pages Web du module](06_PAGES_WEB.md) | rédigé |
| 7 | Cartes interfaces et raccordement | à rédiger |
| 8 | Scripts | à rédiger |
| 9 | Notifications | à rédiger |
| 10 | Espace en ligne | à rédiger |
| 11 | Mises à jour | à rédiger |
| 12 | Sécurité | à rédiger |
| 13 | En cas de problème | à rédiger |
| 14 | Glossaire | à rédiger |

## Conventions

- **« Libellé »** : texte exact affiché par l'écran ou la page.
- **Écran > Page** : chemin à suivre, par exemple ADMIN > Systeme.
- `[capture : …]` : emplacement d'une photo de l'écran du module, à ajouter.
- Images : dossier `images/`. Les captures des pages Web sont de vraies
  pages 5.20.0 avec un jardin d'exemple, produites par
  `python tools/apercu_web/captures_manuel.py` (à relancer quand une page
  change). Les dessins `.svg` sont des illustrations, signalées comme
  telles ; les photos de l'écran du module restent à ajouter.

## Tenue à jour

Toute évolution qui change un libellé, un écran ou un comportement décrit
ici met à jour le chapitre concerné dans le même lot. Le manuel ne décrit
que ce qui est réellement en place : une fonction prévue mais absente n'y
figure pas.
