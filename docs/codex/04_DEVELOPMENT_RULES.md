# 04 — Règles de développement

## C++

- Compatible avec la configuration PlatformIO/Arduino actuelle.
- Éviter les allocations dynamiques dans les chemins fréquents.
- Préférer buffers bornés, `strlcpy`, `snprintf`.
- Ne pas introduire `String` dans les modules temps réel sans justification.
- Conserver les responsabilités des managers.
- Éviter les dépendances circulaires.
- Ajouter les nouveaux chemins de log avec `EventLog`.
- Toute constante matérielle compile-time va dans `config.h`.
- Toute valeur modifiable en production va dans ConfigManager.

## Mémoire, flux et régressions indirectes

Cette règle s’applique à tous les développements ESP32, ESP8266 et Arduino réalisés à partir de ce socle.

- Ne pas évaluer la mémoire d’une fonction isolément. Évaluer le pic transitoire du chemin complet : pile de tâche, buffers réseau, `String`, documents JSON, caches, sprites, structures runtime et allocations concurrentes.
- Toute création de tâche FreeRTOS ajoute une pile dédiée et peut réduire la marge disponible du heap global. Ce coût doit être pris en compte avant de déplacer une fonction précédemment synchrone vers une tâche asynchrone.
- Sur une cible sans PSRAM, ne pas charger intégralement une réponse réseau volumineuse dans un `String` lorsqu’une lecture en flux est possible.
- Pour JSON, CSV, fichiers, réponses HTTP et autres données potentiellement volumineuses, préférer le parsing en flux, les filtres de champs, les buffers bornés ou le traitement par morceaux.
- Éviter de conserver simultanément plusieurs représentations d’une même donnée, par exemple payload complet plus document JSON plus copie de diagnostic.
- Toute augmentation de l’empreinte RAM globale impose de retester les fonctions déjà stables qui utilisent des allocations transitoires importantes, même si leur code n’a pas été modifié.
- Une refonte asynchrone, l’ajout d’observabilité, d’un cache ou d’une structure runtime doit être considérée comme susceptible de provoquer une régression indirecte dans un autre module.
- Avant validation, relever au minimum le heap libre au repos, le heap avant l’opération sensible, le résultat de l’opération et le heap après libération lorsque l’environnement le permet.
- Les diagnostics doivent distinguer autant que possible : erreur réseau, réponse vide, erreur de format, manque de mémoire et authentification refusée.
- Ne pas considérer une fonction comme validée uniquement parce qu’elle compilait ou fonctionnait avant une refonte d’architecture.

## Validation des fichiers et artefacts applicables

- Tout patch, diff, script, archive, fichier de configuration ou fichier de transformation destiné à l’utilisateur doit être testé avant publication avec l’outil exact qui sera utilisé par l’utilisateur.
- Un patch Git doit être généré depuis une base fidèle, et non rédigé manuellement lorsque l’outil `git diff` peut le produire.
- La validation minimale d’un patch Git comprend obligatoirement :
  1. `git apply --check <patch>` sur la base ciblée ;
  2. application réelle dans une copie ou un worktree temporaire ;
  3. `git diff --check` après application ;
  4. vérification des fichiers et fonctions réellement modifiés ;
  5. absence de duplication et de changement hors périmètre.
- Ne jamais publier un patch avec des en-têtes ou compteurs de hunk écrits ou modifiés manuellement sans régénération et revalidation complète.
- Une archive doit être ouverte et extraite dans un répertoire temporaire ; son contenu, ses chemins et l’absence de fichiers parasites doivent être contrôlés.
- Un script doit au minimum passer un contrôle syntaxique ou un mode non destructif lorsqu’il existe.
- Si l’environnement ne permet pas la validation complète, le fichier ne doit pas être présenté comme validé. La limitation et la commande de contrôle locale doivent être indiquées explicitement.
- Ne jamais utiliser l’utilisateur comme premier testeur d’un artefact que l’environnement de développement pouvait vérifier.

## Versionnement et identité du produit

Cette règle est obligatoire pour AquaLook et doit être reprise dans tout nouveau projet développé.

- Proposer un numéro de version dès qu’un projet devient exécutable ou publiable.
- Utiliser une version fonctionnelle lisible, de préférence compatible avec le versionnement sémantique : `MAJEUR.MINEUR.CORRECTIF`, avec suffixe éventuel `-dev`, `-beta` ou `-rc`.
- Ne pas déduire automatiquement la version fonctionnelle du seul nombre de commits ou d’une estimation subjective de l’avancement.
- Compléter la version fonctionnelle par une identité de build automatique lorsque Git est disponible : numéro de build, SHA court, branche ou origine et date de compilation.
- Centraliser ces informations dans une source unique du code, par exemple `BuildInfo.h`, un fichier généré ou des constantes de compilation.
- Ne jamais dupliquer manuellement des numéros de version différents dans plusieurs fichiers.
- Prévoir dans chaque projet un espace durable de type `À propos`, `Système`, `Version`, écran de démarrage, commande CLI ou endpoint de diagnostic.
- Cet espace doit afficher au minimum le nom du produit, la version fonctionnelle et l’identifiant exact du build.
- Lorsque le produit possède plusieurs interfaces, réutiliser la même source de version sur toutes les interfaces pertinentes.
- Proposer explicitement une nouvelle version lors d’un palier fonctionnel, d’une release, d’une évolution significative ou d’une correction publiée.
- Inclure la version et le SHA dans les checkpoints, archives, releases et procédures de reprise.
- Tout écran d’accueil, splash ou page de démarrage contenant une identité ou une version doit rester visible assez longtemps pour être lu. Prévoir un temps minimal explicite, configurable et documenté ; valeur de référence : 1,5 seconde sur un écran embarqué, sans ralentir inutilement les redémarrages critiques.

## Persistance

Avant de modifier une struct persistée :

1. mesurer `sizeof(PersistedConfig)` ;
2. décider d’un changement de schéma ;
3. définir le comportement pour l’ancien blob ;
4. vérifier CRC ;
5. tester valeurs par défaut ;
6. tester reset ;
7. documenter la migration.

Ne jamais changer silencieusement l’ordre, le type ou la taille d’un champ persisté.

## Web

### HTML

- IDs uniques.
- Ne pas dupliquer un bloc lors d’un déplacement.
- Utiliser les IDs existants.
- Éviter les commentaires volumineux dans le fichier embarqué.
- Ne pas ajouter de backup dans `data/`.

### JavaScript

- Conserver les signatures appelées par les attributs HTML.
- Gérer les erreurs `fetch`.
- Ne pas supposer qu’une réponse est arrivée après un reboot.
- Ne pas stocker de secret dans le frontend.
- Éviter frameworks et dépendances externes.

### CSS

- Réutiliser variables et classes existantes.
- Éviter le CSS inline supplémentaire lorsque la taille le permet.
- Contrôler desktop et mobile.
- Ne pas déplacer du CSS vers un autre fichier en prétendant réduire la taille totale LittleFS.

## LCD

- Utiliser la palette Theme.
- Préserver les sprites réutilisés.
- Éviter les redraws complets.
- Tester les modes 1, 2, 4 et 8 zones.
- Vérifier le touch après modification de géométrie.
- Ne pas inclure séparément des polices GFX déjà fournies par TFT_eSPI.

## Relais

- Toute modification est critique.
- Tester état initial OFF, direct, inverse, contrôleur, première et dernière zone, arrêt manuel, timeout et reboot.
- Ne jamais utiliser un test matériel long par défaut.

## Logs

Niveaux : INFO, WARN, ERROR. Le buffer est borné ; les messages doivent être courts et actionnables.

## Commentaires

Documenter pourquoi une contrainte existe, le risque supprimé et l’invariant concerné. Éviter les commentaires qui répètent le code.

## Encodage

- UTF-8 sans BOM pour les sources Web et C++ sauf contrainte outil.
- Vérifier emojis et caractères accentués dans les scripts PowerShell.
- Préférer des marqueurs ASCII dans les scripts de transformation.
