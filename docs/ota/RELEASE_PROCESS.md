# Processus de release OTA AquaLook

## Objectif

Publier une release GitHub contenant des binaires Legacy et V4 construits depuis le même commit, accompagnés d'un manifeste vérifié compatible avec `aqualook-ota-manifest-v1`.

Le firmware consulte :

```text
https://github.com/cnuma/AquaLook/releases/latest/download/aqualook-manifest.json
```

La release utilisée par AquaLook doit donc être une release GitHub publiée et marquée comme dernière release, pas un brouillon ni une prerelease.

## Source unique de version

La version fonctionnelle se trouve exclusivement dans :

```text
VERSION
```

Format accepté :

```text
MAJEUR.MINEUR.CORRECTIF
```

Un suffixe éventuel est accepté pour les builds internes, mais une release servie par `/releases/latest/` doit rester une release publiée compatible avec la politique de canal utilisée.

`tools/version_build.py` lit ce fichier et injecte la même valeur dans les deux environnements PlatformIO.

## Assets publiés

Pour la version `5.9.0`, la release `v5.9.0` contient :

```text
AquaLook-legacy-5.9.0.bin
AquaLook-v4-5.9.0.bin
aqualook-manifest.json
SHA256SUMS.txt
```

Le manifeste contient pour chaque cible :

- la carte attendue ;
- l'environnement PlatformIO ;
- l'URL HTTPS GitHub exacte ;
- la taille exacte du binaire ;
- le SHA-256 exact du binaire.

## Déclenchement automatique

Le workflow :

```text
.github/workflows/ota-release.yml
```

se déclenche lors du push d'un tag au format :

```text
vMAJEUR.MINEUR.CORRECTIF
```

Le tag doit correspondre exactement à `v` suivi du contenu de `VERSION`.

Exemple :

```powershell
git switch agent/ota-2.3-core
git pull --ff-only
Get-Content VERSION

git tag -a v5.9.0 -m "AquaLook 5.9.0"
git push origin v5.9.0
```

## Étapes réalisées par GitHub Actions

1. récupérer le commit tagué avec l'historique Git complet ;
2. vérifier la correspondance entre le tag et `VERSION` ;
3. installer PlatformIO ;
4. compiler `ProgrammeArrosage` ;
5. compiler `ProgrammeArrosage_v4` ;
6. renommer les deux `firmware.bin` ;
7. générer le manifeste avec `tools/generate_ota_manifest.py` ;
8. calculer `SHA256SUMS.txt` ;
9. conserver tous les fichiers comme artefact du workflow ;
10. créer ou mettre à jour la release GitHub ;
11. marquer cette release comme dernière release.

## Contrôles bloquants

La génération échoue si :

- la version est invalide ;
- le tag ne correspond pas à la version ;
- un binaire est absent ou vide ;
- un binaire dépasse `2 031 616` octets ;
- une URL ne pointe pas vers GitHub en HTTPS ;
- le manifeste dépasse `8 192` octets.

## Premier test OTA-2.3

Le module actuellement installé en `5.8.0` doit détecter la release `5.9.0` comme mise à jour disponible.

Après publication :

1. ouvrir `/ota` ;
2. lancer **Vérifier la version disponible** ;
3. attendre le redémarrage maintenance puis le retour normal ;
4. contrôler :
   - version installée `5.8.0` ;
   - version disponible `5.9.0` ;
   - cible `v4` sur le module V4 ;
   - état `Mise à jour disponible` ;
   - icône LCD présente ;
   - notification mobile reçue une seule fois.

Aucun firmware ne doit être téléchargé ni installé à ce stade.

## Retour arrière

Ne jamais supprimer une release déjà utilisée par des modules sans publier immédiatement une release de remplacement valide.

Pour neutraliser une version problématique, publier une nouvelle version corrigée supérieure. Le firmware OTA-2.3 ne télécharge pas encore le binaire, mais cette règle prépare les futures étapes OTA-2.4 et suivantes.
