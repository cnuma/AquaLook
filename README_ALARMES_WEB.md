# Correctif interface Web des alarmes

## Diagnostic exact

La branche contenait bien les routes :

- `GET /api/faults`
- `POST /api/logs/ack`
- `GET /logs`

Mais la page principale ne chargeait aucun composant d'alarme.

Le lien discret en bas de `data/index.html` pointait vers :

```text
/api/logs
```

Cette route affiche uniquement le tableau brut. Le bouton d'acquittement se trouve
sur :

```text
/logs
```

C'est pourquoi le bouton semblait avoir disparu, y compris en navigation privée.
Ce n'était pas un problème de cache.

## Fichiers

- `src/WebManager.h` : remplacement complet
- `data/alarm.js` : nouveau
- `data/alarm.css` : nouveau

## Fonctionnement

### Erreur non acquittée

- panneau visible sur `index.html`
- triangle rouge clignotant
- panneau rouge pulsé
- clic vers `/logs`

### Défaut actif acquitté

- panneau toujours visible
- triangle rouge fixe
- fond hachuré rouge/vert
- clic vers `/logs`
- bouton `/logs` hachuré, libellé `Erreurs acquittees`

### Plus aucun défaut et aucune alarme non acquittée

- panneau masqué

## Déploiement

Les deux fichiers dans `data/` imposent le téléversement LittleFS :

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t upload
pio run -e ProgrammeArrosage -t uploadfs
```

Selon l'environnement PlatformIO, la cible peut être `uploadfs` et non `buildfs`.

## Test direct

Avant même d'ouvrir l'interface :

```text
http://IP_DU_MODULE/api/faults
http://IP_DU_MODULE/logs
```

`/api/faults` doit retourner un JSON contenant `active` et `unacknowledged`.
