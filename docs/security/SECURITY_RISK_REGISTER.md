# AquaLook — Registre des risques cybersécurité

## 1. Usage

Ce registre est vivant. Il est révisé lors de chaque évolution touchant le réseau, l'OTA, le cloud, MQTT, l'application mobile, la persistance, les secrets ou les commandes d'équipements, **à chaque faille publiée qui concerne une technologie du projet, et au moins chaque trimestre** (`CYBERSECURITY_ARCHITECTURE.md` §1.1). Un risque « réduit » ou « fermé » peut être rouvert si une nouvelle technique d'attaque rend sa mesure insuffisante.

Échelle :

- probabilité : faible, moyenne, élevée ;
- impact : faible, important, critique ;
- état : ouvert, réduit, accepté, transféré, fermé.

## 2. Registre courant

| ID | Risque | Surface | Probabilité | Impact | Mesures actuelles ou prévues | Contrat / preuve | État |
|---|---|---|---|---|---|---|---|
| SEC-001 | Accès administratif local non authentifié fortement | Interface Web locale | Élevée | Critique | Session Web par défi-réponse HMAC, cookie `HttpOnly; SameSite=Strict`, toute écriture sous session, contrôle `Origin`/`Sec-Fetch-Site`, anti « DNS rebinding » (D016 lot F) ; reste : HTTP en clair sur le LAN (P0.5) | lot F validé sur `.141` le 10 oct. 2026 (`ENROLEMENT_ET_SECURITE_LOCALE.md` §8) | Réduit |
| SEC-002 | Mot de passe Wi-Fi ou AP partagé, prévisible ou absent | Wi-Fi / portail captif | Élevée | Important | Identité propre par appareil, AP protégé et temporaire | test `expectedFailure` AP protégé | Ouvert |
| SEC-003 | Commande distante MQTT forgée ou rejouée | MQTT | Moyenne | Critique | TLS, ACL, identité individuelle, expiration, identifiant de commande, anti-rejeu | non implémenté | Ouvert |
| SEC-004 | Firmware OTA modifié ou source non autorisée | OTA | Moyenne | Critique | Signature, contrôle d'intégrité, provenance et rollback | manifeste Web signé (D018) ; manifeste firmware pas encore (P1.4, P1.9) | Ouvert |
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
| SEC-015 | Carte SD modifiée injectant des ressources Web malveillantes | Stockage SD | Moyenne | Important | Manifeste, intégrité, ressources critiques en stockage sûr | traversée de chemin et `/api/` testées ; mises à jour réseau signées (D018), mais la carte elle-même et le dépôt direct ne le sont pas (P1.8) | Réduit |
| SEC-016 | Compromission d'un microcontrôleur d'extension | I2C/UART / Lolin S2 | Faible à moyenne | Important | Validation des trames, autorité limitée | architecture seulement | Ouvert |
| SEC-017 | Absence de révocation après compromission | Exploitation | Moyenne | Critique | Inventaire, révocation et reprovisionnement | procédure documentaire | Ouvert |
| SEC-018 | Sauvegarde inutilisable lors d'un incident | VPS / configuration | Moyenne | Important | Sauvegardes protégées et tests de restauration | exercice non archivé | Ouvert |
| SEC-019 | API météo interceptée ou clé OWM exposée | HTTP sortant / météo | Élevée | Important | HTTPS, minimisation des logs et rotation de clé | test `expectedFailure` HTTPS OWM | Ouvert |
| SEC-020 | Script injecté (XSS) exécuté dans la console, l'espace utilisateur ou l'éditeur en ligne | Service en ligne | Moyenne | Critique | CSP `script-src 'self'` sans `'unsafe-inline'`, aucun script inline (P0.4b, P0.4d, P0.4e) | banc CSP Edge (CDP), témoin négatif ; `script-src 'self'` vérifié en ligne le 12 oct. 2026 sur console, espace utilisateur et éditeur | Réduit |
| SEC-021 | Mot de passe SMTP lu depuis une sauvegarde ou une fuite de la base | Service en ligne / base | Moyenne | Important | Chiffrement XChaCha20-Poly1305, clé `SETTINGS_KEY` hors base (P0.4c) ; reste : base **et** `.env` ensemble le livrent | banc PHP + MariaDB, 25 contrôles ; en production le 12 oct. 2026 (« défini (chiffré) ») | Réduit |
| SEC-022 | Console d'administration chargée et étudiée par un inconnu ou une IA externe | Service en ligne | Élevée | Important | Authentification HTTP Basic avant la page, jeton admin ensuite, `noindex` (P0.4e) | banc PHP + Edge : 401 / 200 ; en production le 12 oct. 2026 (401 sans identifiant) | Réduit |
| SEC-023 | Publication partielle ou divergente du service (fichier modifié à la main, `.htaccess` fautive → 500) | Service en ligne / exploitation | Moyenne | Important | Publication depuis Git (`deploy_cloud.py`), ordre imposé, relecture, contrôle d'en-têtes juste après | outil testé sur banc FTP ; identifiants à fournir (P0.4a) | Ouvert |
| SEC-024 | Paquet de pages Web publié par un tiers (accès FTP ou hébergeur compromis, changement d'hébergeur) | Mise à jour Web / hébergement | Moyenne | Critique | Manifeste signé (ECDSA P-256), clé privée hors ligne, clé publique dans le firmware, refus sans repli (D018) ; anti-rejeu : version antérieure à l'installée refusée (P1.9 lot 2) | `.141` (5.14.0, 13 oct. 2026) : signé accepté, falsifié refusé (`signature-invalide`), non signé refusé (`signature-absente`), pages intactes ; `.141` (5.15.3 build 1499, 13 oct. 2026) : manifeste signé 5.15.1 rejoué sur 5.15.2 installée → `version-anterieure`, 0/27, pages intactes | Réduit |
| SEC-025 | Clé privée de signature volée ou perdue | Poste du propriétaire | Faible | Critique | Hors dépôt et hors OneDrive, droits restreints, sauvegarde hors ligne ; plusieurs clés publiques acceptées pour la rotation et la révocation par firmware | clé créée le 13 oct. 2026 ; sauvegarde hors ligne faite par le propriétaire (confirmée le 14 oct. 2026) | Réduit |
| SEC-027 | Écriture arbitraire de pages sur la SD par les routes de développement `/api/debug/*` (dépôt direct, catalogue), sous session | Module / API locale | Moyenne | Critique | Routes absentes du binaire de production (D020, `AQUALOOK_DEV_ROUTES=0` par défaut) ; ouvertes seulement dans `ProgrammeArrosage_s3_banc` ; `build.devRoutes` dans `/api/diagnostics` | 404 sur `/api/debug/*` avec le binaire de production, vérifié sur `.141` | réduit |
| SEC-026 | Module annoncé sur le réseau local par mDNS (nom, service http) | Réseau local | Moyenne | Faible | Nom choisi par l'utilisateur ou générique, aucun secret annoncé ; écritures toujours sous session (D016) ; filtre d'hôte inchangé (.local déjà admis) | mDNS actif depuis 5.15.1 (13 oct. 2026) | Accepté |

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
