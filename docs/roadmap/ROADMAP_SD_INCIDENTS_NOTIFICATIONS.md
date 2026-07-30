# Roadmap proche — fiabilisation SD, incidents persistants et notifications

## Statut et priorité

- Priorité : très prochaine, avant la reprise de RUN7.11.
- Branche de travail : `work/storage-sd-recovery`.
- Base initiale : checkpoint RUN7.10, commit `0d736456a4e4dd37ddd43e42aced2426cfb92051`.
- Périmètre : stockage SD, journalisation, incidents opérateur et notifications sortantes.
- Hors périmètre initial : autorité physique de l’orchestrateur, pilotage cloud distant et notifications d’information courante.

## Constat actuel

Le journal technique `EventLog` est un buffer circulaire conservé uniquement en RAM. Il contient au maximum 60 entrées et disparaît au redémarrage.

Le défaut SD est également suivi en RAM. Lorsqu’une lecture échoue ou que la carte devient indisponible, le gestionnaire ferme la carte et ne tente actuellement aucun remontage automatique. Un redémarrage efface en outre l’historique de l’incident et son état d’acquittement.

Cette situation n’est pas acceptable pour une installation en production difficilement accessible.

## Objectif 1 — récupération automatique de la carte SD

Mettre en place une récupération non bloquante après une perte ou une erreur de lecture de la carte SD.

Exigences :

- détecter une perte réelle de la carte ou du volume sans conclure sur une seule anomalie transitoire ;
- fermer proprement le système de fichiers avant une nouvelle tentative ;
- retenter le montage selon une temporisation progressive ;
- valider la carte, le volume et la présence de `/www/index.html` après remontage ;
- revenir automatiquement à l’état opérationnel si la récupération réussit ;
- conserver l’interface LittleFS de secours pendant toute l’indisponibilité ;
- ne jamais bloquer le planificateur, les relais, l’écran ou le serveur Web ;
- ne jamais provoquer de boucle de redémarrage automatique ;
- recommander un redémarrage à l’utilisateur après épuisement des tentatives, sans l’imposer automatiquement.

Séquence initiale proposée :

1. tentative après 2 secondes ;
2. tentative après 5 secondes ;
3. tentative après 10 secondes ;
4. tentative après 30 secondes ;
5. tentative après 60 secondes ;
6. passage en incident actif si les cinq essais échouent.

## Objectif 2 — séparer journal technique et incidents opérateur

Conserver deux mécanismes distincts.

### Journal technique volatile

Le journal existant reste destiné au diagnostic immédiat :

- niveaux `INFO`, `WARN` et `ERROR` ;
- buffer circulaire en RAM ;
- remplacement naturel des entrées anciennes ;
- disparition acceptable au redémarrage ;
- absence d’écriture flash à chaque message.

Les tentatives de remontage SD, les résultats HTTP et les transitions techniques y sont enregistrés sans répétition excessive.

### Incidents persistants

Ajouter un gestionnaire d’incidents persistants stockés en NVS.

Un incident persistant doit :

- survivre au redémarrage et à une coupure d’alimentation ;
- posséder un identifiant stable ;
- conserver son état actif, récupéré ou acquitté ;
- conserver un compteur d’occurrences ;
- conserver la première et la dernière détection lorsque l’heure est disponible ;
- rester présenté tant que l’utilisateur ne l’a pas acquitté ;
- distinguer clairement résolution technique et acquittement humain ;
- limiter les écritures NVS aux transitions importantes.

Incidents SD initiaux :

- `STORAGE_SD_RUNTIME_LOST` ;
- `STORAGE_SD_RECOVERY_FAILED` ;
- `STORAGE_SD_ASSETS_MISSING` ;
- `STORAGE_SD_REPEATED_FAILURES`.

Une récupération automatique ne supprime pas silencieusement l’incident. L’état devient « récupéré, non acquitté » afin que l’utilisateur puisse vérifier la carte, son insertion, son alimentation ou sa qualité.

## Objectif 3 — présentation et acquittement

Ajouter une vue locale des incidents nécessitant une action.

Pour chaque incident, présenter au minimum :

- type et gravité ;
- état actuel ;
- première et dernière détection ;
- nombre d’occurrences ;
- résultat de la dernière récupération ;
- action recommandée ;
- état d’acquittement.

L’acquittement signifie uniquement que l’utilisateur a pris connaissance de l’incident. Il ne doit jamais désactiver un défaut matériel encore actif.

Le nettoyage du journal technique ne doit plus acquitter implicitement les incidents persistants.

## Objectif 4 — notifications critiques via ntfy

Ajouter un canal de notification sortant basé initialement sur le service `ntfy.sh`.

Le service ntfy est un transport des incidents persistants, pas un second journal.

Notifications SD initiales :

- perte confirmée de la carte pendant le fonctionnement ;
- épuisement des tentatives de remontage ;
- récupération réussie après une panne déjà notifiée ;
- répétition anormale de pertes sur une période courte.

Ne pas notifier :

- chaque contrôle de santé ;
- chaque tentative individuelle ;
- les événements d’information ordinaires ;
- les démarrages et arrêts normaux des zones.

## File de notification et reprise après panne réseau

Une notification critique doit être conservée tant qu’elle n’a pas été acceptée par le service distant.

L’incident persistant doit suivre séparément :

- notification requise ;
- notification acceptée par ntfy ;
- nombre de tentatives ;
- dernière tentative ;
- prochaine échéance de réessai.

La connexion au Wi-Fi ne suffit pas à considérer Internet disponible. Seule une réponse HTTP de succès du service distant confirme l’envoi.

Politique initiale de réessai proposée :

1. immédiatement ;
2. après 1 minute ;
3. après 5 minutes ;
4. après 15 minutes ;
5. après 1 heure ;
6. ensuite toutes les 6 heures tant que la notification reste nécessaire.

La file doit survivre au redémarrage pour les incidents critiques non livrés.

## Anti-spam

- confirmer la perte SD par plusieurs contrôles cohérents ;
- une seule notification initiale par occurrence d’incident ;
- une seule notification d’escalade après échec complet de récupération ;
- une seule notification de retour à la normale ;
- regrouper les récidives rapprochées et inclure un compteur ;
- ne jamais envoyer une notification à chaque passage dans `loop()`.

## Configuration et sécurité ntfy

La configuration doit être stockée en NVS et désactivée par défaut.

Paramètres minimaux :

- activation du service ;
- URL du serveur, initialement `https://ntfy.sh` ;
- sujet ;
- jeton d’accès éventuel ;
- gravité minimale à notifier ;
- bouton de test manuel.

Contraintes :

- ne pas utiliser un sujet public trivial tel que `aqualook` en production ;
- utiliser un sujet long et difficile à deviner ou une authentification par jeton ;
- ne jamais versionner le sujet réel ni le jeton dans Git ;
- ne jamais afficher ces secrets dans les logs ou les diagnostics ;
- utiliser HTTPS avec validation TLS en production ;
- ne pas utiliser une connexion non vérifiée comme solution finale ;
- imposer des délais réseau courts et une exécution non bloquante pour le reste du firmware.

## Ordre de développement imposé

1. nettoyer les fichiers temporaires RUN7.10 dans un commit séparé ;
2. ajouter le remontage automatique et non bloquant de la SD ;
3. valider le fonctionnement avec retrait, réinsertion et erreurs de lecture simulées ;
4. ajouter le gestionnaire d’incidents persistants en NVS ;
5. découpler l’acquittement des incidents du nettoyage du journal technique ;
6. exposer les incidents dans l’interface locale et permettre leur acquittement individuel ;
7. ajouter le gestionnaire de notifications et sa file persistante ;
8. intégrer ntfy pour les seuls incidents critiques retenus ;
9. ajouter un test manuel de notification ;
10. réaliser les essais sans Internet, avec retour d’Internet et avec redémarrage intermédiaire ;
11. créer un checkpoint matériel validé ;
12. seulement ensuite reprendre RUN7.11.

## Découpage Git recommandé

- `chore: remove temporary RUN7.10 helper files`
- `fix: retry SD mount after runtime failures`
- `feat: persist actionable system incidents`
- `feat: expose and acknowledge persistent incidents`
- `feat: send critical incidents through ntfy`
- `docs: checkpoint SD recovery and notifications`

Chaque commit doit rester compilable, testable et réversible indépendamment.

## Validation minimale

- compilation Legacy et V4 ;
- tests unitaires ou bench des machines d’état de récupération ;
- retrait de la carte en fonctionnement ;
- réinsertion avant et après épuisement des essais ;
- maintien du fallback LittleFS ;
- absence de blocage du planificateur et des relais ;
- persistance des incidents après reboot ;
- acquittement individuel sans effacement du défaut actif ;
- envoi ntfy avec Internet disponible ;
- conservation puis envoi après retour d’Internet ;
- absence de doublons après reboot ;
- absence de secret dans les logs et le dépôt ;
- upload et monitor explicitement sur `COM3`, monitor en dernière commande.

## Invariants impératifs

- l’arrosage local reste autonome, même sans SD, Wi-Fi, Internet ou ntfy ;
- une panne du système de notification ne devient jamais une panne fonctionnelle du programmateur ;
- aucune requête réseau ne doit commander directement un relais ;
- aucune tentative SD ou réseau ne doit bloquer la boucle principale ;
- aucun redémarrage automatique répété ne doit être introduit ;
- les incidents critiques ne doivent être perdus ni par remplissage du journal ni par reboot ;
- l’acquittement utilisateur ne doit jamais être confondu avec la résolution technique ;
- les secrets de notification ne doivent jamais être versionnés ou journalisés ;
- RUN7.11 reste suspendu jusqu’à validation matérielle de ce chantier de fiabilisation.
