# Checkpoint AquaLook — 2026-07-01

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `feature/resilience-coupures`
- Commit fonctionnel figé : `5aaf95eb73762b7197afc1b8af9635a5eaf33782`
- Environnement PlatformIO : `ProgrammeArrosage`
- Carte : ESP32 `esp32dev`, flash 4 Mio
- Système de fichiers : LittleFS

## Évolutions intégrées

### Résilience

- Redémarrage automatique après échecs Wi-Fi répétés.
- Reprise d’un arrosage programmé interrompu par un reboot.
- Recalcul du temps restant après synchronisation NTP.
- Gestion des programmations traversant minuit.
- Les arrosages manuels ne sont pas restaurés après reboot.

### LED RGB

Priorités :

1. erreur persistante : rouge clignotant ;
2. arrosage actif : respiration bleue ;
3. écran en veille : mode configuré ;
4. écran allumé sans erreur : LED éteinte.

### Partitionnement de développement

- OTA désactivé temporairement.
- Table active : `partitions_aqualook_dev.csv`.
- Application : 2,5 Mio.
- LittleFS : environ 1,44 Mio.
- Partition nommée `spiffs`, sous-type `spiffs`.

Après changement de partition :

```powershell
pio run -e ProgrammeArrosage -t clean
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t upload
pio run -e ProgrammeArrosage -t uploadfs
```

### Interface Web

- Hachures pour les zones en mode intervalle.
- Nom de zone sur fond uni pour préserver la lisibilité.
- Jours non planifiés grisés et hachurés.
- Même code visuel dans les cartes de zones en partie haute.
- Gestion automatique en vue normale et dense.

### LCD

- Suppression du fond coloré des jours avec données météo.
- Conservation des icônes, températures et jauges de pluie.
- Marquage des zones intervalle dans la pastille de couleur.
- Dernière version : contour contrasté, diagonales renforcées et taille augmentée quand possible.

### Erreurs système

Composants ajoutés :

- `SystemHealth.h/.cpp`
- `DisplayHealthUi.h/.cpp`
- `DisplayPlanningDecor.h/.cpp`

Défauts gérés : LittleFS, Wi-Fi et NTP.

Comportement :

- LED rouge clignotante ;
- icône d’alerte dans la barre supérieure ;
- appui sur l’icône : ouverture de la page Logs ;
- recommandation contextuelle ;
- bouton de redémarrage uniquement lorsqu’il est pertinent ;
- pour LittleFS, message de rechargement USB.

### Roadmap

Document durable : `docs/ROADMAP.md`.

Il conserve notamment :

- architecture future SD + OTA ;
- interface LittleFS minimale de secours ;
- upload des ressources SD depuis PlatformIO ;
- catalogue externe de diagnostics ;
- sauvegardes, exports et journalisation.

## Validations réalisées

- Nouveau partitionnement et accès Web : validés.
- Hachures Web et lisibilité du nom : validées.
- Respiration bleue pendant l’arrosage : validée.
- Arc-en-ciel LED continu : validé avant les derniers ajouts LCD.
- Suppression du fond météo LCD : observée après flash de la première version.

## Validation restante

Le commit `5aaf95eb73762b7197afc1b8af9635a5eaf33782` doit encore être compilé, flashé et vérifié visuellement pour confirmer le renforcement des hachures LCD.

```powershell
git pull origin feature/resilience-coupures
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t upload
```

Aucun `uploadfs` n’est requis pour cette dernière modification.

## Points de vigilance

- `DisplayManager.cpp` est volumineux et sensible : éviter toute réécriture large.
- Les modules de décor LCD utilisent un accès temporaire aux membres privés de `DisplayManager` pour limiter le risque de régression. Cette dette devra être résorbée lors d’une refonte de l’affichage.
- Après toute évolution Web : vérifier les tailles, les doublons et la construction LittleFS.
- La SD ne devra jamais devenir indispensable au pilotage des relais ou à la configuration critique.
- L’OTA reste volontairement absent pendant le développement.

## Reprise

```powershell
git status
git branch -vv
git log -5 --oneline
pio run -e ProgrammeArrosage
```

Ne pas reconstruire les fichiers depuis des extraits ou des versions antérieures.