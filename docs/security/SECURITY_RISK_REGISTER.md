# AquaLook — Registre des risques cybersécurité

## 1. Usage

Ce registre est vivant. Il est révisé lors de chaque évolution touchant le réseau, l'OTA, le cloud, MQTT, l'application mobile, la persistance, les secrets ou les commandes d'équipements.

Échelle :

- probabilité : faible, moyenne, élevée ;
- impact : faible, important, critique ;
- état : ouvert, réduit, accepté, transféré, fermé.

## 2. Registre courant

| ID | Risque | Surface | Probabilité | Impact | Mesures actuelles ou prévues | Contrat / preuve | État |
|---|---|---|---|---|---|---|---|
| SEC-001 | Accès administratif local non authentifié fortement | Interface Web locale | Élevée | Critique | Authentification serveur, sessions, autorisations, CSRF | aucune preuve fonctionnelle ; architecture `19_HTTPS_AND_SESSIONS.md` | Ouvert |
| SEC-002 | Mot de passe Wi-Fi ou AP partagé, prévisible ou absent | Wi-Fi / portail captif | Élevée | Important | Identité propre par appareil, AP protégé et temporaire | test `expectedFailure` AP protégé | Ouvert |
| SEC-003 | Commande distante MQTT forgée ou rejouée | MQTT | Moyenne | Critique | TLS, ACL, identité individuelle, expiration, identifiant de commande, anti-rejeu | non implémenté | Ouvert |
| SEC-004 | Firmware OTA modifié ou source non autorisée | OTA | Moyenne | Critique | Signature, contrôle d'intégrité, provenance et rollback | non implémenté | Ouvert |
| SEC-005 | Retour vers une version connue vulnérable | OTA | Faible à moyenne | Important | Version minimale et repli contrôlé | non implémenté | Ouvert |
| SEC-006 | Secret présent dans Git, checkpoint, archive, URL ou log | Chaîne de développement | Élevée | Critique | Scan, exclusions, rotation et revue des artefacts | test `expectedFailure` mot de passe Wi-Fi ; revue clé OWM | Ouvert |
| SEC-007 | Clé ou certificat identique sur tous les appareils | Provisionnement | Moyenne | Critique | Identité et certificat individuels, révocation unitaire | non implémenté | Ouvert |
| SEC-008 | Compromission VPS par service ou port inutile | VPS | Moyenne | Critique | Pare-feu, SSH par clé, mises à jour, comptes séparés, supervision | infrastructure non déployée | Ouvert |
| SEC-009 | Dépendance vulnérable non suivie | PlatformIO, Arduino, Flutter, MQTT, Linux | Élevée | Important | Inventaire, versions maîtrisées, alertes CVE | versions fixées dans `platformio.ini` | Ouvert |
| SEC-010 | Déni de service épuisant heap, sockets ou stockage | ESP32 / réseau | Moyenne | Important | Limites, timeouts, quotas, watchdog, mode dégradé | timeout météo contrôlé ; autres charges non testées | Réduit |
| SEC-011 | Journaux contenant des secrets ou données sensibles | Logs locaux et distants | Élevée | Important | Filtrage, masquage, rétention et accès restreint | test `expectedFailure` mot de passe Wi-Fi | Ouvert |
| SEC-012 | Fonction de récupération utilisée comme porte dérobée | Portail captif / recovery | Élevée | Critique | Activation temporaire, signalisation, authentification, journalisation | AP actuellement ouvert ; contrat explicite | Ouvert |
| SEC-013 | Action appliquée au mauvais module | Application mobile / multi-site | Moyenne | Critique | Identité visible, confirmation du module, accusé d'état réel | non implémenté | Ouvert |
| SEC-014 | Horloge incorrecte rendant inefficaces expiration et certificats | NTP / RTC | Moyenne | Important | Politique sans heure fiable, synchronisation et diagnostic | EventLog relatif ; sessions non implémentées | Ouvert |
| SEC-015 | Carte SD modifiée injectant des ressources Web malveillantes | Stockage SD | Moyenne | Important | Manifeste, intégrité, ressources critiques en stockage sûr | traversée de chemin et `/api/` testées ; manifeste absent | Réduit |
| SEC-016 | Compromission d'un microcontrôleur d'extension | I2C/UART / Lolin S2 | Faible à moyenne | Important | Validation des trames, autorité limitée | architecture seulement | Ouvert |
| SEC-017 | Absence de révocation après compromission | Exploitation | Moyenne | Critique | Inventaire, révocation et reprovisionnement | procédure documentaire | Ouvert |
| SEC-018 | Sauvegarde inutilisable lors d'un incident | VPS / configuration | Moyenne | Important | Sauvegardes protégées et tests de restauration | exercice non archivé | Ouvert |
| SEC-019 | API météo interceptée ou clé OWM exposée | HTTP sortant / météo | Élevée | Important | HTTPS, minimisation des logs et rotation de clé | test `expectedFailure` HTTPS OWM | Ouvert |

## 3. Contrats exécutables

La commande de référence est :

```powershell
python -m unittest discover -s tests/contracts -p "test_*.py" -v
```

Les tests `expectedFailure` maintiennent les risques confirmés visibles sans simuler leur correction. Ils doivent devenir des tests ordinaires dans le commit de correction.

## 4. Informations obligatoires pour un nouveau risque

- identifiant unique ;
- scénario et actif concerné ;
- surface d'attaque ;
- préconditions ;
- probabilité et impact ;
- propriétaire du traitement ;
- mesures préventives ;
- détection ;
- réponse et récupération ;
- risque résiduel ;
- contrat ou preuve associée ;
- date ou événement de révision.

## 5. Priorités

1. supprimer le mot de passe Wi-Fi des logs ;
2. protéger et borner dans le temps le portail captif ;
3. migrer OpenWeatherMap vers HTTPS ;
4. remplacer le verrouillage Web visuel par une authentification réelle ;
5. sécuriser OTA et MQTT avant exposition distante ;
6. mettre en place le suivi des dépendances et vulnérabilités ;
7. tester révocation et restauration.

## 6. Règle de fermeture

Un risque ne peut être déclaré fermé que si la mesure est :

- implémentée ;
- testée nominalement et négativement ;
- documentée ;
- observable en exploitation ;
- accompagnée d'une récupération lorsque nécessaire ;
- référencée par un checkpoint ;
- sans `expectedFailure` résiduel correspondant.
