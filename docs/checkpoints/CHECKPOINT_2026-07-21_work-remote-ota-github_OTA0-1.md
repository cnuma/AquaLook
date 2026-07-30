# Checkpoint AquaLook — OTA-0.1 — Diagnostic partitions et mémoire

Date : 2026-07-21

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `work/remote-ota-github`
- Commit précédent : `e6ea1019d35ad40c6abf17a3dd665c22ea8957cd`
- Commit fonctionnel OTA-0.1 : `1763713484d6b8da4b89416a2640a7dfb2f44968`
- Fichier modifié : `src/SystemDiagnostics.cpp`

## Objet

Ajouter uniquement l’observabilité nécessaire pour confirmer la table de partitions réellement présente sur le module installé et mesurer l’état mémoire au démarrage.

Ce run ne réalise :

- aucune connexion HTTP ou HTTPS ;
- aucun téléchargement ;
- aucune écriture dans une partition OTA ;
- aucun changement de partition de démarrage ;
- aucune modification du moteur d’arrosage ;
- aucune modification de l’écran, du tactile ou du portail captif ;
- aucune réactivation du chantier notifications.

## Modifications réalisées

`SystemDiagnostics::begin()` appelle désormais un diagnostic OTA ponctuel après l’initialisation du journal.

Le diagnostic relève :

- heap libre ;
- minimum historique de heap libre ;
- plus grand bloc libre contigu en mémoire 8 bits ;
- partition applicative réellement exécutée ;
- partition de démarrage sélectionnée ;
- présence et caractéristiques de `ota_0` ;
- présence et caractéristiques de `ota_1` ;
- présence et caractéristiques de `otadata` ;
- conformité des tailles des deux partitions applicatives avec la valeur attendue `0x1F0000` ;
- état synthétique `ready=yes/no`.

Les mêmes informations de partition sont également exposées dans le JSON de diagnostic sous l’objet `ota`.

## Logs attendus au démarrage

Exemple de forme attendue :

```text
OTA diag: heapFree=... heapMin=... largestBlock=...
OTA diag: running label=app0 subtype=ota_0 address=0x010000 size=2031616
OTA diag: boot label=app0 subtype=ota_0 address=0x010000 size=2031616
OTA diag: app0 label=app0 subtype=ota_0 address=0x010000 size=2031616
OTA diag: app1 label=app1 subtype=ota_1 address=0x200000 size=2031616
OTA diag: otadata label=otadata address=0x00E000 size=8192
OTA diag: layout=dual_ota sizes=ok ready=yes
```

Le nom de la partition `running` peut être `app0` ou `app1`. Pour un premier flash filaire nominal, `app0` est attendu mais n’est pas imposé comme invariant durable.

## Résultats matériels reçus

Upload et démarrage réalisés sur `COM3` le 21 juillet 2026.

```text
[00:00:00.339] [INF] AquaLook v2.0 demarrage
[00:00:00.341] [INF] OTA diag: heapFree=265080 heapMin=259508 largestBlock=110580
[00:00:00.342] [INF] OTA diag: running label=app0 subtype=ota_0 address=0x010000 size=203161
[00:00:00.352] [INF] OTA diag: boot label=app0 subtype=ota_0 address=0x010000 size=2031616
[00:00:00.362] [INF] OTA diag: app0 label=app0 subtype=ota_0 address=0x010000 size=2031616
[00:00:00.373] [INF] OTA diag: app1 label=app1 subtype=ota_1 address=0x200000 size=2031616
[00:00:00.373] [INF] OTA diag: otadata label=otadata address=0x00E000 size=8192
[00:00:00.383] [INF] OTA diag: layout=dual_ota sizes=ok ready=yes
```

### Interprétation

- partition réellement exécutée : `app0` / `ota_0` ;
- partition de boot : `app0` / `ota_0` ;
- `app0` présente, taille confirmée : `2031616` octets ;
- `app1` présente, taille confirmée : `2031616` octets ;
- `otadata` présente, taille confirmée : `8192` octets ;
- synthèse firmware : `layout=dual_ota sizes=ok ready=yes` ;
- heap libre au tout début du démarrage : `265080` octets ;
- minimum historique relevé à cet instant : `259508` octets ;
- plus grand bloc contigu : `110580` octets.

La valeur `size=203161` de la ligne `running` est considérée comme une troncature lors de la copie du log : la même partition `app0` est immédiatement relue à `2031616` octets, et le contrôle interne conclut `sizes=ok ready=yes`.

Ces valeurs mémoire sont nettement supérieures au diagnostic ntfy réalisé en fonctionnement établi, qui indiquait environ 74 KiB libres et un plus grand bloc contigu d’environ 39 KiB. Cela confirme que la fragmentation apparaît après l’initialisation et/ou au cours du fonctionnement, et devra être mesurée aux différents jalons d’OTA-1.

## Observation indépendante : scan I²C

Le même démarrage contient :

```text
[00:00:00.395] [INF] I2C: scan demarre
[00:00:00.414] [WRN] I2C: scan termine, 0 peripherique(s)
```

Cette observation n’invalide pas OTA-0.1, car elle est indépendante de la table de partitions. Elle doit toutefois être vérifiée avant de déclarer le fonctionnement matériel global validé, notamment si la carte relais XL9535 devait être alimentée et connectée pendant ce test.

## Critères de validation matérielle

Le run OTA-0.1 est validé sur matériel si :

1. les profils Legacy et V4 compilent ;
2. le module démarre normalement ;
3. le tactile reste fonctionnel ;
4. le portail captif et le serveur Web restent fonctionnels ;
5. le planificateur et les relais conservent leur comportement validé ;
6. les logs annoncent `app0` et `app1` présents ;
7. les deux tailles sont égales à `2031616` octets ;
8. `otadata` est présente avec une taille de `8192` octets ;
9. le résumé annonce `layout=dual_ota sizes=ok ready=yes` ;
10. les valeurs mémoire sont relevées pour servir de référence à OTA-1.

## Commandes de compilation

```powershell
git switch work/remote-ota-github
git pull
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

## Procédure de test

Flasher le profil nominal sur `COM3` :

```powershell
pio run -e ProgrammeArrosage -t upload --upload-port COM3
pio device monitor -p COM3 -b 115200
```

Copier les lignes commençant par :

```text
OTA diag:
```

Puis contrôler le fonctionnement habituel du module sans lancer de test HTTPS.

## État de validation

| Critère | État |
|---|---|
| Modification logicielle limitée à `SystemDiagnostics.cpp` | VALIDÉ |
| Aucun HTTPS ajouté | VALIDÉ |
| Aucune écriture OTA ajoutée | VALIDÉ |
| Compilation Legacy | VALIDÉ — binaire mesuré précédemment |
| Compilation V4 | VALIDÉ — binaire mesuré précédemment |
| Upload matériel sur COM3 | VALIDÉ |
| Table réelle du module | VALIDÉ |
| Deux partitions de 2031616 octets | VALIDÉ |
| `otadata` de 8192 octets | VALIDÉ |
| Résumé `ready=yes` | VALIDÉ |
| Mémoire au démarrage | VALIDÉ — 265080 / 259508 / 110580 octets |
| Tactile et portail captif | À CONFIRMER PAR TEST FONCTIONNEL |
| Relais / bus I²C | À VÉRIFIER — scan I²C à 0 périphérique |

## Suite autorisée

Le volet partitionnement et mémoire initiale d’OTA-0.1 est validé. OTA-1 peut préparer un test HTTPS isolé et non destructif, sans écriture dans `app1`.

Avant de clôturer complètement la validation fonctionnelle du firmware chargé, confirmer le tactile, l’accès Web et la situation du bus I²C. Le premier test OTA-1 devra instrumenter la mémoire avant et après initialisation complète afin d’expliquer la chute du plus grand bloc contigu observée lors du diagnostic ntfy.