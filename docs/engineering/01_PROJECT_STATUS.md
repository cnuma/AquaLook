# AquaLook — Situation du projet

## 1. Finalité

AquaLook est un contrôleur d'arrosage embarqué sur ESP32, conçu pour rester autonome localement tout en évoluant vers un écosystème comprenant interface Web, écran tactile, stockage SD, mise à jour distante, notifications, MQTT, application Flutter et services sur VPS.

Le produit doit continuer à exécuter les programmes d'arrosage et les sécurités matérielles même lorsque le Wi-Fi, Internet, le cloud, la carte SD ou un service distant sont indisponibles.

## 2. Plateforme matérielle de référence

- carte : ESP32-2432S028, dite CYD 2,8 pouces ;
- écran : ILI9341, 320 × 240, rotation 1 ;
- tactile résistif : XPT2046 sur bus SPI séparé ;
- PSRAM : absente ;
- relais actuel : carte basée sur XL9535-K2V5, adresse I2C 0x20 ;
- I2C principal : SDA 27, SCL 22 ;
- stockage additionnel : lecteur microSD de la CYD ;
- extensions envisagées : MCP23017, relais unitaires, débitmètres et Lolin S2 Mini.

## 3. Capacités validées

Les éléments suivants sont considérés comme acquis au regard des checkpoints disponibles :

- fonctionnement local du moteur d'arrosage ;
- configuration de 1 à 8 zones ;
- persistance active en NVS ;
- écran LCD et tactile opérationnels ;
- interface Web locale et portail captif ;
- planification jours fixes et intervalle ;
- intégration météo avec comportement de repli ;
- abstraction progressive des relais et topologie multi-cartes ;
- architecture V4 avec orchestrateur et runtime shadow développée par étapes ;
- migration progressive des ressources Web vers SD avec mode de secours ;
- partitions OTA doubles validées sur matériel réel ;
- récupération automatique de la carte SD et persistance des incidents ;
- journalisation enrichie et configuration ntfy persistée.

## 4. État actuel des chantiers majeurs

### 4.1 Cœur fonctionnel

Le moteur local reste la fonction critique. Les décisions de planification ne doivent jamais dépendre d'un service distant. Le planificateur ne commande pas directement les sorties : la commande passe par les couches d'abstraction et de sécurité prévues.

### 4.2 Relais et équipements

Le projet évolue d'un modèle simple « zone vers numéro de relais » vers une topologie explicite : zone logique, affectation, carte physique, contrôleur et voie. La compatibilité avec la carte unique actuelle doit rester le comportement par défaut.

### 4.3 Stockage SD

La SD sert à déplacer les ressources Web volumineuses et à libérer la flash. Le démarrage, le portail captif et le diagnostic minimal doivent rester possibles sans SD. La récupération automatique après incident est validée, mais les effets de concurrence SPI, de retrait à chaud et de corruption doivent continuer à être surveillés.

### 4.4 OTA

Les partitions doubles sont validées. L'architecture distante s'appuie sur GitHub comme source de firmware. Les travaux futurs doivent intégrer signature, contrôle d'intégrité, rollback sûr, journalisation et politique de version.

### 4.5 Notifications

Le diagnostic ntfy a établi que DNS et TCP fonctionnent mais que TLS échoue sur l'ESP32 avec une erreur d'allocation mémoire. Avec environ 74 Ko de heap libre et un plus grand bloc contigu proche de 39 Ko, le problème est principalement la fragmentation mémoire. La tentative de libération des sprites graphiques a provoqué une régression tactile et portail captif et ne doit pas être réintroduite.

Les architectures robustes à comparer sont notamment une passerelle ESP32-S2, un relais local ou un service MQTT/VPS.

### 4.6 MQTT, Flutter et VPS

Ces éléments sont inscrits dans la trajectoire produit : HiveMQ pour les échanges MQTT, Flutter pour l'application mobile et migration éventuelle vers un VPS OVH. Ils ne doivent pas être considérés comme nécessaires au fonctionnement local de base.

### 4.7 Cybersécurité

La cybersécurité est désormais un pilier transverse permanent. Les priorités sont l'authentification réelle de l'administration Web, la gestion des secrets, le TLS, les ACL MQTT, la sécurité OTA, le durcissement du VPS et la supervision des incidents.

## 5. Invariants majeurs

1. Une panne réseau ou cloud ne bloque pas l'arrosage local.
2. Une panne de capteur ou d'extension ne provoque jamais une activation intempestive.
3. Le planificateur ne pilote jamais directement le matériel.
4. La durée maximale de sécurité des relais ne doit pas être supprimée.
5. La SD n'est jamais indispensable au démarrage minimal et à la récupération.
6. Une modification de schéma NVS est versionnée et migrée ou explicitement réinitialisée.
7. Les bus écran, tactile, SD et I2C doivent conserver leurs contraintes matérielles validées.
8. Une recommandation intelligente ne modifie pas silencieusement les programmes.
9. Une fonction distante confirme l'exécution réelle au lieu de supposer qu'une commande reçue a réussi.
10. Toute évolution importante met à jour code, tests, risques et documentation.

## 6. Risques techniques principaux

- fragmentation mémoire sur ESP32 sans PSRAM ;
- concurrence et stabilité des périphériques SPI ;
- évolution simultanée Legacy et V4 ;
- complexité croissante de la configuration persistée ;
- ouverture réseau avant mise en place d'une authentification forte ;
- dépendance future aux certificats et à leur renouvellement ;
- régressions tactiles ou Web provoquées par les optimisations mémoire ;
- dérive entre documentation, roadmap et code réel ;
- difficulté de test exhaustif sans banc matériel automatisé.

## 7. Priorités recommandées

1. maintenir un checkpoint exact par chantier ;
2. sécuriser l'administration locale avant l'exposition distante ;
3. finaliser une architecture de notification compatible avec les contraintes mémoire ;
4. poursuivre OTA avec signature et rollback ;
5. stabiliser le contrat MQTT avant Flutter et VPS ;
6. développer les extensions de débit de manière découplée du cœur de commande ;
7. augmenter progressivement la maturité documentaire des composants critiques vers D5.

## 8. Référence de reprise documentaire

Ce document est une vue consolidée. Lorsqu'un détail diffère d'un checkpoint plus récent sur une branche de travail, le checkpoint et le dépôt réel de cette branche priment. Toute consolidation future doit citer la branche, le commit et les validations matérielles correspondantes.
