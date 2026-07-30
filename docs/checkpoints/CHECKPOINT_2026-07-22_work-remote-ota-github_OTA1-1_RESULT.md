# Checkpoint AquaLook — OTA-1.1 — Résultat TLS dans le runtime complet

Date : 2026-07-22

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/remote-ota-github`
- Checkpoint précédent : `docs/checkpoints/CHECKPOINT_2026-07-22_work-remote-ota-github_OTA1.md`
- Profil de test : `ProgrammeArrosage_ota_tls_probe`
- Test strictement non destructif : aucune écriture dans `app0`, `app1` ou `otadata`

## Objet

Qualifier la capacité d’AquaLook à établir une connexion HTTPS vers GitHub après initialisation complète du runtime nominal : écran, tactile, stockage, Wi-Fi, NTP, météo, serveur Web, planificateur, relais et couches Legacy/V4.

## Résultat matériel

Le probe démarre cinq secondes après la connexion Wi-Fi, alors que le runtime complet est chargé.

Extraits significatifs :

```text
OTA-1.1: memory stage=before-client heapFree=72000 heapMin=42392 ...
OTA-1.1: memory stage=before-connect heapFree=69784 heapMin=42392 ...
SSL - Memory allocation failed (-32512)
OTA-1.1: tls connected=no durationMs=150
OTA-1.1: result=failed phase=tls error=-32512 detail=SSL - Memory allocation failed
OTA-1.1: memory stage=after-failure heapFree=68940 heapMin=42392 ...
```

Le plus grand bloc contigu est tronqué dans la copie du journal, mais les diagnostics antérieurs dans le runtime complet l’établissaient autour de 39 KiB. Le test isolé OTA-1 disposait d’un bloc contigu de 110 580 octets et réussissait TLS.

## Comparaison OTA-1 / OTA-1.1

| Condition | Heap avant TLS | Plus grand bloc connu | Résultat TLS |
|---|---:|---:|---|
| Firmware de banc isolé | 237 860 octets | 110 580 octets | SUCCÈS |
| Runtime AquaLook complet | 69 784 octets | environ 39 KiB précédemment observés | ÉCHEC `-32512` |

Conclusion :

- DNS et TCP ne sont pas en cause ;
- GitHub et la pile TLS fonctionnent sur ce matériel ;
- la table de partitions et la taille des firmwares sont compatibles OTA ;
- l’échec est causé par l’état mémoire du runtime complet, principalement la fragmentation et l’absence d’un bloc contigu suffisant pour initialiser TLS.

## Invariant anti-régression

Ne pas réintroduire la tentative précédente consistant à libérer les sprites graphiques au moment de la connexion TLS.

Cette stratégie a déjà provoqué une régression tactile et portail captif. Elle est interdite comme solution OTA.

## Décision d’architecture D-OTA1.1-1

Le téléchargement OTA ne sera pas exécuté dans le runtime AquaLook complet.

La solution retenue pour la suite est un **mode maintenance OTA minimal au démarrage** :

1. le runtime complet programme ou détecte une demande de maintenance OTA ;
2. l’intention est persistée en NVS ;
3. le module redémarre de manière contrôlée hors cycle d’arrosage ;
4. au boot, avant TFT, tactile, serveur Web, météo, stockage lourd et runtime équipements, le firmware entre dans un chemin minimal ;
5. ce chemin initialise uniquement : journal minimal, NVS, Wi-Fi, TLS, manifeste et moteur OTA ;
6. le manifeste GitHub est vérifié ;
7. le firmware est téléchargé en flux vers la partition inactive ;
8. l’intégrité et la compatibilité sont validées ;
9. la partition de boot est basculée seulement après validation complète ;
10. le module redémarre sur la nouvelle image ;
11. en absence de mise à jour ou en cas d’échec, l’intention NVS est effacée et le runtime nominal démarre normalement.

## Pourquoi cette architecture est adaptée

Au démarrage OTA-0.1, avant chargement du runtime complet, les mesures étaient :

- heap libre : 265 080 octets ;
- plus grand bloc contigu : 110 580 octets.

Le test OTA-1 isolé a démontré qu’une connexion TLS vers GitHub consomme temporairement environ 41,5 KiB et réussit avec cette configuration mémoire.

Le mode maintenance exploite donc un état mémoire déjà validé sans démonter le boîtier, sans arrêter manuellement les sprites et sans faire dépendre l’OTA de la fragmentation accumulée par le runtime principal.

## Déclenchement distant

Le déclenchement instantané depuis Internet reste à définir, car le runtime complet ne peut pas ouvrir une connexion TLS fiable.

Deux niveaux sont retenus :

### Niveau initial robuste

- vérification GitHub pendant une fenêtre de maintenance planifiée ;
- redémarrage contrôlé à une heure configurable, uniquement si aucune zone n’arrose ;
- fréquence faible, par exemple quotidienne ou hebdomadaire ;
- téléchargement uniquement si une version compatible est disponible.

Cette méthode permet déjà une mise à jour entièrement distante après publication d’une GitHub Release, sans présence locale et sans accès au même réseau.

### Niveau ultérieur à valider

- signal léger via passerelle locale, ESP32-S2, MQTT ou autre mécanisme ;
- ce signal ne transporte pas le firmware et ne remplace pas la validation GitHub ;
- il sert uniquement à demander le prochain redémarrage en maintenance OTA.

Aucun port entrant ne doit être ouvert sur la box.

## Sécurité

Le probe actuel utilise `setInsecure()` uniquement pour qualifier le transport.

Le mode OTA réel devra obligatoirement utiliser au minimum :

- validation du certificat ou de l’autorité de certification ;
- manifeste versionné ;
- contrôle du modèle matériel ;
- contrôle de la taille ;
- SHA-256 du firmware ;
- idéalement signature numérique du manifeste ou du firmware ;
- refus de toute version incompatible ou plus ancienne sauf procédure de récupération explicite.

## Contraintes d’exploitation

- interdiction de redémarrer pendant un arrosage actif ;
- conservation NVS, programmes et configuration ;
- journalisation du motif d’entrée en maintenance ;
- timeout total du mode maintenance ;
- retour automatique au runtime nominal si Wi-Fi, DNS, TLS, manifeste ou téléchargement échoue ;
- aucun blocage prolongé du programmateur en attente d’Internet ;
- rollback du nouveau firmware à traiter avant mise en production.

## État de validation

| Critère | État |
|---|---|
| HTTPS GitHub en firmware isolé | VALIDÉ |
| HTTPS GitHub dans runtime complet | ÉCHEC CONFIRMÉ — mémoire fragmentée |
| Écriture OTA | NON TESTÉE |
| Modification partition active | AUCUNE |
| Architecture de contournement | DÉCIDÉE — maintenance minimale au boot |
| Déclenchement distant initial | FENÊTRE DE MAINTENANCE PLANIFIÉE |
| Sécurité certificat / signature | À IMPLÉMENTER |
| Rollback | À IMPLÉMENTER |

## Suite autorisée

La prochaine étape est `OTA-2` : définir puis implémenter le squelette du mode maintenance OTA sans téléchargement de firmware.

Première livraison attendue :

1. marqueur NVS de demande de maintenance ;
2. détection très tôt au boot ;
3. chemin minimal séparé du setup nominal ;
4. connexion Wi-Fi et probe HTTPS sécurisé ou encore temporairement instrumenté ;
5. timeout et retour au runtime nominal ;
6. aucune écriture dans `app1` ;
7. aucune bascule de partition.

Le firmware de test `ProgrammeArrosage_ota_tls_probe` doit être remplacé par le profil nominal après les relevés matériels.
