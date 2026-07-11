# CHECKPOINT AquaLook — Phase 6
## Modèle d’objets, dépendances, persistance et invariant non bloquant

**Date :** 11 juillet 2026  
**Projet :** AquaLook  
**Dépôt :** `cnuma/AquaLook`  
**Branche de travail :** `feature/aqualook-v4-domain`  
**Base fonctionnelle avant ce checkpoint :** `3ee5f0c2cc587980c95f5c91e59b5f450421081e`  
**Statut du working tree utilisateur au dernier retour :** propre et synchronisé après validation du Run 6.20.  
**Attention :** le Run 6.21 a été poussé mais sa compilation n’est pas encore confirmée au moment de ce checkpoint.

---

# 1. Objet du checkpoint

Ce document fige l’état technique atteint après les Runs 6.18 à 6.21 et, surtout, les décisions structurantes prises pour la suite d’AquaLook.

La trajectoire initiale centrée sur une « pompe configurable » a été corrigée. La pompe doit désormais être considérée comme un objet métier parmi d’autres dans un système générique :

- équipements physiques ;
- capteurs ;
- objets logiques ;
- programmes et activités ;
- ressources partagées ;
- conditions ;
- interlocks ;
- inhibitions ;
- défauts et alarmes.

Ces objets possèdent un état, créent des dépendances et peuvent provoquer l’arrêt, le blocage, la reprise ou le déroutement d’autres objets.

Une deuxième décision majeure est figée :

> AquaLook doit être entièrement événementiel et non bloquant.  
> Aucun `delay()` ni aucune attente active ne doit être utilisé dans le runtime, sauf exception rare, localisée, documentée et justifiée.

---

# 2. Source de vérité et règles de reprise

## 2.1 Source de vérité

La source de vérité est la branche :

```text
feature/aqualook-v4-domain
```

Base avant création du présent checkpoint :

```text
3ee5f0c2cc587980c95f5c91e59b5f450421081e
```

Le présent checkpoint doit être ajouté dans :

```text
docs/checkpoints/
```

Après son commit, le nouveau HEAD devient la nouvelle référence documentaire.

## 2.2 Environnements de compilation obligatoires

Toute modification doit être compilée dans les deux environnements :

```powershell
pio run -e ProgrammeArrosage_v4
pio run -e ProgrammeArrosage_legacy
```

## 2.3 Règles Git et anti-régression

Avant et après toute modification :

```powershell
git restore .vscode/launch.json
git status
git pull --ff-only
git rev-parse HEAD
```

Après compilation :

```powershell
git restore .vscode/launch.json
git status
```

`.vscode/launch.json` est généré automatiquement et ne doit jamais être commité.

Pour toute évolution :

1. identifier la base utilisée ;
2. identifier les fichiers et fonctions concernés ;
3. préserver les invariants ;
4. modifier le minimum nécessaire ;
5. comparer avec la base ;
6. compiler les deux environnements ;
7. indiquer les tests matériels restant à effectuer ;
8. ne jamais reconstruire un fichier depuis la mémoire si une version plus récente existe.

---

# 3. État validé des Runs récents

## 3.1 Run 6.18 — contrat de configuration runtime

Fichiers introduits :

```text
src/EquipmentRuntimeConfig.h
src/EquipmentRuntimeConfig.cpp
docs/architecture/AQUALOOK_V4_PHASE6_RUN6_18_CONFIGURABLE_EQUIPMENT_RUNTIME.md
```

Objectifs :

- introduire des modes d’exécution ;
- rendre les délais configurables ;
- préparer une configuration runtime ;
- maintenir la commande physique de pompe désactivée.

Correction nécessaire appliquée ensuite :

```cpp
EquipmentControlMode::MODE_DISABLED
EquipmentControlMode::MODE_SHADOW
EquipmentControlMode::MODE_PHYSICAL
```

Les anciens noms `DISABLED`, `SHADOW`, `PHYSICAL` ne doivent pas être réintroduits, car `DISABLED` entre en collision avec une macro du framework ESP32.

## 3.2 Run 6.19 — persistance NVS dédiée

Fichiers introduits :

```text
src/EquipmentRuntimeConfigStore.h
src/EquipmentRuntimeConfigStore.cpp
docs/architecture/AQUALOOK_V4_PHASE6_RUN6_19_EQUIPMENT_CONFIG_NVS_STORE.md
```

La configuration runtime dispose d’une clé NVS indépendante :

```text
namespace : aqualook
clé       : equipCfg
schema    : 1
```

Caractéristiques :

- magic ;
- schema ;
- taille ;
- CRC32 ;
- repli sûr ;
- mode désactivé par défaut.

Compilation validée :

```text
ProgrammeArrosage_v4     : SUCCESS
ProgrammeArrosage_legacy : SUCCESS
```

## 3.3 Run 6.20 — branchement au démarrage

Fichiers concernés :

```text
src/main.cpp
docs/architecture/AQUALOOK_V4_PHASE6_RUN6_20_RUNTIME_CONFIG_BOOTSTRAP.md
```

Comportement :

```text
MODE_DISABLED → aucun scénario pompe
MODE_SHADOW   → simulation passive
MODE_PHYSICAL → demande neutralisée et rabattue vers shadow
```

Aucune commande physique de pompe n’est autorisée.

Validation matérielle confirmée :

```text
Shadow START : steps=1, pump=no
Shadow STOP  : steps=1, pump=no
Compteur pompe : users=0
```

Commandes de vanne validées :

```text
Equipment: zone 1 ON  path=physical_backend
Equipment: zone 1 OFF path=physical_backend
```

Aucun événement `PUMP_ON` ou `PUMP_OFF` observé.

Le Run 6.20 est validé :

- compilation V4 ;
- compilation legacy ;
- téléversement ;
- test manuel d’une zone ;
- absence de régression constatée.

## 3.4 Run 6.21 — profil générique et abstraction de stockage

Fichiers ajoutés :

```text
src/EquipmentAutomationProfile.h
src/EquipmentAutomationProfile.cpp
src/EquipmentConfigRepository.h
src/EquipmentConfigRepository.cpp
docs/architecture/AQUALOOK_V4_PHASE6_RUN6_21_GENERIC_EQUIPMENT_PROFILE_AND_STORAGE.md
```

Décision :

- la pompe n’est plus un cas particulier ;
- elle est un équipement de type `EQUIP_PUMP` ;
- le profil couvre tous les équipements ;
- la persistance est abstraite ;
- la SD devient la future source principale ;
- la NVS devient un fallback sûr.

Ordre de chargement prévu :

```text
1. SD
2. NVS fallback
3. safe defaults
```

**Statut au moment du checkpoint :**

```text
Code poussé       : OUI
Compilation V4    : EN ATTENTE DE RETOUR
Compilation legacy: EN ATTENTE DE RETOUR
Test matériel     : NON REQUIS À CE STADE
```

Le Run 6.21 n’est pas encore branché au runtime courant.

---

# 4. Décision structurante : tout est objet

AquaLook ne doit pas devenir une accumulation de cas particuliers :

```text
si pompe...
si pression...
si éclairage...
si ventilation...
```

Le système doit être construit autour d’objets génériques.

## 4.1 Catégories d’objets

### Objets physiques

Exemples :

- vanne ;
- pompe ;
- relais ;
- éclairage ;
- ventilateur ;
- brumisateur ;
- capteur de pression ;
- débitmètre ;
- capteur de niveau ;
- carte relais distante.

### Objets logiques

Exemples :

- programme d’arrosage ;
- activité manuelle ;
- autorisation générale ;
- réseau d’eau disponible ;
- mode vacances ;
- protection antigel ;
- condition météo ;
- ressource électrique ;
- capacité hydraulique.

### Objets calculés

Exemples :

```text
Réseau d’eau sain =
pression correcte
AND débit détecté
AND niveau cuve suffisant
```

---

# 5. Séparation obligatoire des couches

Chaque objet doit être représenté dans trois couches distinctes.

## 5.1 Configuration

Contient notamment :

- identifiant stable ;
- type ;
- nom ;
- paramètres ;
- dépendances ;
- politique d’exécution ;
- affectation physique ;
- état sûr ;
- timeouts ;
- politique de reprise.

## 5.2 État runtime

Contient notamment :

```text
enabled
available
healthy
requestedState
commandedState
observedState
confirmedState
runtimeState
faulted
faultCode
rootCauseId
lastChangeMs
```

## 5.3 Backend matériel

Contient notamment :

- relais local ;
- GPIO ;
- expander I²C ;
- carte distante ;
- capteur analogique ;
- capteur numérique ;
- équipement virtuel ;
- simulation shadow.

La logique métier ne doit pas dépendre directement du type de backend.

---

# 6. Machine à états générique

Un booléen `active` est insuffisant.

États de base envisagés :

```text
DISABLED
UNKNOWN
IDLE
STARTING
RUNNING
STOPPING
BLOCKED
FAULT
RECOVERING
```

Exemple pompe :

```text
IDLE
→ STARTING
→ commande relais
→ attente confirmation pression/débit
→ RUNNING
```

Échec :

```text
STARTING
→ timeout ou pression invalide
→ FAULT
→ propagation aux dépendants
```

Aucun état ne doit attendre de manière bloquante. Toute attente doit être représentée par :

- un état ;
- une échéance ;
- un retour immédiat à la boucle principale.

---

# 7. Commande, observation et confirmation

Il faut distinguer :

```text
requestedState
commandedState
observedState
confirmedState
```

Exemple :

```text
requestedState = ON
commandedState = ON
observedState  = OFF
confirmedState = false
```

Cette situation peut signifier :

- relais commandé mais pompe non alimentée ;
- vanne commandée ouverte mais débit nul ;
- ventilateur demandé mais retour tachymétrique absent.

Chaque transition doit pouvoir définir :

- délai de démarrage ;
- délai d’arrêt ;
- timeout de confirmation ;
- timeout de communication ;
- timeout de récupération.

---

# 8. Graphe de dépendances

## 8.1 Sens canonique

Les liens sont stockés dans un seul sens :

```text
objet dépendant → objet requis
```

Exemple :

```text
Programme zone 1 → Vanne zone 1
Programme zone 1 → Pompe principale
Pompe principale → Capteur pression
```

Un index inverse est construit en RAM pour propager rapidement les changements vers les objets dépendants.

## 8.2 Types de dépendances

Types structurants à prévoir :

```text
REQUIRED
OPTIONAL
INTERLOCK
TRIGGER
INHIBIT
RESOURCE
```

### REQUIRED

La perte de la dépendance arrête ou bloque le dépendant.

### OPTIONAL

Le fonctionnement dégradé reste possible.

### INTERLOCK

Une condition précise doit être satisfaite avant activation.

### TRIGGER

Un changement d’état déclenche une action.

### INHIBIT

L’état de la dépendance interdit l’activation.

### RESOURCE

Le dépendant consomme une capacité partagée.

## 8.3 Exemple de protection de pompe

```text
Capteur pression
        ↓ REQUIRED / INTERLOCK
Pompe
        ↓ REQUIRED
Programmes d’arrosage
```

Si la pression devient incorrecte :

1. le capteur publie un changement d’état ;
2. la pompe devient indisponible ou fautive ;
3. le graphe identifie tous les dépendants ;
4. les programmes reçoivent `DEPENDENCY_LOST` ;
5. les programmes sont arrêtés ;
6. les vannes sont fermées selon le plan de sécurité ;
7. la pompe est arrêtée ;
8. la cause racine est conservée.

Ce mécanisme doit fonctionner pour n’importe quel objet, pas uniquement une pompe.

---

# 9. Propagation causale des défauts

Un défaut source ne doit pas être dupliqué comme plusieurs défauts indépendants.

Exemple :

```text
Fault #42
source physique : capteur pression 1
cause racine    : pression insuffisante
conséquence     : pompe principale indisponible
conséquence     : programme zone 1 arrêté
conséquence     : programme zone 2 arrêté
```

Chaque chaîne doit porter :

```text
eventId
correlationId
rootCauseId
sourceObjectId
dependentObjectId
timestamp
configurationRevision
```

Les logs, l’interface Web et les diagnostics doivent pouvoir afficher la cause directe et la cause racine.

---

# 10. Politiques de reprise

La disparition d’un défaut ne signifie pas automatiquement reprise.

Politiques à prévoir :

```text
MANUAL_RESET
AUTO_RETRY
AUTO_RESUME
RESTART
ABORT
```

Paramètres associés :

- nombre maximum de tentatives ;
- délai entre tentatives ;
- fenêtre temporelle ;
- verrouillage après échecs ;
- acquittement manuel obligatoire.

Exemple pompe :

```text
3 tentatives
30 secondes entre tentatives
puis défaut verrouillé
```

Les politiques sont configurées sur les objets ou les dépendances, pas codées dans les managers.

---

# 11. États sûrs

Chaque type d’objet doit définir son état sûr.

Exemples :

```text
vanne       → fermée
pompe       → arrêtée
éclairage   → éteint, sauf règle particulière
ventilation → potentiellement active en sécurité
chauffage   → arrêté
alarme      → active
```

Champs possibles :

```text
safeState
failOpen
failClosed
holdLastState
```

La règle universelle « toutes les sorties à OFF » n’est pas suffisante.

---

# 12. Ressources partagées, priorités et arbitrage

La pompe n’est qu’un premier exemple de ressource partagée.

Ressources futures :

- débit disponible ;
- puissance électrique ;
- alimentation 24 V ;
- réserve d’eau ;
- nombre maximum de relais simultanés ;
- bus de communication ;
- pompe commune ;
- capacité d’une cuve.

Exemple :

```text
Zone 1 demande 20 l/min
Zone 2 demande 15 l/min
Pompe limitée à 30 l/min
→ démarrage simultané refusé ou séquencé
```

Chaque activité pourra posséder :

```text
priority
preemptible
maximumWaitMs
conflictPolicy
```

Exemples de priorités :

```text
protection antigel : critique
arrosage manuel    : élevée
arrosage programmé : normale
maintenance        : spécifique
```

---

# 13. Validation du graphe

Une configuration doit être validée entièrement avant activation.

Vérifications minimales :

- identifiants uniques ;
- références existantes ;
- absence d’auto-dépendance ;
- absence de cycle ;
- types compatibles ;
- affectations relais valides ;
- absence de conflit matériel ;
- capacités respectées ;
- états sûrs définis ;
- limites de structures respectées.

Exemple de cycle interdit :

```text
A dépend de B
B dépend de C
C dépend de A
```

Une configuration invalide doit être rejetée globalement, pas partiellement appliquée.

---

# 14. Configuration active et candidate

Deux configurations doivent être distinguées :

```text
activeConfig
candidateConfig
```

Workflow :

1. charger la candidate ;
2. valider son schema ;
3. valider les objets ;
4. valider les dépendances ;
5. détecter les cycles ;
6. vérifier les backends ;
7. construire le graphe runtime ;
8. tester éventuellement en shadow ;
9. sauvegarder ;
10. promouvoir atomiquement.

La dernière configuration valide doit rester disponible.

Les changements de configuration doivent être transactionnels.

---

# 15. Persistance : SD principale, NVS de secours

## 15.1 SD

La SD est adaptée pour :

- profil complet ;
- dépendances ;
- politiques ;
- historique ;
- sauvegardes ;
- révisions ;
- export/import ;
- diagnostics.

Fichier envisagé :

```text
/config/equipment-profile.json
```

Le JSON ne doit pas rester en mémoire. Il doit être :

1. lu en streaming ou par section ;
2. validé ;
3. converti en structures compactes ;
4. libéré.

## 15.2 NVS

La NVS reste utile pour :

- démarrage sans SD ;
- copie minimale de secours ;
- dernière configuration sûre ;
- paramètres essentiels ;
- protection contre une SD absente ou corrompue.

La NVS ne doit pas devenir la source principale d’un profil complexe.

---

# 16. Mode simulation par objet

Le mode doit être défini au niveau de chaque objet :

```text
MODE_DISABLED
MODE_SHADOW
MODE_PHYSICAL
```

Exemple de configuration mixte :

```text
vannes           : physical
pompe             : shadow
capteur pression  : shadow
éclairage         : disabled
```

Le mode shadow est un outil permanent de validation, pas seulement une étape temporaire.

---

# 17. Invariant majeur : runtime non bloquant

## 17.1 Règle

> Aucun composant runtime ne doit suspendre volontairement l’exécution. Toute attente fonctionnelle doit être représentée par un état et une échéance.

Interdit :

```cpp
relayOn();
delay(500);
openValve();
```

Correct :

```cpp
state = WAITING_STARTUP_DELAY;
deadlineMs = nowMs + 500U;
return;
```

Puis :

```cpp
if (deadlineReached(nowMs, deadlineMs)) {
    openValve();
    state = RUNNING;
}
```

Helper recommandé, sûr avec le débordement de `millis()` :

```cpp
inline bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}
```

## 17.2 Exceptions

Un `delay()` ne peut être accepté que si :

- il est hors runtime critique ;
- il est indispensable au matériel ;
- il est très court ;
- il est localisé ;
- il est documenté ;
- il ne peut pas être remplacé raisonnablement ;
- son impact est mesuré.

Même les `delay()` du `setup()` doivent être audités et idéalement supprimés.

## 17.3 Blocages indirects à auditer

La recherche ne doit pas se limiter à `delay()`.

Inspecter :

- `while` d’attente ;
- boucles de polling ;
- timeouts réseau synchrones ;
- scans I²C complets ;
- écritures SD longues ;
- lectures JSON volumineuses ;
- rendu TFT massif ;
- DNS/Wi-Fi ;
- météo HTTP ;
- opérations de fichier ;
- appels de bibliothèques potentiellement synchrones.

---

# 18. Précision temporelle

## 18.1 État actuel

`ScheduleManager` est non bloquant et utilise `millis()` :

```text
startMs
durationMs
remainingMs
```

L’arrêt est réalisé au prochain passage dans `loop()`.

La différence avec la seconde exacte correspond donc à la latence de boucle, pas nécessairement à une dérive de `millis()`.

## 18.2 Affichage des secondes

Pour éviter qu’un décompte de 60 secondes passe immédiatement à 59 :

```cpp
remainingSec = (remainingMs + 999UL) / 1000UL;
```

et non :

```cpp
remainingSec = remainingMs / 1000UL;
```

## 18.3 Budget temporel proposé

Première cible :

```text
tour de loop normal       < 10 ms
tour dégradé              < 50 ms
alerte diagnostic         > 100 ms
aucune attente métier bloquante
```

Il faut mesurer :

- durée courante de boucle ;
- maximum glissant ;
- nombre de dépassements ;
- composant responsable si possible.

---

# 19. Enveloppe de capacité ESP32

L’architecture reste compatible avec l’ESP32 si elle demeure déterministe et bornée.

Première enveloppe proposée :

```text
48 objets runtime
96 dépendances
16 activités simultanées
16 défauts actifs
32 événements en file
8 ressources partagées
```

Principes :

- tableaux statiques ;
- index numériques ;
- pas d’allocation dynamique dans le runtime ;
- pas de listes chaînées ;
- pas de gros JSON conservé en RAM ;
- événements bornés ;
- historiques sur SD ;
- recalcul incrémental des dépendants ;
- pas de moteur de script généraliste.

Le risque principal n’est pas la puissance brute de l’ESP32, mais :

- fragmentation mémoire ;
- abstraction excessive ;
- appels bloquants ;
- concurrence inutile ;
- serveur Web trop gourmand ;
- gros documents JSON ;
- traitements TFT ou SD trop longs.

---

# 20. Orientation pour le prochain run

Le prochain run ne doit pas encore brancher la SD ni activer des équipements physiques.

## Run recommandé : 6.22 — audit non bloquant et fondations temporelles

Objectifs :

1. inventorier tous les `delay()` ;
2. inventorier les boucles d’attente ;
3. identifier les appels potentiellement bloquants ;
4. supprimer les `delay()` non justifiés ;
5. introduire un helper commun pour les deadlines ;
6. mesurer la durée des tours de `loop()` ;
7. journaliser les dépassements ;
8. corriger l’arrondi des décomptes en secondes ;
9. définir les budgets temporels par composant ;
10. documenter les exceptions restantes.

Ce run doit rester limité à l’observabilité et au durcissement temporel.

## Runs suivants envisagés

### Run 6.23 — noyau d’objets runtime

- identité universelle ;
- états normalisés ;
- état demandé/commandé/observé/confirmé ;
- état sûr ;
- défauts ;
- timestamps ;
- structures statiques.

### Run 6.24 — graphe de dépendances

- liens orientés ;
- REQUIRED ;
- INTERLOCK ;
- INHIBIT ;
- index inverse ;
- validation ;
- détection de cycles.

### Run 6.25 — propagation causale

- événements ;
- correlationId ;
- rootCauseId ;
- propagation vers les dépendants ;
- arrêt sécurisé ;
- politiques de reprise minimales.

### Run 6.26 — configuration candidate/active

- validation transactionnelle ;
- promotion atomique ;
- rollback ;
- révision.

### Run ultérieur — backend SD

- fichier JSON versionné ;
- chargement en streaming ;
- SD principale ;
- NVS fallback ;
- dernière configuration valide.

---

# 21. Invariants à préserver

## I-P6-01 — Aucun cas spécial pompe

La pompe est un objet de type `EQUIP_PUMP`, pas une architecture séparée.

## I-P6-02 — Toute attente est non bloquante

Une attente métier est représentée par un état et une deadline.

## I-P6-03 — Aucun `delay()` runtime

Exception uniquement documentée et justifiée.

## I-P6-04 — Séparation config/runtime/backend

Aucune logique métier ne dépend directement du matériel.

## I-P6-05 — Dépendances orientées

Le lien canonique va du dépendant vers le requis.

## I-P6-06 — Propagation causale

Une conséquence conserve sa cause racine.

## I-P6-07 — Configuration atomique

Une configuration incomplète ne devient jamais active.

## I-P6-08 — État inconnu explicite

Au démarrage ou après perte de communication, ne jamais supposer arbitrairement `OFF`.

## I-P6-09 — État sûr par objet

Le système ne généralise pas aveuglément « sortie OFF ».

## I-P6-10 — Capacités bornées

Toutes les files et collections runtime ont une taille maximale déterministe.

## I-P6-11 — Shadow par objet

Chaque objet peut être désactivé, simulé ou physique.

## I-P6-12 — Activation physique progressive

Aucune nouvelle catégorie d’équipement n’est activée physiquement sans validation shadow puis test matériel explicite.

## I-P6-13 — Double compilation obligatoire

Les environnements V4 et legacy doivent rester compilables.

## I-P6-14 — Source réelle obligatoire

Ne jamais modifier depuis une reconstruction mémoire.

---

# 22. Tests restant à effectuer

## Run 6.21

À confirmer :

```text
ProgrammeArrosage_v4     : SUCCESS / FAILED
ProgrammeArrosage_legacy : SUCCESS / FAILED
```

Aucun test matériel nécessaire tant que le profil générique n’est pas branché.

## Audit temporel futur

À mesurer sur carte :

- durée moyenne de boucle ;
- durée maximale ;
- impact TFT ;
- impact Web ;
- impact SD ;
- impact météo ;
- précision réelle de fin d’arrosage ;
- comportement après plusieurs heures ;
- dérive apparente des secondes ;
- latence maximale d’une protection critique.

---

# 23. Risques et dettes techniques identifiés

1. Des `delay()` existent encore dans `main.cpp`, au moins au démarrage.
2. L’absence de `delay()` ne garantit pas l’absence de blocage.
3. L’interface Web et le TFT sont probablement plus coûteux que le futur graphe.
4. Les écritures SD devront être différées et bornées.
5. Les logs ne doivent pas saturer la boucle.
6. Le JSON ne doit pas être conservé en RAM.
7. Les structures génériques ne doivent pas devenir un automate universel ou un langage de script.
8. Le modèle actuel `ZoneEquipmentLink` contient encore le champ spécialisé `pumpEquipmentIndex`; il devra évoluer vers des dépendances génériques.
9. `EquipmentRuntimeConfig` et `EquipmentRuntimeConfigStore` restent orientés pompe et devront être migrés ou remplacés progressivement, sans régression.
10. Le Run 6.21 est une fondation non branchée et doit être validé par compilation avant toute suite.

---

# 24. Texte de reprise pour un nouveau chat

Copier le bloc suivant dans le nouveau chat :

```text
Nous reprenons AquaLook sur le dépôt cnuma/AquaLook, branche feature/aqualook-v4-domain.

Lis d’abord le checkpoint :
docs/checkpoints/CHECKPOINT_2026-07-11_PHASE6_OBJECT_MODEL_NON_BLOCKING.md

Base avant le commit documentaire :
3ee5f0c2cc587980c95f5c91e59b5f450421081e

État validé :
- Run 6.19 compilé V4 + legacy ;
- Run 6.20 compilé, téléversé et validé sur carte ;
- configuration NVS sûre ;
- pompe physique toujours interdite ;
- vanne physique fonctionnelle ;
- shadow sans pompe validé lorsque le mode est disabled ;
- working tree propre au dernier contrôle.

Run 6.21 :
- profil générique d’équipements ;
- repository de stockage principal/fallback ;
- SD destinée à devenir la source principale ;
- NVS destinée au fallback ;
- compilation encore à confirmer selon le dernier retour utilisateur.

Décisions structurantes :
- tout élément est un objet : équipement, capteur, programme, activité, condition ou ressource ;
- les objets ont une machine à états ;
- séparation stricte configuration / runtime / backend ;
- distinction requested / commanded / observed / confirmed ;
- graphe orienté dependent -> required ;
- politiques REQUIRED, OPTIONAL, INTERLOCK, TRIGGER, INHIBIT, RESOURCE ;
- propagation causale des défauts avec rootCauseId/correlationId ;
- arrêt des objets dépendants lorsqu’une condition requise disparaît ;
- états sûrs par objet ;
- configuration active/candidate et promotion transactionnelle ;
- détection des cycles ;
- modes disabled/shadow/physical par objet ;
- structures statiques et capacités bornées pour ESP32.

Invariant majeur :
- tout le runtime doit être événementiel et non bloquant ;
- aucun delay(), sauf exception rare et documentée ;
- toute attente = état + deadline + retour immédiat ;
- auditer aussi les while, appels réseau, SD, TFT, I2C et bibliothèques synchrones.

Prochaine tâche recommandée :
Run 6.22 — audit global non bloquant et précision temporelle :
1. inventaire delay/while ;
2. appels potentiellement bloquants ;
3. suppression des délais non justifiés ;
4. helper deadline overflow-safe ;
5. mesure du temps de loop ;
6. alertes sur boucle >100 ms ;
7. correction de l’arrondi des secondes ;
8. documentation des exceptions.

Avant toute modification :
- inspecter le dépôt réel ;
- vérifier branche et HEAD ;
- préserver tous les invariants ;
- modifier le minimum ;
- compiler ProgrammeArrosage_v4 et ProgrammeArrosage_legacy ;
- restaurer .vscode/launch.json ;
- fournir les fichiers complets si plus de deux fichiers sont modifiés.
```

---

# 25. Conclusion

La Phase 6 ne doit plus être interprétée comme « ajouter une pompe configurable ».

Elle devient la construction progressive d’un moteur d’automatisme embarqué :

- générique mais borné ;
- orienté objets ;
- événementiel ;
- non bloquant ;
- causal ;
- déterministe ;
- testable en shadow ;
- configurable de manière transactionnelle ;
- compatible avec les limites de l’ESP32.

La priorité immédiate n’est pas d’ajouter davantage de fonctionnalités. Elle est de garantir que le socle temporel et le modèle d’exécution sont suffisamment propres pour que les futurs capteurs, protections et dépendances ne soient pas greffés sur une boucle déjà trop lente ou bloquante.
