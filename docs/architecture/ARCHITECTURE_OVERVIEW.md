# AquaLook — Architecture directrice du produit

## 1. Objet

Ce document est la porte d'entrée de l'architecture AquaLook. Il ne remplace pas les documents techniques existants : il les fédère et fournit une grille commune pour analyser, concevoir, tester, documenter et maintenir chaque évolution.

AquaLook doit être considéré comme un écosystème comprenant le firmware embarqué, le matériel, l'interface locale, les communications, les services distants, les outils de déploiement et les applications clientes.

## 2. Sources documentaires existantes

Les documents existants restent les sources détaillées de leur domaine :

- `AGENTS.md` : règles de travail, invariants et procédure de livraison ;
- `docs/codex/00_CONTEXT.md` : contexte du projet ;
- `docs/codex/01_ARCHITECTURE.md` : architecture logicielle embarquée actuelle ;
- `docs/codex/02_DECISIONS.md` : décisions structurantes ;
- `docs/codex/03_INVARIANTS.md` : invariants techniques et fonctionnels ;
- `docs/codex/05_BUILD_AND_TEST.md` : compilation et tests ;
- `docs/codex/06_ANTI_REGRESSION.md` : protocole anti-régression ;
- `docs/codex/08_RISKS_AND_DEBT.md` : risques et dette ;
- `docs/architecture/RELAY_TOPOLOGY.md` : topologie des relais ;
- `ROADMAP.md` : évolutions envisagées.

## 3. Les dix piliers permanents

### 3.1 CORE

Moteur d'arrosage, planification, orchestration, règles métier, pluie, capteurs, débit, équipements et sécurité fonctionnelle des sorties.

### 3.2 HARDWARE

ESP32 et coprocesseurs, écran, tactile, bus, relais, extensions I2C, capteurs, alimentation, protections électriques et contraintes physiques.

### 3.3 USER EXPERIENCE

Écran tactile, interface Web, future application Flutter, accessibilité, lisibilité, performances perçues, modes dégradés et parcours de récupération.

### 3.4 CONNECTIVITY

Wi-Fi, point d'accès, DNS captif, NTP, HTTP/HTTPS, MQTT, API, Bluetooth éventuel et protocoles inter-cartes.

### 3.5 CLOUD

VPS, broker MQTT, services de notification, stockage distant, sauvegardes, supervision, distribution OTA et services applicatifs.

### 3.6 CYBERSECURITY

Protection des identités, secrets, firmwares, communications, interfaces, infrastructures et journaux pendant tout le cycle de vie. Ce pilier est transverse à tous les autres.

Références :

- `docs/security/CYBERSECURITY_ARCHITECTURE.md` ;
- `docs/security/SECURITY_RISK_REGISTER.md` ;
- `docs/roadmap/CYBERSECURITY_LIFECYCLE.md`.

### 3.7 OBSERVABILITY

Logs, métriques, diagnostics, état mémoire, fragmentation, charge, réseau, stockage, capteurs, relais, événements de sécurité et capacité de diagnostic à distance.

### 3.8 QUALITY

Tests unitaires, tests d'intégration, bancs matériels, compilation multi-environnements, analyse statique, validation OTA, non-régression Web/LCD et qualité des dépendances.

### 3.9 DOCUMENTATION

Architecture, décisions, invariants, procédures, checkpoints, guides utilisateur et développeur, historique des versions et transmission autonome du projet.

### 3.10 EVOLUTION

Compatibilité ascendante, migrations NVS, nouveaux matériels, nouvelles interfaces, API, expérimentation, dépréciation et fin de support.

## 4. Règle d'analyse obligatoire

Toute évolution significative doit indiquer explicitement :

1. les piliers concernés ;
2. les risques introduits ;
3. les invariants à préserver ;
4. les fichiers et interfaces touchés ;
5. les tests nécessaires ;
6. la documentation à mettre à jour ;
7. le comportement en cas de panne ou d'indisponibilité ;
8. l'impact cybersécurité.

## 5. Matrice minimale de revue

| Pilier | Question minimale |
|---|---|
| CORE | Le moteur local et la sécurité des équipements restent-ils déterministes ? |
| HARDWARE | Les contraintes électriques, bus et ressources sont-elles respectées ? |
| USER EXPERIENCE | L'état, l'erreur et le mode dégradé sont-ils compréhensibles ? |
| CONNECTIVITY | Que se passe-t-il si la liaison disparaît ou devient instable ? |
| CLOUD | Le fonctionnement local reste-t-il autonome ? |
| CYBERSECURITY | Qui peut agir, avec quelle preuve, et comment révoquer l'accès ? |
| OBSERVABILITY | Le fonctionnement réel et les échecs sont-ils diagnostiquables ? |
| QUALITY | Quels tests prouvent l'absence de régression ? |
| DOCUMENTATION | La reprise du projet reste-t-elle autonome ? |
| EVOLUTION | Comment migrer, revenir en arrière ou retirer cette fonction ? |

## 6. Gouvernance documentaire

- Ne pas dupliquer une architecture détaillée déjà documentée.
- Ajouter des liens depuis cette vue vers les documents spécialisés.
- Utiliser des ADR pour les décisions irréversibles ou coûteuses à modifier.
- Mettre à jour le registre des risques lorsqu'une surface d'attaque ou une dépendance externe apparaît.
- Conserver les checkpoints comme preuves d'un état validé, pas comme remplacement de l'architecture permanente.

## 7. Invariants directeurs

- Le moteur d'arrosage et la sécurité des relais restent locaux et autonomes.
- Une dépendance cloud, réseau, mobile ou SD ne doit pas rendre le système dangereux.
- Une commande distante doit être authentifiée, autorisée, tracée et confirmée par l'état réel.
- Aucune évolution ne doit contourner les couches d'abstraction matérielle établies.
- Toute migration persistante doit être versionnée et réversible ou disposer d'un repli documenté.
- La cybersécurité est une activité continue, jamais une étape finale.