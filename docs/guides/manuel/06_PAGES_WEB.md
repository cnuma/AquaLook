# 6. Les pages Web du module

Les pages Web servent à tout régler et à surveiller le module. Elles
s'ouvrent depuis un navigateur sur le réseau de la maison (chapitre 1) et
s'adaptent à l'écran d'un téléphone.

## L'accueil

`[capture Web : accueil, ordinateur]`

De haut en bas :

**Le bandeau**

| Élément | Sens |
|---|---|
| nom du module et ville | identité du module |
| pastille Wi-Fi | qualité du signal |
| « ⬆ MAJ » (violet, clignotant) | une mise à jour est disponible ; un toucher ouvre la page « Mises a jour » (chapitre 11) |
| pastille nuage | synchro avec l'espace en ligne : **vert** à jour, **orange** échange en cours ou en attente, **rouge** en panne ; un toucher ouvre « Synchro cloud » |
| bouton à trois traits | ouvre les « Paramètres » |

Sur téléphone, les pastilles n'affichent que leur icône.

**Le bandeau « Alarme »** (triangle rouge) : il n'apparaît que si une
alarme attend. Un toucher ouvre le « Journal du module », où l'on comprend
et acquitte l'alarme (chapitre 13).

**« Zones »** : une tuile par zone, avec son état (« ON », « OFF »,
« N/C »), son mode (« Jours fixes » ou « Intervalle / 3j »), son seuil de
pluie (« ≥2mm / 24h »), la dernière décision (« Planifie », « Reporté
(pluie) »…) et les boutons d'arrosage (chapitre 5). Avec beaucoup de zones,
« Affichage dense » (Paramètres > « Zones ») les présente en tableau.

**« Planning semaine »** : les 7 prochains jours et la météo (chapitre 3).

**« Activité »** : les 20 dernières actions faites depuis cette page
(arrosages lancés, réglages enregistrés) ; « Masquer » la replie. Le
journal complet du module est sur une page à part (« Journal du module »).

En bas de page : la version des pages Web installées.

## Les Paramètres

Le bouton à trois traits ouvre un **menu de rubriques** : on en ouvre une à
la fois, la flèche « ‹ » ramène au menu.

Par défaut, seules les rubriques du quotidien sont visibles :

| Rubrique | Contenu | Chapitre |
|---|---|---|
| « Zones » | nombre de zones, durées, réglages par zone, affichage dense | 3 |
| « Météo » | infos-bulles du planning | 4 |

et trois liens : « Santé du module », « Scripts », « Synchro cloud ».

**« Options avancées du module »** affiche le reste. Le module demande alors
le mot de passe Web si la session est fermée (« Les options avancées
modifient la configuration du module »). « Masquer les options avancées »
les replie.

| Rubrique | Contenu | Chapitre |
|---|---|---|
| « Réseau » | « WiFi » (changer de réseau : « Enregistrer et redémarrer », ou « Lancer portail captif »), « NTP / Heure » (serveur de l'heure, décalage horaire et heure d'été) | 1 |
| « Météo » | source, clé, localisation, unités | 4 |
| « Affichage » | couleurs, formes et rafraîchissement de l'écran du module, météo sur l'écran | 2 |
| « Notifications » | ntfy : serveur, sujet, jeton, « Envoyer un test » | 9 |
| « Serveur » | synchronisation avec l'espace en ligne | 10 |
| « Mises à jour » | vérification automatique (heure, fréquence), « Ouvrir la gestion des mises à jour » | 11 |
| « Système » | veille de l'écran, « LED signe de vie », bandeau d'activité, « Reset configuration (reboot) » | 2, 13 |
| « Cartes interfaces » | raccordement des vannes et des entrées | 7 |

et le lien « Liste des URLs disponibles ».

> **Prudence :** « Reset configuration (reboot) » remet **tous** les
> réglages d'origine (zones, planning, Wi-Fi…) et redémarre le module. À ne
> faire qu'en connaissance de cause.

## Les autres pages

| Page | À quoi elle sert |
|---|---|
| « Sante du module » | bilan général (normal, attention, dégradé), carte SD, mémoire, défauts actifs ; le premier endroit où regarder si quelque chose cloche |
| « Journal du module » | messages récents du module, alarmes à acquitter (chapitre 13) |
| « Synchro cloud » | état des échanges avec l'espace en ligne : bandeau vert, orange ou rouge, dernier échange, intervalle (chapitre 10) |
| « Mises a jour » | programme du module et pages Web : vérifier, préparer, installer (chapitre 11) |
| « Diagnostic système » | mesures détaillées, utiles pour un dépannage avancé |
| « Scripts » | éditeur de scripts (chapitre 8) |
| « URLs disponibles » | liste de toutes les adresses du module, pour un usage avancé |

Ces pages ont, en haut, un bouton de retour vers l'accueil (« ‹ Retour a
AquaLook »).

## Le mode dégradé

Si le module a redémarré plusieurs fois de suite sans réussir à fonctionner
normalement, il se met **en sécurité** : la page affiche « Mode dégradé
actif » et certaines fonctions sont suspendues.

1. Consultez le « Journal du module » ou la « Page Santé » pour comprendre
   la cause.
2. Puis touchez « Réactiver les fonctions suspendues » : elles reprennent au
   prochain démarrage.

Le module réessaie aussi tout seul après une période stable.
