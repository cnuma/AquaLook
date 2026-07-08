# AquaLook V4 — Stratégie de lecture d’état EquipmentOutput Web/LCD

**Date :** 8 juillet 2026  
**Phase :** 4 — Run 4.5  
**Statut :** stratégie documentaire, aucun changement Web/LCD actif

## 1. Objectif

Le Run 4.5 prépare la migration progressive des lectures d’état actuellement faites directement via `RelaisManager::getState(zone)`.

Le Run 4.3 a déjà déplacé la commande runtime vers :

```text
ScheduleManager
  -> onRelayRequest(zone, state)
  -> EquipmentOutputRuntimeAdapter::setZoneValve(zone, state, millis())
  -> RelaisManager::setRelay(zone, state)
```

Mais les lectures d’état Web/LCD restent encore branchées sur `RelaisManager`.

Objectif du Run 4.5 : documenter les points de lecture et définir la stratégie de migration, sans modifier encore les écrans.

## 2. Invariant majeur

La source d’état affichable doit rester cohérente avec l’état réellement commandé.

Tant que `RelaisManager` est le backend actif, l’adaptateur `EquipmentOutputRuntimeAdapter` doit lire l’état auprès de `RelaisManager`.

La migration doit donc être progressive :

```text
Phase actuelle : Web/LCD -> RelaisManager::getState(zone)
Phase suivante : Web/LCD -> EquipmentOutputRuntimeAdapter::getZoneValveState(zone)
Backend réel   : EquipmentOutputRuntimeAdapter -> RelaisManager::getState(zone)
```

## 3. Points de lecture Web actuels

### 3.1 API status

Fichier :

```text
src/WebManager.cpp
```

Fonction :

```cpp
WebManager::handleStatus(AsyncWebServerRequest* req)
```

Lecture actuelle :

```cpp
zo["active"] = _relais ? _relais->getState(z) : false;
```

Usage : état actif exposé au JSON `/api/status`, consommé par l’interface Web.

Rôle futur : remplacer cette lecture par une fonction d’état logique `EquipmentOutput`, mais seulement après injection contrôlée de l’adaptateur dans `WebManager`.

## 4. Points de lecture LCD actuels

### 4.1 Détection d’activité pour veille/refresh

Fichier :

```text
src/DisplayManager.cpp
```

Fonction :

```cpp
DisplayManager::update()
```

Lecture actuelle :

```cpp
if (_relais->getState(z)) { anyActive = true; break; }
```

Usage :

- empêcher ou adapter la veille écran quand un arrosage est actif ;
- choisir la cadence de refresh active ou nominale ;
- déclencher l’affichage dynamique à 1 seconde pendant un arrosage.

### 4.2 Bouton arroser/arrêter

Fichier :

```text
src/DisplayManager.cpp
```

Fonction :

```cpp
DisplayManager::handleTouchZone(uint16_t tx, uint16_t ty)
```

Lecture actuelle :

```cpp
if (_relais && _relais->getState(_selectedZone)) {
    _schedule->stopManualWatering(_selectedZone);
} else {
    _schedule->startManualWatering(_selectedZone);
}
```

Usage : décider si le bouton doit arrêter ou démarrer l’arrosage manuel.

## 5. Décision d’architecture Run 4.5

Ne pas faire dépendre directement Web/LCD de `RelaisManager` à long terme pour les états logiques.

Mais ne pas brancher Web/LCD directement sur le domaine pur non plus.

La bonne cible est une façade runtime simple :

```text
EquipmentOutputRuntimeAdapter
```

Elle masque encore `RelaisManager` aujourd’hui, mais pourra demain masquer :

- un registre de drivers binaires ;
- un état runtime `EquipmentRuntimeState` ;
- un backend relais ;
- un backend GPIO ;
- un backend I2C différent ;
- un équipement non-relais.

## 6. Stratégie recommandée

### Étape 4.6 — Injection passive dans WebManager

Ajouter un pointeur optionnel :

```cpp
AquaLook::Runtime::EquipmentOutputRuntimeAdapter* _outputs = nullptr;
```

Ajouter une méthode :

```cpp
void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs);
```

Ne modifier aucune route ni aucun JSON dans ce premier temps.

### Étape 4.7 — Lecture Web via adaptateur

Modifier uniquement `WebManager::handleStatus()` :

Avant :

```cpp
zo["active"] = _relais ? _relais->getState(z) : false;
```

Après :

```cpp
zo["active"] = outputStateAsBool(z);
```

Avec helper local :

```cpp
bool WebManager::zoneValveActive(uint8_t zone) const;
```

Comportement du helper :

1. si `_outputs` est disponible, lire `getZoneValveState(zone)` ;
2. si l’état est `VALID`, retourner la valeur logique ;
3. sinon fallback vers `_relais->getState(zone)` ;
4. sinon `false`.

### Étape 4.8 — Injection passive dans DisplayManager

Même principe côté LCD :

```cpp
AquaLook::Runtime::EquipmentOutputRuntimeAdapter* _outputs = nullptr;
void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs);
```

Aucune logique UI modifiée dans cette étape.

### Étape 4.9 — Lecture LCD via helper

Introduire :

```cpp
bool DisplayManager::zoneValveActive(uint8_t zone) const;
```

Puis remplacer progressivement :

```cpp
_relais->getState(z)
```

par :

```cpp
zoneValveActive(z)
```

Priorité :

1. `DisplayManager::update()` pour `anyActive` ;
2. `DisplayManager::handleTouchZone()` pour bouton arroser/arrêter.

## 7. Pourquoi ne pas modifier directement Web/LCD au Run 4.5

Les écrans Web et LCD sont stables après Run 4.4.

L’utilisateur a validé que Web et LCD semblent encore fonctionner correctement après compilation et test matériel rapide.

Il faut donc éviter une modification large immédiate, car :

- Web et LCD sont des surfaces utilisateur visibles ;
- les lectures d’état pilotent aussi la veille écran ;
- le bouton LCD arroser/arrêter dépend de cette lecture ;
- une erreur d’état pourrait inverser le comportement manuel ;
- la migration doit rester réversible.

## 8. Contrat du helper d’état

Le futur helper doit retourner un booléen simple pour préserver les usages existants :

```cpp
bool zoneValveActive(uint8_t zone) const;
```

Mais il doit s’appuyer sur le modèle d’état générique :

```cpp
Domain::EquipmentStateValue state = _outputs->getZoneValveState(zone);
```

Conversion proposée :

```text
VALID + BINARY + value != 0 -> true
VALID + BINARY + value == 0 -> false
INVALID/UNKNOWN/NOT_SUPPORTED -> fallback historique
```

## 9. Invariants à préserver pour la migration active

1. Aucun changement NVS.
2. Aucun changement de format JSON.
3. Aucun changement de nom de champ Web.
4. Aucun changement de comportement bouton LCD.
5. Aucun changement de cadence refresh LCD.
6. Fallback obligatoire vers `RelaisManager` tant que la migration n’est pas complète.
7. Ne pas supprimer `_relais` de Web/LCD dans les premiers runs.
8. Ne pas brancher les drivers V4 Phase 3.
9. Ne pas déplacer la sécurité `RelaisManager::update()`.
10. Ne pas modifier `ScheduleManager`.

## 10. Critères de réussite du Run 4.5

Le Run 4.5 est réussi si :

1. les points de lecture Web/LCD sont clairement identifiés ;
2. la stratégie de migration est documentée ;
3. aucune modification runtime active n’est introduite ;
4. la prochaine étape peut injecter l’adaptateur dans WebManager sans changer les sorties JSON ;
5. le plan de fallback vers `RelaisManager` est explicite.

## 11. Suite recommandée

```text
AquaLook V4 — Phase 4 — Run 4.6
Injection passive de EquipmentOutputRuntimeAdapter dans WebManager
```

Objectif : préparer `WebManager` à recevoir l’adaptateur, sans modifier le JSON `/api/status` et sans changer le comportement affiché.
