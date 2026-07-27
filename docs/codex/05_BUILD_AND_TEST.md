# 05 — Compilation et tests

## Statut des profils firmware

- `ProgrammeArrosage_legacy` est la référence historique et le firmware de repli.
- `ProgrammeArrosage_v4` est le profil de migration à qualifier en priorité sur matériel.
- Une compilation V4 réussie ne constitue pas une validation fonctionnelle ou matérielle.
- Tant que le backend V4 n’est pas réellement câblé dans `main.cpp` et qu’aucune zone n’est migrée, un essai avec le profil V4 ne doit pas être présenté comme un test complet du chemin V4.
- Le profil `ProgrammeArrosage` reste un alias nominal historique pendant la phase de migration ; pour éviter toute ambiguïté, utiliser les noms explicites `_legacy` et `_v4` dans les procédures de développement.

## Chaîne obligatoire pour un nouveau code

### 1. Précontrôles Git

```powershell
git status
git diff --check
git diff --stat
```

### 2. Compilation de contrôle

Toute modification de code embarqué doit compiler dans les deux profils :

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4
```

Cette double compilation vérifie :

- que le nouveau chemin V4 reste constructible ;
- que les fichiers communs n’ont pas cassé le firmware Legacy ;
- que le retour arrière reste disponible.

Elle ne remplace pas les tests fonctionnels.

### 3. LittleFS lorsque les ressources embarquées changent

Le dépôt utilise `littlefs/` comme `data_dir` PlatformIO. Après toute modification de `littlefs/` :

```powershell
pio run -e ProgrammeArrosage_v4 -t buildfs
```

Avant un checkpoint ou une livraison nécessitant un repli Legacy vérifié :

```powershell
pio run -e ProgrammeArrosage_legacy -t buildfs
pio run -e ProgrammeArrosage_v4 -t buildfs
```

### 4. Firmware à charger pour les essais courants

Les nouveaux développements et les fonctions migrées doivent être essayés en priorité avec le profil V4 :

```powershell
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM9
pio device monitor -p COM9 -b 115200
```

Le profil Legacy n’est chargé que dans les cas suivants :

- comparaison d’un comportement douteux ;
- confirmation d’une régression ;
- retour temporaire à la référence stable ;
- campagne explicite de non-régression Legacy.

```powershell
pio run -e ProgrammeArrosage_legacy -t upload --upload-port COM9
pio device monitor -p COM9 -b 115200
```

### 5. Validation avant checkpoint ou livraison

```powershell
git diff --check
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4
```

Ajouter selon le périmètre :

```powershell
pio run -e ProgrammeArrosage_v4 -t buildfs
pio run -e test_execution_engine
pio run -e calibration
pio run -e test_relais
```

## Règle de qualification V4

Une fonction ne peut être déclarée « migrée V4 » ou « validée V4 » que si les quatre conditions suivantes sont réunies :

1. elle compile dans `ProgrammeArrosage_v4` ;
2. son chemin V4 est réellement instancié et appelé dans le runtime ;
3. elle a été testée sur la carte avec un effet observable ;
4. son résultat a été comparé au comportement Legacy ou à un résultat attendu documenté.

Pour chaque fonction testée, consigner au minimum :

- le profil flashé ;
- le fichier et la fonction concernés ;
- le point d’entrée exécuté ;
- la zone ou le matériel utilisé ;
- le résultat attendu ;
- le résultat observé ;
- les écarts et risques restants.

Si le backend V4 n’est pas encore câblé pour la fonction concernée, noter explicitement :

> Compilation V4 réussie, mais test fonctionnel V4 non applicable ou non représentatif à ce stade.

## Stratégie de test pendant la migration

- Les essais matériels courants se font en V4 dès que le chemin concerné est réellement actif.
- Legacy reste la référence de comparaison et la solution de repli.
- Les nouvelles évolutions importantes ne doivent pas s’accumuler tant que les fonctions V4 déjà intégrées n’ont pas été testées.
- Les tests commencent sur une seule zone, avec une durée courte et sous surveillance.
- Toute différence entre Legacy et V4 doit être qualifiée comme régression, correction volontaire ou évolution documentée.

## Matrice de validation minimale

| Type de changement | Legacy compile | V4 compile | buildfs | Test V4 sur carte | Comparaison Legacy |
|---|---:|---:|---:|---:|---:|
| C++ métier partagé | Oui | Oui | Si ressources touchées | Selon impact | Selon impact |
| Backend ou relais V4 | Oui | Oui | Non sauf UI | Obligatoire | Obligatoire |
| HTML/JS/CSS | Oui | Oui | Oui | Oui | Selon impact |
| Config NVS | Oui | Oui | Si ressources touchées | Oui | Obligatoire |
| TFT/touch | Oui | Oui | Si assets touchés | Obligatoire | Selon impact |
| Documentation seule | Non | Non | Non | Non | Non |

## Tests Web de non-régression

1. page principale ;
2. zones ;
3. planning ;
4. mode dense ;
5. modales ;
6. démarrage manuel ;
7. arrêt manuel ;
8. modification zone ;
9. sauvegarde créneaux ;
10. météo ;
11. info-bulles ;
12. paramètres utilisateur ;
13. verrouillage admin ;
14. mauvais mot de passe ;
15. bon mot de passe ;
16. persistance de session ;
17. reverrouillage ;
18. sauvegarde Wi-Fi avant reboot ;
19. logs ;
20. portail captif.

## Tests NVS

Boot sans NVS, sauvegarde, reboot, chargement, CRC invalide, taille invalide, reset, migration JSON et suppression JSON seulement après NVS valide.

## Tests planning

Heure non synchronisée, créneau actif/inactif, plusieurs zones, plusieurs créneaux, changement de jour, passage minuit, intervalle, pluie sous/au-dessus du seuil et durée maximale.

## Tests relais

Boot sûr, direct, inverse, XL9535, MCP23017 lorsque disponible, zone 1, dernière zone, arrêt manuel, timeout et reboot.

## Critères de livraison

Une livraison n’est pas valide sans :

- compilation `SUCCESS` des profils Legacy et V4 pour tout changement de firmware ;
- buildfs `SUCCESS` si `littlefs/` change ;
- diff contrôlé ;
- état Git explicite ;
- profil réellement flashé indiqué ;
- liste des tests matériels exécutés et non exécutés ;
- absence de déclaration « validé V4 » lorsque le chemin V4 n’est pas réellement actif ou testé.