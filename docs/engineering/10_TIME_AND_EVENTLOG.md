# AquaLook Engineering Reference — Temps et journalisation

- Version documentaire : 1.0
- Statut : référence initiale
- Dernière consolidation : 2026-07-27
- Sources : checkpoints EventLog/NTP, code du dépôt
- Composants : gestion NTP, horodatage, EventLog centralisé
- Maturité : D3

## Mission

La gestion du temps fournit une chronologie continue dès le boot. L’EventLog centralise les événements significatifs et conserve leur ordre, leur origine et leur gravité sans bloquer le Runtime.

## Politique d’horodatage

### Avant synchronisation NTP

Les événements utilisent `millis()` comme temps relatif depuis le démarrage. Cette référence permet d’ordonner le boot et les incidents réseau initiaux.

### Après synchronisation NTP

Dès qu’une synchronisation valide est obtenue, les nouveaux événements utilisent l’heure absolue. Le changement de source temporelle doit être identifiable dans les logs.

La conservation conjointe d’un `uptime_ms` et d’un horodatage absolu, lorsqu’il est disponible, évite qu’une correction d’horloge détruise l’information de durée.

## Politique de synchronisation

- première tentative après connexion Wi-Fi stable ;
- serveur historique : `pool.ntp.org` ;
- resynchronisation de référence : toutes les six heures ;
- nouvelle tentative après reconnexion réseau ;
- échec NTP : conservation de l’horloge locale et poursuite du fonctionnement ;
- correction significative : journalisation du delta.

La valeur exacte de l’intervalle doit rester centralisée dans le code ou la configuration, et non dupliquée dans plusieurs modules.

## EventLog

L’EventLog reçoit des événements provenant du boot, de la planification, des relais, du stockage, du réseau, du Web, de l’OTA et de la sécurité.

Structure minimale attendue :

| Champ | Usage |
|---|---|
| identifiant | unicité ou séquence |
| source | composant émetteur |
| type/code | classification stable |
| gravité | information, avertissement, erreur, critique |
| `uptime_ms` | chronologie monotone depuis le boot |
| timestamp absolu | présent après synchronisation |
| message/contexte | données de diagnostic |

## Invariants

### INV-TIME-001

Le système commence à journaliser avant la disponibilité du réseau.

### INV-TIME-002

La perte du NTP ne bloque ni le Scheduler ni les relais.

### INV-TIME-003

Le passage de `millis()` à l’heure absolue est traçable.

### INV-EVT-001

La journalisation ne bloque pas la boucle principale.

### INV-EVT-002

Les événements critiques restent disponibles même lorsque les canaux distants sont indisponibles.

## Modes dégradés

- NTP indisponible : chronologie relative et horloge locale ;
- SD absente : journal réduit ou persistance alternative selon le checkpoint ;
- stockage saturé : politique de rétention et signalement ;
- service de notification indisponible : conservation locale de l’événement.

## Interfaces

Les URL de consultation des logs ne sont pas déclarées ici tant qu’elles ne sont pas extraites du code courant. Les routes envisagées dans les conversations ne constituent pas des interfaces confirmées.

## Tests

- événements avant connexion Wi-Fi ;
- passage à l’heure NTP ;
- resynchronisation après six heures ou reconnexion ;
- échec NTP ;
- correction de date importante ;
- absence de blocage sous rafale d’événements ;
- comportement sans SD.

## Références

- checkpoints de centralisation de l’EventLog ;
- checkpoints de diagnostic TLS/ntfy ;
- documents Observabilité et Cybersécurité ;
- implémentation NTP du dépôt.

## Historique

### 1.0

Première consolidation de la chronologie et de l’EventLog.
