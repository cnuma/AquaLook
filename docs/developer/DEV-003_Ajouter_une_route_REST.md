# AquaLook Developer Guide — Ajouter une route REST

- Référence : DEV-003
- Statut : actif
- Maturité : D4

## Principe

Une route HTTP est un contrat. Son URL, sa méthode, son schéma JSON, ses erreurs et son effet doivent être documentés et testés.

## Étapes

1. vérifier qu’une route existante ne couvre pas le besoin ;
2. choisir GET pour une lecture sans effet et POST pour une modification ;
3. déclarer la route dans `WebManager::setupRoutes()` ;
4. utiliser `POST_JSON`/`addJsonHandler()` pour les corps JSON ;
5. valider présence, type, longueur et bornes de chaque champ ;
6. répondre avant toute opération longue ou redémarrage ;
7. différer les écritures LittleFS/NVS hors callback AsyncTCP lorsque nécessaire ;
8. protéger l’action selon la politique d’authentification lorsqu’elle sera disponible ;
9. ajouter tests positifs, négatifs et dégradés ;
10. mettre à jour `09_WEB_AND_HTTP_INTERFACES.md`, contrats et checkpoint.

## Réponses

- succès : JSON cohérent et code HTTP adapté ;
- entrée invalide : refus explicite sans effet partiel ;
- dépendance indisponible : erreur contrôlée ;
- route inconnue hors portail : 404 ;
- aucune donnée sensible dans le corps d’erreur ou les logs.

## Sécurité

- aucune action administrative protégée uniquement côté navigateur ;
- taille des corps bornée ;
- pas de secret dans URL, logs ou diagnostics ;
- chemins `/api/` exclus du handler SD ;
- cache désactivé pour les réponses sensibles ;
- préparer les tests CSRF et session pour les futures routes authentifiées.

## Validation minimale

- méthode incorrecte ;
- JSON absent ou mal formé ;
- champs manquants ;
- valeurs hors bornes ;
- répétition rapide ;
- service dépendant absent ;
- vérification de l’effet réel après réponse et après reboot si persistant.

## Références

- `docs/engineering/09_WEB_AND_HTTP_INTERFACES.md`
- `docs/engineering/19_HTTPS_AND_SESSIONS.md`
- `docs/engineering/37_SECURITY_CONTRACTS_AND_CI.md`
