# Roadmap AquaLook

Ce document regroupe les évolutions envisagées pour AquaLook. Il ne constitue pas un engagement de développement immédiat ; chaque point devra être détaillé, priorisé et traité sur une branche dédiée.

## Évolutions futures

### Connexion à un cloud externe

Permettre au module AquaLook de se connecter de manière sécurisée à un service cloud externe afin de rendre le système accessible sans connexion directe au réseau local du module.

Objectifs fonctionnels :

- centraliser et historiser les logs techniques et fonctionnels du module ;
- consulter à distance l’état du programmateur, des zones, des cycles et des éventuelles erreurs ;
- piloter l’application et envoyer des commandes au module depuis une interface distante ;
- modifier certains paramètres ou programmes d’arrosage à distance ;
- conserver un fonctionnement local autonome si la connexion Internet ou le service cloud est indisponible ;
- synchroniser les données accumulées localement après une interruption de connexion.

Points d’architecture à étudier :

- protocole de communication sortant initié par le module, par exemple MQTT sécurisé ou HTTPS ;
- authentification forte du module et des utilisateurs ;
- chiffrement TLS des échanges ;
- gestion des droits d’accès et protection contre les commandes non autorisées ;
- file locale persistante pour les logs et commandes en attente ;
- limitation du volume et de la fréquence des remontées afin de préserver la mémoire, la bande passante et la stabilité du firmware ;
- choix entre une plateforme existante, un serveur auto-hébergé ou un service cloud dédié ;
- mécanisme de mise à jour ou de révocation des identifiants du module ;
- traçabilité des commandes distantes et confirmation de leur exécution réelle.

Invariant impératif : le cloud doit rester une extension du système. Le planificateur, la sécurité des relais et les cycles d’arrosage doivent continuer à fonctionner localement et de manière autonome en cas de perte du cloud ou d’Internet.
