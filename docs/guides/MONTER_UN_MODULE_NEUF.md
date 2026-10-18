# Monter un module AquaLook neuf

Guide pas à pas, de la carte nue au module rattaché à un compte en ligne.
Rédigé le 18 octobre 2026 pour la version 5.19.0 (P2.18, lot 5). Les
libellés cités entre guillemets sont ceux de l'écran et des pages, relevés
dans le code de cette version.

Durée : environ 30 minutes, dont 5 de flash. Aucune compétence en
programmation n'est nécessaire ; il faut un PC Windows avec Python et
PlatformIO (ou `esptool`) déjà installés, comme pour le développement.

---

## 1. Matériel

| Élément | Référence ou conseil |
|---|---|
| Carte | Guition **JC4827W543C_I** (ESP32-S3, écran tactile 480×272) |
| Antenne Wi-Fi | **obligatoire**, clipsée sur le connecteur U.FL de la carte |
| Carte SD | voir §2 |
| Alimentation | 5 V externe stable, plutôt que le port USB d'un PC |
| Carte relais | XL9535, sur le second bus I²C (SCL 17, SDA 18), adresse `0x20` |
| Câble USB | câble **de données** (un câble de charge seule ne permet pas le flash) |

Pourquoi l'antenne et l'alimentation d'abord : sans antenne, le signal tombe
vers −90 dBm et le module annonce un faux « mot de passe refusé » (constaté
le 17 oct. 2026) ; sur le port USB d'un PC, les pointes de courant du Wi-Fi
ou de l'ouverture d'une vanne provoquent des redémarrages (`resetReason` =
`brownout` dans `/api/diagnostics`) qui ressemblent à des pannes logicielles.

Le module fonctionne sans carte relais (il arrose alors « dans le vide ») :
pratique pour une première mise en route sur table.

## 2. Carte SD

La carte contient les pages de l'interface Web. Le module **arrose sans
elle** ; sans elle, seules les pages de secours intégrées sont disponibles.

| Conseil | Raison |
|---|---|
| **SDHC 32 Go**, formatée en FAT32 | validée : se remonte seule après l'installation des pages (17 oct. 2026) |
| FAT32 ou exFAT | les deux formats sont lus par le module |
| **À éviter : Lenovo 64 Go SDXC** | se bloque après le redémarrage qui suit l'installation des pages ; seule une coupure d'alimentation la rend (P3.34, 4 essais sur 4) |
| Autre carte de plus de 32 Go | à essayer une fois avant d'en équiper un module (voir §9) |

Une carte neuve et vide convient : les pages s'installent depuis le module
(§6). Il n'est pas nécessaire de les copier depuis un PC.

## 3. Flasher le programme

Le PC télécharge la dernière release publiée, vérifie sa signature et chaque
image, puis écrit tout le module (chargeur, partitions, programme de
production, système de fichiers de secours).

1. Brancher le module en USB, **fermer tout moniteur série** (il tient le port).
2. Trouver son port : `pio device list` (par exemple `COM5`).
3. Depuis la racine du dépôt :

   ```powershell
   python tools/preparer_module.py flasher --port COM5 --effacer
   ```

   `--effacer` remet le module à neuf (tout réglage précédent est perdu).
   Pour reflasher un module **déjà réglé** sans perdre son Wi-Fi, son PIN ni
   son rattachement : `--sauver-nvs avant.bin` à la place de `--effacer`.

4. Attendu en fin de commande : la bannière du module relue sur la série,
   avec la version de la release et l'environnement `ProgrammeArrosage_s3`.

En cas d'échec « esptool a échoué » : port occupé par un moniteur, ou câble
de charge seule.

## 4. Premier démarrage, à l'écran

1. **Réglage de l'écran** (« Reglage de l'ecran 1/3 » à « 3/3 ») : répondre
   à ce que montre la dalle (image à l'endroit ou retournée, couleurs normales
   ou inversées). Si l'image paraît fausse après coup : ADMIN > Système >
   « Regler l'ecran ».
2. **Code PIN** (« Code PIN de l'ecran ») : proposé une seule fois.
   « Choisir un PIN » (4 à 6 chiffres, saisis deux fois) ou « Plus tard ».
   Le PIN protège l'administration sur l'écran. Oubli : maintenir le doigt
   sur l'écran 10 s au démarrage.

Accès à l'administration de l'écran, pour la suite : toucher le **coin
supérieur gauche** de l'écran d'accueil (PIN demandé s'il est posé).

## 5. Wi-Fi

Sans réseau enregistré, l'écran affiche « Wi-Fi a regler : reseau
Arrosage-Setup » et « puis http://192.168.4.1 ».

1. Avec un téléphone, se connecter au réseau **Arrosage-Setup**.
2. Ouvrir `http://192.168.4.1` (la page s'ouvre souvent seule).
3. Choisir le réseau de la maison dans la liste, saisir son mot de passe,
   enregistrer. Le module redémarre et rejoint le réseau ; en cas d'échec, la
   page en donne la cause.
4. L'adresse du module s'affiche ensuite à l'écran (ADMIN > Système). Son nom
   local, par exemple `http://aqualook-64cc.local`, marche aussi depuis la
   plupart des appareils.

## 6. Mot de passe Web et pages

À la première visite de l'adresse du module, la page **« Sécuriser votre
module »** (`/securiser`) s'ouvre : le mot de passe Web est obligatoire.

1. « 1. Autoriser sur l'écran du module » : sur l'écran, ADMIN > Système >
   « Autoriser un mot de passe Web » (l'écran affiche alors « Pose autorisee
   (10 min) »). Ce geste prouve qu'on est devant le module.
2. « 2. Choisir le mot de passe » : 12 à 64 caractères, deux fois, puis
   « Enregistrer le mot de passe ». Le noter : il ouvre l'interface Web.
3. **Carte SD vide** : la page de mise à jour de secours indique « aucune
   (carte SD neuve ou vidée) ». Cliquer **« Installer les pages Web »** : le
   module télécharge les pages, vérifie leur signature et redémarre seul
   (environ une minute). L'interface complète apparaît ensuite.

Si la page annonce « installées, carte SD muette » : débrancher
l'alimentation 10 secondes, rebrancher, revenir sur la page (carte du type
de celle du §2 à éviter ; en prévoir une autre).

## 7. Raccordement des vannes

**Sans raccordement enregistré, le module ne pilote aucune sortie** : il ne
devine pas comment il est câblé.

Interface Web > « Options avancées du module » > rubrique « Raccordement des
sorties » (nommée « Câblage entrées / sorties » avant P3.35) : pour chaque
zone, indiquer la carte et le canal du relais qui commande sa vanne, puis
« Enregistrer ». La modification prend effet au redémarrage suivant.

Premier essai, **toujours une seule zone, sur une durée courte, en
regardant la vanne** : démarrage manuel depuis l'accueil, vérifier que c'est
la bonne vanne qui s'ouvre, puis l'arrêter.

## 8. Rattacher le module à un compte en ligne

Facultatif : le module fonctionne entièrement sans service en ligne. Le
rattachement permet de le suivre et de le régler depuis l'espace en ligne.
Aucun réglage de serveur n'est nécessaire : le module connaît le serveur
AquaLook par défaut.

1. Sur l'écran : ADMIN > En ligne > « Rattacher a un compte ». L'écran
   affiche « Demande du code au serveur... » puis « Code a saisir dans
   l'espace en ligne : » avec un code de 8 caractères, valable 10 minutes.
   **Ne jamais transmettre ce code** à quelqu'un d'autre : il donne le module.
2. Dans l'espace en ligne (adresse affichée sous le code, `.../app`) :
   « Mon compte » > « Rattacher un module », saisir le code.
3. L'écran affiche « Rattachement reussi. » ; le module envoie aussitôt sa
   configuration et apparaît dans l'espace en ligne.
4. Le nom donné au module dans l'espace en ligne devient aussi son nom local
   (`<nom>.local`), à la synchronisation suivante.

Messages possibles : « Deja rattache : detacher en ligne » (le module
appartient encore à un autre compte : le détacher depuis cet espace),
« N codes/h atteints : reessayer a HH:MM », « Code expire ».

> Lot 4 de P2.18 : le code de ce parcours existe ; l'essai de bout en bout
> sur un module neuf restait à faire au 18 oct. 2026.

## 9. Liste de contrôle finale

| ✓ | Vérification | Où |
|---|---|---|
| ☐ | Antenne en place, signal meilleur que −75 dBm | `/api/diagnostics`, `wifi.rssi` |
| ☐ | Version attendue, environnement `ProgrammeArrosage_s3` | ADMIN > A propos, ou `/api/diagnostics`, `build` |
| ☐ | Écran à l'endroit, couleurs normales, tactile juste | à l'œil |
| ☐ | PIN posé (ou choix « Plus tard » assumé) | ADMIN > Securite |
| ☐ | Mot de passe Web posé : l'adresse mène à la page de connexion | navigateur |
| ☐ | Pages installées, santé « normale » | page Santé, `/api/health` |
| ☐ | Carte SD remontée après **une coupure** d'alimentation | page Santé après rebranchement |
| ☐ | Raccordement enregistré, une zone essayée sur une durée courte | §7 |
| ☐ | Rattaché (si voulu), synchronisation réussie | ADMIN > En ligne, espace en ligne |
| ☐ | Après installation dans son boîtier : alimentation 5 V, aucun redémarrage inattendu sur une journée | `resetReason` |

La remontée de la carte après une coupure (avant-dernière ligne) suffit à
écarter une carte du type de celle à éviter.

## 10. Plus tard : verrouillage matériel

Avant toute mise en service chez un tiers, le verrouillage matériel (Secure
Boot, chiffrement de la flash, USB sécurisé) sera obligatoire (P1.6, D022).
Il est irréversible et se fera module par module ; il modifiera l'étape 3 de
ce guide. En attendant : module sur un Wi-Fi invité ou dédié, prise USB
inaccessible sans outil.
