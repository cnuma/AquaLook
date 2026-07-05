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
