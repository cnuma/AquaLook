# 12. Sécurité

AquaLook ouvre des vannes d'eau et vit sur le réseau de la maison. Quelques
règles simples le protègent, et protègent vos autres appareils.

## Les deux protections du module

| Protection | Où | Sert à |
|---|---|---|
| **Mot de passe Web** (12 à 64 caractères) | pages Web du module | toute modification depuis un navigateur |
| **Code PIN** (4 à 6 chiffres, facultatif) | écran du module | menu d'administration et arrosage depuis l'écran |

Le mot de passe Web est **obligatoire** : il est choisi à l'installation
(guide [Monter un module AquaLook neuf](../MONTER_UN_MODULE_NEUF.md)).

Contre les essais au hasard, le module impose une **attente croissante**
après plusieurs erreurs, jusqu'à 15 minutes, pour le mot de passe Web comme
pour le PIN. La session Web se ferme après 30 minutes sans activité.

## Bonnes pratiques

- **Ne communiquez jamais** le mot de passe Web, le PIN, le sujet ntfy ou
  le mot de passe de l'espace en ligne, ni par message, ni par courriel, ni
  à une assistance.
- **Choisissez un mot de passe Web propre au module**, que vous n'utilisez
  nulle part ailleurs.
- **Placez le module sur un Wi-Fi invité** (ou un réseau dédié), séparé de
  vos ordinateurs et téléphones, si votre box le permet. Les pages du module
  circulent **en clair** sur le réseau local (pas de cadenas `https`) : un
  réseau à part limite ce qu'un intrus pourrait voir.
- **Fermez le boîtier** : la prise USB du module ne doit pas être accessible
  sans outil. Quelqu'un qui y branche un ordinateur peut changer son
  programme.
- **Installez les mises à jour** quand le violet le signale (chapitre 11) :
  elles corrigent aussi des failles.

## Ce qui est déjà garanti

- Aucune mise à jour ne s'installe sans votre geste, et le module refuse
  tout programme ou page **non signé** par le projet.
- Personne ne peut déclencher une mise à jour à distance, pas même
  l'espace en ligne.
- Le code d'AquaLook est public : sa sécurité ne repose pas sur un secret
  caché dans le programme, mais sur vos mots de passe.

## Mot de passe ou PIN oublié

**Mot de passe Web oublié** (il faut être devant le module) :

1. Sur l'écran : ADMIN > « Systeme » > « Oublier le mot de passe Web »,
   puis toucher une seconde fois pour confirmer (« Confirmer ? (retaper
   ici) »).
2. Toujours sur l'écran, « Autoriser un mot de passe Web » : la pose d'un
   nouveau mot de passe est permise pendant 10 minutes.
3. Ouvrez l'adresse du module dans le navigateur : la page « Sécuriser
   votre module » permet de choisir le nouveau mot de passe.

**PIN oublié** : coupez l'alimentation, rebranchez, puis maintenez le doigt
sur l'écran pendant 10 secondes au démarrage. Le PIN est effacé ; vous
pouvez en poser un nouveau dans ADMIN > « Securite ».

Ces deux gestes demandent d'être **devant le module** : c'est voulu, ils ne
peuvent pas se faire à distance.
