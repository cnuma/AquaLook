# AquaLook — Registre des risques cybersécurité

## 1. Usage

Ce registre est vivant. Il doit être révisé lors de chaque évolution touchant le réseau, l'OTA, le cloud, MQTT, l'application mobile, la persistance, les secrets ou les commandes d'équipements.

Échelle proposée :

- probabilité : faible, moyenne, élevée ;
- impact : faible, important, critique ;
- état : ouvert, réduit, accepté, transféré, fermé.

## 2. Registre initial

| ID | Risque | Surface | Probabilité | Impact | Mesures actuelles ou prévues | État |
|---|---|---|---|---|---|---|
| SEC-001 | Accès administratif local non authentifié fortement | Interface Web locale | Élevée | Critique | Remplacer le verrouillage visuel par une authentification réelle, sessions et autorisations | Ouvert |
| SEC-002 | Mot de passe Wi-Fi ou AP partagé ou prévisible | Wi-Fi / portail captif | Moyenne | Important | Identifiant propre par appareil, pas de secret universel, procédure de changement | Ouvert |
| SEC-003 | Commande distante MQTT forgée ou rejouée | MQTT | Moyenne | Critique | TLS, ACL, identité individuelle, expiration, identifiant de commande, anti-rejeu | Ouvert |
| SEC-004 | Firmware OTA modifié ou provenant d'une source non autorisée | OTA | Moyenne | Critique | Signature du firmware, contrôle d'intégrité, provenance et rollback | Ouvert |
| SEC-005 | Retour vers une version connue vulnérable | OTA | Faible à moyenne | Important | Politique de version minimale et procédure de secours contrôlée | Ouvert |
| SEC-006 | Secret présent dans Git, un checkpoint, une archive ou un log | Chaîne de développement | Moyenne | Critique | Exclusions, scan de secrets, rotation immédiate et revue des archives | Ouvert |
| SEC-007 | Clé ou certificat identique sur tous les appareils | Provisionnement | Moyenne | Critique | Identité et certificat individuels, révocation unitaire | Ouvert |
| SEC-008 | Compromission du VPS par service ou port inutile | VPS | Moyenne | Critique | Pare-feu restrictif, SSH par clé, mises à jour, comptes séparés, supervision | Ouvert |
| SEC-009 | Dépendance vulnérable non suivie | PlatformIO, Arduino, Flutter, MQTT, Linux | Élevée | Important | Inventaire, versions maîtrisées, alertes CVE et revues périodiques | Ouvert |
| SEC-010 | Déni de service épuisant heap, sockets ou stockage | ESP32 / réseau | Moyenne | Important | Limites de taille et fréquence, timeouts, quotas, watchdog, mode dégradé | Ouvert |
| SEC-011 | Journaux contenant des secrets ou données trop sensibles | Logs locaux et distants | Moyenne | Important | Filtrage, masquage, politique de rétention et accès restreint | Ouvert |
| SEC-012 | Utilisation d'une fonction de récupération comme porte dérobée | Portail captif / recovery | Moyenne | Critique | Activation explicite et temporaire, signalisation, authentification, journalisation | Ouvert |
| SEC-013 | Action appliquée au mauvais module | Application mobile / multi-site | Moyenne | Critique | Identité visible, confirmation du module, accusé d'état réel | Ouvert |
| SEC-014 | Horloge incorrecte rendant inefficaces expiration et certificats | NTP / RTC | Moyenne | Important | Politique explicite sans heure fiable, synchronisation et diagnostic | Ouvert |
| SEC-015 | Carte SD modifiée injectant des ressources Web malveillantes | Stockage SD | Moyenne | Important | Manifestes/version, contrôle d'intégrité, ressources critiques conservées en stockage sûr | Ouvert |
| SEC-016 | Compromission d'un microcontrôleur d'extension | I2C/UART / Lolin S2 | Faible à moyenne | Important | Validation des trames, autorité limitée, aucune activation directe non contrôlée | Ouvert |
| SEC-017 | Absence de procédure de révocation après compromission | Exploitation | Moyenne | Critique | Inventaire des identités, mécanisme de révocation et reprovisionnement | Ouvert |
| SEC-018 | Sauvegarde inutilisable lors d'un incident | VPS / configuration | Moyenne | Important | Sauvegardes chiffrées si nécessaire et tests réguliers de restauration | Ouvert |

## 3. Informations obligatoires pour un nouveau risque

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
- date ou événement de révision.

## 4. Priorités initiales

1. remplacer le verrouillage Web visuel par une authentification réelle ;
2. définir l'identité propre de chaque appareil ;
3. sécuriser l'architecture OTA avant exposition distante ;
4. définir MQTT avec TLS, ACL et anti-rejeu avant l'application Flutter ;
5. sortir tous les secrets des sources et archives ;
6. définir le durcissement minimal du futur VPS ;
7. mettre en place le suivi des dépendances et vulnérabilités.

## 5. Règle de fermeture

Un risque ne peut être déclaré fermé que si la mesure est :

- implémentée ;
- testée, y compris en cas d'échec ;
- documentée ;
- observable en exploitation ;
- accompagnée d'une procédure de récupération lorsque nécessaire.