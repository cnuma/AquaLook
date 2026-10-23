# 11. Mises à jour

AquaLook a deux choses à mettre à jour, indépendantes l'une de l'autre :

- **le programme du module** (le « firmware ») ;
- **les pages Web**, rangées sur la carte SD.

**Règle d'or : une mise à jour n'est jamais installée sans vous.** Le module
peut vérifier tout seul qu'une version existe, mais c'est toujours vous qui
lancez l'installation. Chaque version est **signée** : le module refuse un
programme ou des pages qui ne viennent pas du projet.

## Savoir qu'une mise à jour attend

Le **violet** signale toujours une mise à jour :

- sur l'écran du module : « MAJ DISPO » dans le bandeau ;
- sur le ruban : la LED d'état clignote lentement en violet ;
- dans les pages Web : la pastille « ⬆ MAJ » dans le bandeau ;
- dans l'espace en ligne : la pastille « ⬆ MAJ » et la carte du module en
  violet ;
- et, si elles sont réglées, une notification (chapitre 9).

## La vérification automatique

« Paramètres » > « Options avancées du module » > « Mises à jour » :

- « Vérifier automatiquement les mises à jour » ;
- « Heure de la vérification » et « Tous les (jours) ».

À l'heure choisie, le module **redémarre une trentaine de secondes** en
mode maintenance pour interroger le serveur, puis reprend. Choisissez une
heure sans arrosage (la nuit). Rien n'est installé à cette étape.

## Installer : la page « Mises a jour »

Touchez la pastille « ⬆ MAJ », ou « Paramètres » > « Mises à jour » >
« Ouvrir la gestion des mises à jour ».

### Le programme du module : trois étapes, dans l'ordre

| Étape | Bouton | Ce qui se passe |
|---|---|---|
| 1 | « Verifier maintenant » | le module redémarre environ 30 secondes, interroge les sources, puis revient. Rien n'est installé |
| 2 | « Preparer » | le module télécharge le programme, vérifie son empreinte et l'écrit dans une partie de réserve. **Il continue de tourner sur l'ancien** |
| 3 | « Installer » | le module bascule sur le nouveau programme et redémarre dessus |

Chaque étape ne s'active que lorsque la précédente a réussi. **Aucune n'est
possible pendant un arrosage.**

**Sécurité :** si le nouveau programme ne démarre pas correctement plusieurs
fois de suite, le module **revient tout seul à l'ancien**.

### Les pages Web

« Mettre a jour les pages Web » : le module redémarre en mode maintenance,
télécharge chaque fichier, vérifie son empreinte, puis bascule. Si quoi que
ce soit échoue, **les pages actuelles sont conservées**. Le programme du
module n'est pas touché.

## Pendant la mise à jour

- L'écran du module devient **violet**, avec les étapes et une barre de
  progression ; toutes les LED du ruban sont violettes.
- Le module ne pilote **aucune vanne** pendant ce temps.
- **Ne débranchez pas le module.** Il redémarre seul à la fin.
- Comptez environ une minute pour les pages Web, un peu plus pour le
  programme.

Les détails de la dernière opération restent consultables sous « Details »,
en bas de la page.

## Carte SD neuve ou vide

Sans pages sur la carte SD, l'adresse du module ouvre une page de mise à
jour de secours : « Installer les pages Web » les télécharge et les installe
(voir le guide [Monter un module AquaLook neuf](../MONTER_UN_MODULE_NEUF.md)).

Si la page annonce que les pages sont installées mais que la carte SD ne
répond plus : **débranchez l'alimentation 10 secondes**, puis rebranchez.
Certaines cartes ne se réveillent qu'après une vraie coupure (chapitre 13).
