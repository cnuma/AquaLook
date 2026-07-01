# Roadmap — AquaLook

Ce document centralise les évolutions à conserver pour les prochaines versions.
Il ne constitue pas une liste de tâches immédiates : chaque évolution devra être reprise dans une branche dédiée, analysée, documentée et validée avant intégration.

## Décision actuelle — partitionnement de développement

Statut : **retenu pour la branche de développement actuelle**.

- Utiliser une seule grande partition applicative.
- Désactiver le double partitionnement OTA pendant la phase de développement.
- Agrandir fortement LittleFS pour permettre l’évolution de l’interface Web.
- Continuer les mises à jour du firmware par USB depuis VS Code / PlatformIO.
- Table active : `partitions_aqualook_dev.csv`.

Répartition prévue sur une flash de 4 Mio :

- NVS : `0x5000` octets ;
- initialisation PHY : `0x1000` octets ;
- application principale : `0x280000` octets, soit 2,5 Mio ;
- LittleFS : `0x170000` octets, soit environ 1,44 Mio.

Cette organisation est destinée au développement et n’intègre volontairement aucune partition OTA.

## Option B — architecture cible avec carte SD et OTA

Statut : **à conserver pour une future branche dédiée**.

Nom de branche suggéré :

```text
feature/web-sd-ota
```

Objectif :

- réactiver les mises à jour OTA lorsque le produit sera suffisamment stabilisé ;
- conserver deux partitions applicatives capables de recevoir le firmware complet ;
- réduire LittleFS à une interface minimale de secours ;
- stocker l’interface Web complète sur la carte SD intégrée à la Yellow Card ;
- permettre la mise à jour des fichiers SD depuis VS Code, idéalement par Wi-Fi ;
- maintenir le fonctionnement de l’arrosage même si la carte SD est absente ou corrompue.

Architecture cible envisagée :

```text
NVS
├── paramètres système
├── Wi-Fi
├── zones
└── plannings

LittleFS
├── page de secours
├── diagnostic
└── configuration minimale

Carte SD
├── /web
│   ├── index.html
│   ├── app.js
│   ├── style.css
│   └── assets/
├── /backup
├── /exports
└── /logs
```

Contraintes à respecter :

- la carte SD ne doit jamais devenir indispensable au pilotage des vannes ;
- les fichiers Web doivent être servis en lecture directe sans chargement complet en RAM ;
- les mises à jour doivent utiliser un fichier temporaire puis un renommage atomique ;
- les écritures fréquentes sur SD doivent être limitées à cause des coupures secteur ;
- une vérification de taille et d’empreinte doit suivre chaque upload ;
- le câblage exact du lecteur SD et le partage SPI avec TFT et tactile doivent être validés avant développement.

## OTA — rappel pour les prochains runs

L’OTA est volontairement désactivé pendant le développement actuel.

Lorsqu’une future demande utilisateur mentionnera la remise en service de l’OTA, il faudra :

1. vérifier la taille réelle du firmware ;
2. définir une nouvelle table de partitions compatible avec deux images applicatives ;
3. vérifier la place résiduelle pour LittleFS et/ou l’interface de secours ;
4. tester un cycle OTA complet avec retour arrière en cas d’échec ;
5. documenter la procédure de récupération USB ;
6. ne pas réactiver l’OTA silencieusement dans une branche de développement ordinaire.

## Autres évolutions à conserver

- Upload des ressources Web de la carte SD depuis PlatformIO / VS Code.
- Vérification automatique après upload : taille, fichiers manquants, empreintes et doublons.
- Historique d’arrosage exportable.
- Sauvegarde et restauration ponctuelle de la configuration.
- Journalisation sur SD avec écriture tamponnée et fréquence limitée.
- Page système affichant l’état de la SD, sa capacité et les erreurs de montage.
- Interface de secours indépendante de la SD.

## Règle de maintenance

Toute nouvelle évolution validée oralement pour une version ultérieure doit être ajoutée ici avec :

- son objectif ;
- son statut ;
- ses contraintes ;
- la branche envisagée ;
- les risques connus ;
- les conditions de validation.
