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

### D9 — Release GitHub sans binaire S3 (relevé le 10 oct. 2026)

`.github/workflows/ota-release.yml` compile `ProgrammeArrosage` et
`ProgrammeArrosage_v4` (carte CYD) ; `tools/generate_ota_manifest.py` ne
publie que les cibles `legacy` et `v4`, et `tools/version_build.py` donne
`ota_target = "unsupported"` à `ProgrammeArrosage_s3`. Une mise à jour
firmware par le manifeste GitHub ne trouverait donc pas sa cible sur `.141`
(`target-missing`, `MaintenanceBoot.cpp`). Les alias `_legacy` et `_v4` sont
gardés pour cette seule chaîne. À traiter comme un chantier à part : cible
`s3` dans les trois fichiers, décision documentée, essai de mise à jour sur
`.141`, puis retrait des alias.

### D10 — Résumé CloudSync qui écrasait le détail d'erreur (corrigé le 10 oct. 2026)

En fin de `CloudSync::run`, le résumé « ok, … » n'est conditionné qu'à
`reportSuccess` : si la télémétrie passe mais que la configuration échoue, le
détail utile (par exemple `config: http=500`) est remplacé, et le journal
ment au moment où il sert.

Corrigé le 10 oct. 2026 (`14d0130`, d'après le stash du 25 août 2026) : le
résumé exige rapport et configuration réussis et un détail encore vide.
Cycle nominal vérifié sur `.141` (build 1358 : rapport, config et sondage
en 200, aucun `WARN`) ; le cas config en échec n'a pas été provoqué sur le
matériel (il faudrait une erreur serveur).

### D11 — Choix du nœud Wi-Fi sur un mesh (10 oct. 2026)

Constat : sur le mesh Synology de `.141` (trois nœuds, un SSID, canal 11),
le module s'accrochait au premier nœud qui répondait, souvent le plus
lointain (`00:11:32:D3:9F:05`, −89 à −92 dBm), alors qu'un nœud était reçu à
−52 dBm (`00:11:32:A4:35:15`). Un reset tirait un autre nœud au hasard, d'où
le « ça remonte au reset ». Ni l'antenne ni le connecteur n'étaient en cause.

Corrigé (branche `feature/wifi-diag-bssid`, 5.13.0) : avant chaque
`WiFi.begin`, scan asynchrone après `STA_START` (150 ms par canal, ~1,3 s),
puis connexion au BSSID le plus fort du SSID ; une tentative sur deux reste
sur le choix du pilote (jamais bloqué sur un nœud qui refuserait
l'association). Le réglage « tous canaux + tri par signal » seul ne suffit
pas : le pilote a encore retenu le nœud à −91 dBm. Diagnostic ajouté :
`/api/diagnostics.wifi.bssid`, `/api/wifi/scan?all=1&refresh=1` (BSSID,
canal), BSSID dans le journal de connexion/déconnexion, `wifi {rssi, bssid,
channel}` dans chaque rapport CloudSync (historique serveur 90 jours).

Validé sur `.141` : 4 démarrages sur 4 sur le nœud proche (−54/−55 dBm),
dont 3 resets matériels (EN par esptool). Non testé : reconnexion après
coupure réelle (redémarrer le nœud `A4:35:15` : le module doit viser
`CE:6F:A2`), redémarrage logiciel. Reste ouvert (point 2) : changer de
nœud sans déconnexion si le lien se dégrade ; à décider d'après l'historique
serveur (vérification prévue vers le 14 oct. 2026). Antérieur et inchangé :
première association souvent refusée (`raison=202`), réussie ~1 s après.

## Limites connues

- OpenWeatherMap dépend d’un service externe.
- La météo ne garantit pas la pluie réelle sur le jardin.
- Le système n’a pas encore de mesure de débit intégrée.
- Le support MCP23017 doit être validé matériellement.
- La YellowCard n’expose pas tous les GPIO ; l’I²C est l’axe d’extension privilégié.
