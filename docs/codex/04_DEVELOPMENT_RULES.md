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
