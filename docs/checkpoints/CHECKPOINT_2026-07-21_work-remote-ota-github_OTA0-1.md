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

Flasher le profil nominal :

```powershell
pio run -e ProgrammeArrosage -t upload
pio device monitor -b 115200
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
| Compilation Legacy | À EXÉCUTER LOCALEMENT |
| Compilation V4 | À EXÉCUTER LOCALEMENT |
| Upload matériel | À EXÉCUTER |
| Table réelle du module | À RELEVER |
| Mémoire au démarrage | À RELEVER |
| Tactile et portail captif | À REVALIDER |

## Suite autorisée

OTA-1 ne pourra commencer qu’après réception des logs matériels OTA-0.1 et validation du fonctionnement normal du programmateur.

La première étape OTA-1 restera un test HTTPS isolé et non destructif. Aucune écriture dans `app1` ne sera autorisée à ce stade.
