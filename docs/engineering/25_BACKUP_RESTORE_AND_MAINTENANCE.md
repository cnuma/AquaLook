# AquaLook Engineering Reference — Sauvegarde, restauration et maintenance

- Version documentaire : 1.0
- Statut : référence initiale
- Dernière consolidation : 2026-07-27
- Sources : gouvernance documentaire, registre des risques, checkpoints, roadmaps stockage
- Composants : configuration, NVS, LittleFS, microSD, firmware, exploitation
- Maturité : D3

## Mission

Ce document définit les principes de sauvegarde, restauration, remplacement et maintenance permettant de reprendre AquaLook sans perdre la configuration validée ni contourner les sécurités.

## Périmètre sauvegardé

Une sauvegarde complète peut inclure :

- configuration active et version de schéma ;
- programmes et paramètres des zones ;
- calibration et paramètres matériels nécessaires ;
- ressources utilisateur autorisées ;
- inventaire du matériel et version firmware ;
- manifeste des fichiers et sommes de contrôle ;
- journaux utiles à un incident, selon la politique de rétention.

Les secrets ne sont inclus que si une procédure chiffrée, contrôlée et explicitement documentée le prévoit.

## Éléments exclus

- caches et données volatiles ;
- fichiers temporaires ;
- secrets de publication OTA ;
- clés privées non destinées à l’équipement ;
- logs sans utilité de reprise ;
- copies obsolètes non identifiées.

## Types de sauvegarde

### Configuration

Export des données persistantes nécessaires à la reconstruction fonctionnelle.

### Checkpoint de développement

Archive du dépôt sans `.git`, `.pio`, secrets, logs et fichiers temporaires, accompagnée d’un SHA-256 et d’un document de reprise.

### Sauvegarde d’exploitation

Copie destinée à restaurer une installation après panne, remplacement ou erreur de configuration.

## Cycle de sauvegarde

```mermaid
flowchart LR
  READ[Lecture cohérente] --> MAN[Création du manifeste]
  MAN --> ARCH[Création archive]
  ARCH --> HASH[SHA-256]
  HASH --> TEST[Test d'ouverture]
  TEST --> STORE[Stockage contrôlé]
```

Une sauvegarde n’est déclarée valide qu’après ouverture et contrôle de son contenu.

## Restauration

1. identifier le matériel et la version cible ;
2. vérifier le manifeste et les sommes de contrôle ;
3. contrôler la compatibilité du schéma ;
4. préserver une copie de l’état courant si possible ;
5. restaurer dans un ordre documenté ;
6. redémarrer en mode contrôlé ;
7. exécuter les auto-tests ;
8. vérifier écran, Web, Scheduler et relais ;
9. journaliser le résultat.

## Remplacement d’un contrôleur

Le remplacement exige :

- identification du modèle matériel ;
- firmware compatible ;
- restauration ou reprovisionnement des paramètres ;
- rotation des identités et secrets lorsque nécessaire ;
- validation d’une seule zone, durée courte, sous surveillance ;
- mise à jour de l’inventaire.

## Maintenance préventive

- vérifier les sauvegardes ;
- contrôler l’espace microSD et LittleFS ;
- vérifier les versions et dépendances ;
- contrôler les certificats et secrets ;
- consulter les incidents récurrents ;
- vérifier les relais et équipements ;
- confirmer la synchronisation temporelle ;
- nettoyer uniquement les données prévues par la politique de rétention.

## Maintenance corrective

Une intervention corrective conserve :

- description du symptôme ;
- version et SHA ;
- journaux utiles ;
- diagnostic ;
- fichiers modifiés ;
- tests réalisés ;
- résultat matériel ;
- procédure de retour arrière.

## Maintenance évolutive

Toute évolution suit la branche dédiée, les validations PlatformIO, les tests matériels pertinents et la consolidation du référentiel lors du checkpoint.

## Modes d’échec

### Archive corrompue

La restauration est refusée. Une autre sauvegarde ou une reconstruction contrôlée est utilisée.

### Schéma incompatible

Une migration explicite est requise ; aucune conversion implicite destructive.

### microSD absente

La configuration active en NVS reste prioritaire. Les ressources Web utilisent les fallbacks documentés.

### Secret compromis

Le secret n’est pas restauré tel quel ; il est révoqué et reprovisionné.

## Risques associés

- `SEC-006` : secrets dans les archives ;
- `SEC-015` : ressources SD modifiées ;
- `SEC-017` : révocation impossible ;
- `SEC-018` : sauvegarde inutilisable.

## Invariants

### INV-MNT-001

Une sauvegarde non testée n’est pas considérée valide.

### INV-MNT-002

Une restauration ne contourne ni la validation de schéma ni la sécurité des relais.

### INV-MNT-003

Les secrets compromis sont révoqués plutôt que recopiés.

### INV-MNT-004

Toute intervention matérielle se termine par un test contrôlé et tracé.

### INV-MNT-005

Le checkpoint documentaire et le référentiel sont mis à jour avant clôture d’un jalon significatif.

## Références

- `AGENTS.md` ;
- `docs/engineering/07_CONFIGURATION_AND_PERSISTENCE.md` ;
- `docs/engineering/11_CHECKPOINT_CONSOLIDATION.md` ;
- `docs/engineering/14_SD_AND_STATIC_RESOURCES.md` ;
- `docs/engineering/17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md` ;
- `docs/security/SECURITY_RISK_REGISTER.md`.

## Historique

### 1.0

Première consolidation des procédures de sauvegarde, restauration et maintenance.