# 11 — Orientations du projet

## Court terme

1. Stabiliser la branche `main`.
2. Ajouter une CI PlatformIO.
3. Corriger le doublon HTML historique.
4. Retirer ou conditionner le scan I²C temporaire.
5. Valider complètement le contrôleur MCP23017.
6. Préserver la marge LittleFS.

## Gestion de la carte SD et préparation de l’OTA

Créer une branche dédiée :

`feature/sd-web-assets-ota`

Objectifs :

- mettre en place la détection, le montage et le contrôle de disponibilité de la carte SD ;
- identifier les ressources statiques du serveur Web pouvant être déplacées hors de LittleFS ;
- servir depuis la carte SD les fichiers Web adaptés, notamment les ressources CSS, JavaScript, images et autres fichiers statiques volumineux ;
- conserver dans LittleFS uniquement les ressources indispensables au démarrage, au diagnostic et au mode de secours ;
- prévoir un fonctionnement dégradé explicite lorsque la carte SD est absente, illisible ou corrompue ;
- mesurer la place réellement libérée dans la flash ;
- adapter ensuite le partitionnement pour réserver suffisamment d’espace aux mises à jour OTA ;
- valider que l’OTA ne modifie ni la configuration NVS ni les données présentes sur la carte SD.

Critères de validation :

- la carte SD est initialisée sans bloquer le démarrage ;
- les ressources Web déplacées sont correctement servies depuis la carte SD ;
- l’absence de carte SD ne rend pas le contrôleur inutilisable ;
- la taille de LittleFS et l’espace firmware libéré sont mesurés avant et après migration ;
- le partitionnement OTA compile et accepte une image complète du firmware ;
- la sécurité des relais reste inchangée pendant et après une mise à jour OTA.

## Évolution débitmètres

Orientation envisagée :

- Lolin S2 Mini comme coprocesseur de comptage ;
- plusieurs entrées impulsions locales ;
- communication avec la YellowCard par I²C ;
- AquaLook reste maître de l’orchestration ;
- les impulsions ne doivent pas être comptées par polling lent.

Points à décider avant code : protocole I²C, format atomique des compteurs, fréquence de lecture, remise à zéro, overflow, calibration impulsions/litre, persistance, perte du secondaire, alimentation et masses.

## Sécurité

Évolution recommandée : authentification admin côté serveur, mot de passe configurable et hashé, contrôle des routes sensibles, expiration de session et aucun secret dans `index.html`.

## Testabilité

Extraire les décisions de planning dans du code testable et couvrir minuit, intervalle, pluie, doublons, sécurité de durée et changements NTP.

## Observabilité

Conserver EventLog, puis envisager compteurs de reboot, dernière cause reset, état I²C, erreurs météo, échecs NVS, statistiques d’arrosage et volume.

## Capacité

Ne pas augmenter au-delà de 8 zones avant validation de l’interface LCD, du Web, de la NVS, de la RAM, du contrôleur physique, de l’ergonomie et de l’alimentation.
