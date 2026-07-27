# AquaLook Developer Guide — Bonnes pratiques de développement

- Référence : DEV-005
- Statut : actif
- Maturité : D4

## Base de travail

- partir du dernier checkpoint et du code réel ;
- utiliser une branche dédiée ;
- ne pas recréer un fichier complet depuis la mémoire ;
- préserver les invariants et fallbacks validés ;
- distinguer clairement legacy, V4, bancs et code prospectif.

## Code

- API minimale et types explicites ;
- buffers bornés et chaînes terminées ;
- pas d’allocation dynamique répétitive dans la boucle ;
- pas d’appel bloquant sans timeout ;
- gestion correcte du débordement de `millis()` ;
- validation des entrées à la frontière ;
- propriétaire unique pour chaque ressource matérielle ou persistante ;
- logs utiles, limités et sans secret ;
- modes dégradés définis.

## ESP32 et Runtime

- conserver un boot anti-blocage ;
- ne jamais attendre indéfiniment Serial, Wi-Fi, NTP ou HTTP ;
- différer les écritures et redémarrages hors AsyncTCP ;
- mesurer les traitements ajoutés à `loop()` ;
- vérifier le heap et le plus grand bloc lors d’ajouts réseau/TLS ;
- tester sur matériel toute modification électrique.

## Web et données

- une route est un contrat versionné ;
- aucune donnée sensible dans une URL ;
- schémas JSON validés ;
- pas de changement NVS silencieux ;
- migrations et valeurs par défaut explicites ;
- ressources SD non autorisées à masquer `/api/` ;
- cache adapté à la sensibilité de la réponse.

## Git et livraison

- diff limité au besoin ;
- `git diff --check` propre ;
- commits intentionnels ;
- builds des environnements concernés ;
- contrats et tests exécutés ;
- test matériel déclaré réalisé ou non réalisé ;
- documentation et checkpoint mis à jour avant fusion.

## Documentation synchronisée

Toute modification majeure vérifie quatre niveaux :

1. tome `docs/engineering/` ;
2. fiche `docs/firmware/` ;
3. guide `docs/developer/` si la méthode change ;
4. checkpoint et registre de traçabilité.

## Références

- `AGENTS.md`
- `docs/engineering/05_EDITORIAL_STANDARD.md`
- `docs/engineering/30_TEST_AND_ANTI_REGRESSION_MATRIX.md`
- `docs/engineering/CODE_LINKED_REFERENCE_PROCESS.md`
