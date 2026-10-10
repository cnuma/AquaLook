# 08 — Risques, limites et dette technique

## Risques critiques

### R1 — Activation involontaire des relais

Causes possibles : logique directe/inverse incorrecte, état initial du contrôleur, erreur de mapping ou changement de contrôleur.

Mesures : état sûr au boot, tests courts, durée maximale, documentation des sorties et validation matérielle.

### R2 — Saturation LittleFS

La marge est très faible. Quelques centaines d’octets peuvent suffire à faire échouer buildfs.

Mesures : mesurer data, supprimer les commentaires embarqués inutiles, ne jamais mettre de backup dans data et exécuter buildfs.

### R3 — Corruption ou incompatibilité NVS

Causes : changement de struct, taille ou ordre modifié, schéma non incrémenté ou migration absente.

Mesures : version, magic, taille, CRC, test de repli et checkpoint avant migration.

### R4 — Heure incorrecte

Effet : arrosage au mauvais moment ou aucun démarrage.

Mesures : NTP requis, offsets vérifiés et statut NTP exposé.

### R5 — Verrouillage admin faible

Traité par le lot F de D016 (10 oct. 2026) : session Web côté module,
filtre unique sur toutes les écritures, gardes anti-CSRF et anti « DNS
rebinding ». Restent : HTTP en clair sur le LAN (cookie captable, risque
assumé), secret en clair dans la NVS (accès physique). Première pose
soumise à un geste sur l'écran (B5) et changement chiffré (B1) : validés le
10 oct. 2026.

## Dette technique identifiée

### D1 — Scan I²C temporaire au boot

Le retirer après validation matérielle ou le conditionner à un flag debug.

### D2 — Commentaire historique sur config.h

Le commentaire indique une duplication src/include à vérifier. Éviter deux sources divergentes.

### D3 — Environnement debug_boot

La procédure de renommage manuel de main.cpp est fragile. Créer un filtre de sources autonome.

### D4 — Protection admin côté frontend

Résolue par le lot F de D016 (verrou visuel retiré, session côté module).

### D5 — IDs HTML historiques

Un doublon de `btn-toggle-activity` a été observé. Le corriger dans une branche dédiée avec tests ciblés.

### D6 — CI absente ou non identifiée

Ajouter compilation PlatformIO, buildfs, diff check et validation structurelle HTML simple.

### D7 — Tests automatisés limités

Isoler et tester les règles de planning : pluie, intervalle, minuit et durée maximale.

### D8 — Capacité interne 16 / fonctionnelle 8

Conserver `MAX_ACTIVE_ZONES` comme garde unique et ajouter assertions et tests.

## Limites connues

- OpenWeatherMap dépend d’un service externe.
- La météo ne garantit pas la pluie réelle sur le jardin.
- Le système n’a pas encore de mesure de débit intégrée.
- Le support MCP23017 doit être validé matériellement.
- La YellowCard n’expose pas tous les GPIO ; l’I²C est l’axe d’extension privilégié.
