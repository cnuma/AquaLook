# Checkpoint AquaLook — OTA-1 — Qualification HTTPS/TLS isolée

Date : 2026-07-22

## 1. Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/remote-ota-github`
- Checkpoint précédent : `docs/checkpoints/CHECKPOINT_2026-07-21_work-remote-ota-github_OTA0-1.md`
- Firmware de banc : `src/test_ota_https.cpp`
- Environnement PlatformIO : `test_ota_https`

## 2. Objet du run

Qualifier une connexion HTTPS/TLS sortante vers GitHub dans un firmware de banc isolé, sans moteur d’arrosage, sans écran, sans tactile, sans serveur Web et sans écriture dans les partitions OTA.

Le test utilise :

- cible : `api.github.com:443` ;
- méthode : `HEAD` ;
- chemin : `/repos/cnuma/AquaLook/releases/latest` ;
- validation du certificat désactivée uniquement pour cette qualification de transport ;
- aucune écriture dans `app0`, `app1` ou `otadata`.

## 3. Résultat matériel

Logs validés :

```text
[OTA-1] isolated HTTPS qualification firmware
[OTA-1] irrigation_runtime=disabled ota_write=disabled
[OTA-1] memory stage=boot heapFree=274144 heapMin=239412 largestBlock=110580 stackFree=6252
[OTA-1] wifi=connected ip=172.20.10.7 rssi=-44
[OTA-1] probe=start target=api.github.com method=HEAD insecure=yes
[OTA-1] memory stage=before-client heapFree=240076 heapMin=236628 largestBlock=110580 stackFree=6240
[OTA-1] memory stage=before-connect heapFree=237860 heapMin=236628 largestBlock=110580 stackFree=6240
[OTA-1] tls connected=yes durationMs=1298
[OTA-1] memory stage=after-connect heapFree=196300 heapMin=188324 largestBlock=110580 stackFree=3340
[OTA-1] http statusLine=HTTP/1.1 404 Not Found
[OTA-1] memory stage=after-close heapFree=236940 heapMin=188324 largestBlock=110580 stackFree=3340
[OTA-1] result=success tls=yes request=yes otaWrite=no
```

## 4. Analyse mémoire

| Étape | Heap libre | Plus grand bloc | Stack libre |
|---|---:|---:|---:|
| Boot | 274 144 | 110 580 | 6 252 |
| Avant création du client | 240 076 | 110 580 | 6 240 |
| Avant connexion TLS | 237 860 | 110 580 | 6 240 |
| Après connexion TLS | 196 300 | 110 580 | 3 340 |
| Après fermeture | 236 940 | 110 580 | 3 340 |

Observations :

- la négociation TLS consomme temporairement environ 41 560 octets de heap entre `before-connect` et `after-connect` ;
- après fermeture, la heap revient à 236 940 octets, soit seulement 920 octets de moins qu’avant connexion ;
- le plus grand bloc libre reste stable à 110 580 octets pendant tout le test ;
- aucun échec d’allocation TLS n’est observé ;
- la durée de connexion TLS est de 1 298 ms ;
- la baisse du niveau de pile disponible doit être surveillée lors des essais répétés et dans le runtime nominal.

## 5. Interprétation du code HTTP 404

Le code `HTTP/1.1 404 Not Found` ne constitue pas un échec du transport.

Il confirme que :

1. la résolution réseau a fonctionné ;
2. la connexion TCP a fonctionné ;
3. la négociation TLS a réussi ;
4. la requête HTTP a atteint GitHub ;
5. GitHub a renvoyé une réponse HTTP valide.

Le dépôt privé, l’absence de release publique ou l’accès non authentifié peuvent expliquer le 404. Cette question relève de la future stratégie de publication GitHub Releases, pas de la qualification TLS.

## 6. Décisions OTA-1

### D-OTA1-1 — Transport TLS techniquement possible

Le matériel peut établir une connexion TLS vers GitHub dans un environnement isolé sans erreur mémoire.

### D-OTA1-2 — Aucun certificat désactivé en production

`setInsecure()` reste réservé au banc de qualification. Le futur transport OTA devra valider l’identité du serveur avec une autorité racine ou une stratégie cryptographique équivalente.

### D-OTA1-3 — Validation requise dans le runtime nominal

Le succès du banc isolé ne suffit pas à garantir le succès dans AquaLook complet, où la heap disponible et le plus grand bloc sont plus faibles après initialisation de l’écran, du Web, de la SD et des autres composants.

### D-OTA1-4 — Étape suivante non destructive

La prochaine étape doit intégrer un probe HTTPS ponctuel, désactivé par défaut et explicitement déclenché, dans le runtime nominal. Il doit mesurer la mémoire avant, pendant et après TLS, sans écrire dans une partition OTA et sans modifier les sprites graphiques.

## 7. Critères de validation

| Critère | État |
|---|---|
| Compilation du banc OTA-1 | VALIDÉE |
| Connexion Wi-Fi | VALIDÉE |
| Connexion TLS vers GitHub | VALIDÉE |
| Réponse HTTP reçue | VALIDÉE — 404 acceptable |
| Absence d’écriture OTA | VALIDÉE |
| Retour de heap après fermeture | VALIDÉ — écart 920 octets |
| Plus grand bloc contigu stable | VALIDÉ — 110 580 octets |
| Certificat serveur vérifié | NON — volontairement reporté |
| Test dans runtime nominal complet | À RÉALISER |
| Répétition et test longue durée | À RÉALISER |

## 8. Conclusion

OTA-1 valide la faisabilité fondamentale d’une connexion HTTPS/TLS entre le CYD et GitHub. Le blocage observé précédemment avec ntfy n’est donc pas une incapacité matérielle générale à utiliser TLS ; il dépend de l’état mémoire du runtime complet et de sa fragmentation.

La suite logique est `OTA-1.1` : probe GitHub intégré de façon contrôlée au runtime nominal, sans écriture OTA, avec instrumentation mémoire et possibilité de désactivation immédiate.