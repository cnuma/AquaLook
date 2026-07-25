# AquaLook — Pilier Observabilité

## Objectif

Rendre l'état réel d'AquaLook compréhensible sans perturber le moteur d'arrosage ni masquer les modes dégradés.

## Périmètre

L'observabilité couvre :

- journaux système, arrosage, relais, stockage, réseau, OTA, MQTT et sécurité ;
- métriques mémoire, fragmentation, uptime, redémarrages, RSSI, latences et charge des services ;
- santé des périphériques, cartes relais, SD, horloge, capteurs et communications ;
- traçabilité des commandes locales et distantes ;
- diagnostic accessible localement même sans Internet ni carte SD.

## Principes

1. L'observation ne doit jamais devenir une dépendance du moteur métier.
2. Les collectes et écritures doivent être bornées et non bloquantes.
3. Les événements critiques doivent rester disponibles après redémarrage lorsque cela est pertinent.
4. Les données sensibles ne doivent pas apparaître en clair dans les logs.
5. Chaque métrique doit avoir une unité, une source, une fréquence et une règle de rétention explicites.
6. Un état inconnu ou trop ancien doit être signalé comme tel.

## Niveaux de données

- **Événement** : changement ou incident horodaté.
- **Métrique** : valeur numérique échantillonnée.
- **État de santé** : synthèse `OK`, `DEGRADED`, `FAULT` ou `UNKNOWN`.
- **Trace d'action** : origine, demande, décision, exécution et résultat.

## Minimum embarqué

Le firmware doit pouvoir exposer au minimum :

- identité du build et uptime ;
- cause du dernier redémarrage ;
- heap libre et plus grand bloc contigu ;
- état Wi-Fi, RSSI et synchronisation temporelle ;
- état SD/LittleFS/NVS ;
- état des relais et cycle actif ;
- erreurs récentes ;
- compteur d'incidents critiques.

## Sécurité et confidentialité

Ne jamais journaliser : mots de passe, clés privées, jetons complets, certificats privés ou contenus permettant de réutiliser une session. Les identifiants techniques doivent être tronqués ou pseudonymisés si leur exposition n'est pas nécessaire.

## Validation

Toute évolution de l'observabilité doit vérifier :

- absence de blocage du planificateur ;
- consommation mémoire bornée ;
- rotation ou limitation des journaux ;
- comportement sans réseau et sans SD ;
- cohérence entre événement rapporté et effet matériel réel ;
- absence de secret dans les sorties.

## Roadmap

1. consolider les événements existants ;
2. définir un modèle commun de santé ;
3. exposer une page locale de diagnostic ;
4. persister les incidents critiques ;
5. publier un sous-ensemble sécurisé via MQTT ;
6. centraliser sur VPS sans rendre le cloud nécessaire au fonctionnement local.
