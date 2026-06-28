# 11 — Orientations du projet

## Court terme

1. Stabiliser la branche `main`.
2. Ajouter une CI PlatformIO.
3. Corriger le doublon HTML historique.
4. Retirer ou conditionner le scan I²C temporaire.
5. Valider complètement le contrôleur MCP23017.
6. Préserver la marge LittleFS.

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
