# AquaLook Firmware — WeatherManager

- Référence : FW-012
- Statut : relié au code
- Maturité : D4
- Sources : `src/WeatherManager.h`, `src/WeatherManager.cpp`

## Mission

`WeatherManager` récupère et agrège les prévisions OpenWeatherMap sur cinq jours sans bloquer la boucle Arduino.

## Modèle

`ForecastDay` stocke pluie, températures, ressenti, vent, rafales, probabilité de pluie, humidité, nébulosité, pression, description, icône et validité. Le manager conserve cinq entrées ainsi qu'un résumé pluie/température.

## Cycle non bloquant

`update(wifiConnected)` décide si une récupération est nécessaire. `startFetch()` copie d'abord la configuration utile dans `FetchRequest`, puis lance une tâche unique. La tâche exécute HTTP et JSON dans `performFetch()` sans relire `ConfigManager`. Le résultat borné est déposé dans `FetchResult`, puis appliqué par `applyPendingResult()` dans le contexte normal de la boucle.

Les nouvelles tentatives sont espacées ; une récupération déjà en cours n'est pas dupliquée.

## API de lecture

`isRainExpected()`, `getRainMm()`, `getTempC()`, `hasFetched()`, `getStatusStr()` et `getForecastDay(offset)` exposent l'état calculé sans donner au service météo le contrôle direct des relais.

## Dette de sécurité ouverte

Au commit documenté, l'appel OpenWeatherMap utilise encore HTTP non chiffré. Le contrat de cybersécurité correspondant reste en `expectedFailure` et l'issue #16 reste ouverte. Cette fiche ne considère pas ce point corrigé.

## Validation

Tester hors Wi-Fi, clé absente, timeout, réponse invalide, payload volumineux, reprise après erreur et évolution du heap. La météo distante ne doit jamais empêcher l'arrêt local des relais.

## Références

- `docs/engineering/16_V4_MODEL_AND_WEATHER.md`
- `docs/engineering/15_RUNTIME_AND_PROFILING.md`
- `docs/engineering/37_SECURITY_CONTRACTS_AND_CI.md`
- `docs/developer/DEV-009_Ajouter_un_service_reseau.md`
