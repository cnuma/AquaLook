# AquaLook Engineering Reference — HTTPS et sessions

- Version documentaire : 0.8
- Statut : architecture cible et état réel audité
- Dernière consolidation : 2026-07-27
- Sources : code Web/Wi-Fi/météo, architecture cybersécurité, contrats CI
- Composants : WebManager, authentification, sessions, TLS
- Maturité : D3

## Objet

Ce document distingue le service HTTP réellement présent de la cible HTTPS et sessions. Aucun mécanisme futur n’est présenté comme implémenté.

## État actuel confirmé

- serveur Web local HTTP sur le port 80 ;
- aucune authentification forte côté serveur ;
- verrouillage administrateur principalement visuel côté navigateur ;
- routes sensibles accessibles à un client HTTP direct sur le réseau local ;
- portail captif HTTP ;
- météo OpenWeatherMap encore appelée en HTTP ;
- aucune session serveur, expiration, révocation ou protection CSRF.

Le verrouillage visuel ne constitue pas une barrière de sécurité.

## Écarts prioritaires exécutables

Les contrats `tests/contracts/test_security_contracts.py` suivent trois écarts sous forme de tests `expectedFailure` :

- `handleSetWifi()` journalise le mot de passe Wi-Fi en clair sur la sortie série ;
- le point d’accès `Arrosage-Setup` est créé sans mot de passe ;
- OpenWeatherMap est appelé en HTTP non chiffré.

La CI affiche ces écarts à chaque modification concernée. Leur correction exige de supprimer l’annotation `expectedFailure` correspondante afin de rendre le contrat bloquant.

## Architecture cible

La cible prévoit :

- authentification réelle côté serveur avant toute action sensible ;
- session aléatoire, expirante et révocable ;
- autorisation distincte de l’authentification ;
- protection CSRF des requêtes modifiant l’état ;
- HTTPS lorsque la mémoire, les certificats et les modes dégradés sont validés ;
- aucun secret complet dans les logs, URL de diagnostic ou artefacts.

## Exigences de session

- identifiant imprévisible ;
- durée de vie bornée ;
- invalidation lors du changement de secret ou d’identité ;
- nombre de sessions limité ;
- aucune session dans une URL ;
- cookie `Secure`, `HttpOnly` et `SameSite` si un cookie est retenu ;
- refus en sécurité lorsque l’heure ou le stockage requis est indisponible.

## TLS et ESP32

Le passage HTTPS doit mesurer :

- heap libre et plus grand bloc contigu avant/après négociation ;
- nombre de connexions simultanées ;
- temps de réponse ;
- comportement pendant un arrosage actif ;
- rotation et expiration des certificats ;
- reprise après échec TLS.

## Modes dégradés

- HTTPS indisponible : aucune ouverture automatique d’un accès distant non protégé ;
- heure absolue indisponible : politique restrictive pour certificats et expirations ;
- stockage de session indisponible : refus des actions administratives ;
- réseau absent : Scheduler, relais et sécurités locales restent opérationnels.

## Risques associés

- `SEC-001` : administration locale sans authentification forte ;
- `SEC-002` : point d’accès de récupération non protégé ;
- `SEC-006` et `SEC-011` : secrets dans les sources, URL ou journaux ;
- `SEC-010` : épuisement heap/sockets ;
- `SEC-014` : horloge incorrecte.

## Invariants

- `INV-HTTPS-001` : aucune action sensible n’est protégée uniquement dans le navigateur.
- `INV-HTTPS-002` : une session est expirante et révocable.
- `INV-HTTPS-003` : aucun secret ou jeton complet n’est journalisé.
- `INV-HTTPS-004` : TLS ne compromet pas les sécurités locales ni le Runtime.
- `INV-HTTPS-005` : un écart de sécurité confirmé reste visible dans les contrats jusqu’à correction et test.

## Validation pour D4/D5

D4 exige l’inventaire des routes, menaces, écarts et contrats automatisables. D5 exige en plus : authentification réelle, tests positifs/négatifs, expiration/révocation, mesures mémoire TLS, tests de charge et preuve archivée sur cible.

## Références

- `09_WEB_AND_HTTP_INTERFACES.md` ;
- `23_SECURITY_OPERATIONS.md` ;
- `37_SECURITY_CONTRACTS_AND_CI.md` ;
- `docs/security/CYBERSECURITY_ARCHITECTURE.md` ;
- `docs/security/SECURITY_RISK_REGISTER.md`.

## Historique

### 0.8

Ajout de l’état audité, des écarts exécutables et des critères D4/D5.
