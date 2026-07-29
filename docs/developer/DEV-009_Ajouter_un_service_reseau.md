# AquaLook Developer Guide — Ajouter un service réseau

- Référence : DEV-009
- Statut : actif
- Maturité : D4

## Principe

Un service réseau est optionnel pour l’arrosage local. Il doit disposer de timeouts, d’un état observable et d’un comportement hors ligne explicite.

## Étapes

1. définir protocole, exposition et données sensibles ;
2. injecter la dépendance Wi-Fi sans bloquer le boot ;
3. créer une machine d’états ou une tâche bornée ;
4. définir timeouts, retries et backoff ;
5. limiter mémoire, sockets, taille des réponses et fréquence ;
6. interdire les secrets dans URL, logs et diagnostics ;
7. prévoir TLS et validation de certificat lorsque requis ;
8. publier état et défauts ;
9. ajouter tests hors ligne, timeout et réponse invalide ;
10. mesurer heap et temps d’exécution sur cible.

## Règles

- aucune boucle d’attente infinie ;
- aucune dégradation des sécurités locales ;
- aucune reconnexion agressive permanente ;
- fermeture propre des connexions ;
- échec réseau sans activation matérielle imprévue.

## Références

- `docs/firmware/FW-009_WiFiManager.md`
- `docs/engineering/18_NETWORK_AND_WIFI.md`
- `docs/engineering/23_SECURITY_OPERATIONS.md`
