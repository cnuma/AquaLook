# 13. En cas de problème

Premier réflexe, toujours le même : **ouvrir la page Santé**.

- sur l'écran du module : touchez le bandeau du haut (il change de couleur
  quand quelque chose ne va pas), ou ADMIN > « Sante » ;
- dans les pages Web : « Paramètres » > « Santé du module ».

L'« Etat general » y vaut « Normal », « Attention » (une alarme ou un défaut
attend votre regard) ou dégradé (voir plus bas), et la page liste les
« Defauts actifs ».

## Les alarmes

Quand le module détecte un défaut, la LED d'état **clignote en rouge** et
les pages Web affichent le bandeau **« Alarme »** (triangle rouge).

1. Touchez le bandeau « Alarme » : le « Journal du module » s'ouvre.
2. Lisez les dernières lignes en rouge ou orange : elles disent ce qui
   s'est passé.
3. Une fois la cause comprise ou réglée, touchez **« Acquitter les
   erreurs »** (et « Acquitter l'incident » pour un incident de carte SD).

Acquitter n'efface pas le problème : si le défaut est toujours là, l'alarme
revient.

## Symptômes et conduite à tenir

| Ce que vous voyez | Cause probable | Que faire |
|---|---|---|
| Aucun arrosage programmé ne part, l'arrosage à la main marche | le module n'a pas l'heure (pas d'Internet depuis le démarrage) | vérifier le Wi-Fi et la box ; la pastille d'heure des pages affiche « NTP... » tant que l'heure manque |
| Une zone n'arrose pas, les autres oui | seuil de pluie atteint, jour non prévu, ou pas de vanne raccordée | lire la dernière décision sur la tuile ou l'écran de la zone (chapitres 3 et 4) ; « N/C » = raccordement à faire (chapitre 7) |
| Une zone n'arrose jamais, même par temps sec | seuil de pluie à 0 mm | mettre un seuil d'au moins 1 mm (chapitre 4) |
| LED d'état orange qui clignote vite | le module cherche le Wi-Fi | patienter ; si cela dure, vérifier la box. Pour changer de réseau : ADMIN > « WiFi » > « Lancer portail captif » |
| Les pages ne s'ouvrent pas avec `http://….local` | certains réseaux relaient mal les noms locaux | utiliser l'adresse IP affichée dans ADMIN > « WiFi » |
| Les pages ne s'ouvrent pas du tout | module éteint, hors Wi-Fi, ou téléphone sur un autre réseau (4G, Wi-Fi invité) | vérifier que le téléphone est sur le même Wi-Fi que le module |
| « Stockage SD : … » en haut des pages, alarme carte SD | carte SD absente, mal enfoncée ou muette | voir « Carte SD » ci-dessous |
| Pastille nuage **rouge** | la synchro avec l'espace en ligne échoue depuis plusieurs essais | ouvrir « Synchro cloud » pour le détail ; l'arrosage n'en dépend pas |
| « Mode dégradé actif » | redémarrages répétés : le module s'est mis en sécurité | voir « Mode dégradé » ci-dessous |
| Écran noir | veille de l'écran | toucher l'écran ; s'il reste noir, vérifier l'alimentation |
| Image de l'écran à l'envers ou couleurs inversées | réglage de la dalle | ADMIN > « Systeme » > « Regler l'ecran » |

## Carte SD

La carte SD ne contient que les pages Web : **l'arrosage continue sans
elle**. Sans carte, le module sert des pages de secours réduites.

- **Carte retirée ou mal enfoncée** : la remettre en place ; le module la
  retrouve seul en quelques secondes.
- **Carte qui ne répond plus après une mise à jour des pages** :
  **débrancher l'alimentation 10 secondes**, puis rebrancher. Certaines
  cartes ne se réveillent qu'après une vraie coupure. Si cela se répète,
  changer de carte (32 Go SDHC conseillée, voir le guide d'installation).
- **Carte neuve ou vide** : la page de secours propose « Installer les pages
  Web » (chapitre 11).

## Mode dégradé

Si le module redémarre plusieurs fois de suite sans réussir à fonctionner
normalement, il **se met en sécurité** : certaines fonctions sont
suspendues, et la page d'accueil affiche « Mode dégradé actif ».

1. Ouvrez le « Journal du module » ou la page Santé pour comprendre la
   cause (coupure de courant répétée, carte SD, mise à jour…).
2. Touchez « Réactiver les fonctions suspendues » (pages Web) ou « Lever le
   mode degrade » (écran, page Santé) : la météo, la synchro et la
   vérification des mises à jour reprennent aussitôt. Si une fonction ne
   revient pas, redémarrez le module.

Le module réessaie aussi seul après une période stable, et envoie alors
« AquaLook - retour a la normale » si les notifications sont réglées.

## Avant de demander de l'aide

Notez, sans jamais donner de mot de passe :

- la version (ADMIN > « A propos », ou page Santé > « A propos ») ;
- ce que montrent la page Santé et les dernières lignes du Journal ;
- ce que vous avez fait juste avant le problème.
