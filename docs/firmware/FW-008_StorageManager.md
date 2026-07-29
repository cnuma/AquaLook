# AquaLook Firmware — StorageManager et SdStaticHandler

- Référence : FW-008
- Statut : relié au code
- Maturité : D4
- Sources : `src/StorageManager.*`, `src/SdStaticHandler.*`

## Mission

Monter et superviser la microSD, puis servir les ressources statiques autorisées avec des fallbacks sûrs.

## Chaîne de service

1. montage SdFat ;
2. contrôle périodique de la sentinelle `/www/index.html` ;
3. résolution des ressources SD ;
4. fallback vers handler embarqué ou LittleFS ;
5. réponse avec type MIME, cache et en-tête d’origine.

## Invariants

- aucune ressource SD ne masque `/api/` ;
- rejet des traversées de chemin ;
- les ressources critiques gardent un fallback ;
- une absence de SD ne bloque pas l’arrosage ;
- les erreurs de lecture restent observables.

## Limites

- manifeste de contenu SD encore à versionner ;
- `usedBytes` incomplet dans l’état actuel ;
- l’intégrité cryptographique des ressources n’est pas encore validée.

## Validation

Tester les matrices SD présente/absente/corrompue, LittleFS disponible/absent et ressources firmware. Vérifier `/api/storage`, les en-têtes de cache et les types MIME.

## Références

- `docs/engineering/14_SD_AND_STATIC_RESOURCES.md`
- `docs/engineering/27_FILE_AND_STORAGE_MAP.md`
- `docs/developer/DEV-008_Ajouter_une_ressource_Web.md`
