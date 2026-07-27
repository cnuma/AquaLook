# AquaLook Developer Guide — Ajouter une ressource Web

- Référence : DEV-008
- Statut : actif
- Maturité : D4

## Supports

- `data/` : contenu de référence destiné à la carte SD ;
- `littlefs/` : contenu embarqué par `buildfs` ;
- firmware : fallback minimal pour les ressources critiques.

## Étapes

1. déterminer si la ressource est critique ;
2. choisir SD, LittleFS ou fallback firmware ;
3. déclarer le chemin et le type MIME ;
4. empêcher tout masquage d’une route `/api/` ;
5. définir la politique de cache ;
6. borner les chemins et rejeter `..` ;
7. mettre à jour le futur manifeste SD ;
8. tester toutes les combinaisons de stockage ;
9. exécuter `buildfs` si `littlefs/` change ;
10. documenter l’origine servie via les diagnostics.

## Sécurité

Une carte SD est une entrée non fiable. Ne jamais y placer une clé privée, un secret, une route administrative ou une ressource capable de contourner les contrôles serveur.

## Validation

Tester ressource présente, absente, tronquée, mauvais MIME, SD retirée et fallback indisponible. Vérifier les en-têtes de cache et `X-AquaLook-Storage`.

## Références

- `docs/firmware/FW-008_StorageManager.md`
- `docs/engineering/14_SD_AND_STATIC_RESOURCES.md`
